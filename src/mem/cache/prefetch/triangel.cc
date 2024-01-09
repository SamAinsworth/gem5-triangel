
/*
 * Copyright (c) 2023
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2005 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * Stride Prefetcher template instantiations.
 */
#include "mem/cache/prefetch/triangel.hh"

#include "debug/HWPrefetch.hh"
#include "mem/cache/prefetch/associative_set_impl.hh"
#include "params/TriangelPrefetcher.hh"
#include <cmath>

namespace gem5
{

GEM5_DEPRECATED_NAMESPACE(Prefetcher, prefetch);
namespace prefetch
{

Triangel::Triangel(
    const TriangelPrefetcherParams &p)
  : Queued(p),
    degree(p.degree),
    cachetags(p.cachetags),
    cacheDelay(p.cache_delay),
    should_lookahead(p.should_lookahead),
    should_rearrange(p.should_rearrange),
    owntags(p.owntags),
    max_size(p.address_map_actual_entries),
    size_increment(p.address_map_actual_entries/p.address_map_max_ways),
    global_timestamp(0),
    current_size(0),
    target_size(0),
    maxWays(p.address_map_max_ways),    
    bl(),
    bloomset(-1),
    way_idx(p.address_map_actual_entries/(p.address_map_max_ways*p.address_map_actual_cache_assoc),0),
    trainingUnit(p.training_unit_assoc, p.training_unit_entries,
                 p.training_unit_indexing_policy,
                 p.training_unit_replacement_policy),
    lookupAssoc(p.lookup_assoc),
    lookupOffset(p.lookup_offset),        
    setPrefetch(cachetags->getWayAllocationMax(),SatCounter8(8,128)),      
    useHawkeye(p.use_hawkeye),
    sampleUnit(p.sample_assoc,
    		  p.sample_entries,
    		  p.sample_indexing_policy,
    		  p.sample_replacement_policy),
    testUnit(p.test_assoc,
    		  p.test_entries,
    		  p.test_indexing_policy,
    		  p.test_replacement_policy),
    addressMappingCache(p.address_map_rounded_cache_assoc,
                          p.address_map_rounded_entries,
                          p.address_map_cache_indexing_policy,
                          p.address_map_cache_replacement_policy,
                          AddressMapping()),                   
    prefetchedCache(p.prefetched_cache_assoc,
                          p.prefetched_cache_entries,
                          p.prefetched_cache_indexing_policy,
                          p.prefetched_cache_replacement_policy,
                          AddressMapping()),
    lastAccessFromPFCache(false)
{
	assert(p.address_map_rounded_entries / p.address_map_rounded_cache_assoc == p.address_map_actual_entries / p.address_map_actual_cache_assoc);
	addressMappingCache.setWayAllocationMax(p.address_map_actual_cache_assoc);
	assert(cachetags->getWayAllocationMax()> maxWays+1);
	int bloom_size = p.address_map_actual_entries/128 < 1024? 1024: p.address_map_actual_entries/128;
	assert(bloom_init2(&bl,bloom_size, 0.01)==0);
	
	for(int x=0;x<64;x++) {
		hawksets[x].setMask = p.address_map_rounded_entries/ hawksets[x].maxElems - 1;
		hawksets[x].reset();
	}
	
	for(int x=0;x<64;x++) {
		sizeDuels[x].reset(size_increment/p.address_map_actual_cache_assoc - 1 ,p.address_map_actual_cache_assoc,cachetags->getWayAllocationMax());
	}
	for(int x=0;x<1024;x++) {
		lookupTable[x]=0;
    		lookupTick[x]=0;
	}	
}


bool
Triangel::randomChance(int reuseConf, int replaceRate) {
	replaceRate -=8;

	uint64_t baseChance = 1000000000l * sampleUnit.numEntries / addressMappingCache.numEntries;
	baseChance = replaceRate>0? (baseChance << replaceRate) : (baseChance >> (-replaceRate));
	baseChance = reuseConf < 3 ? baseChance / 16 : baseChance;
	uint64_t chance = random_mt.random<uint64_t>(0,1000000000ul);

	return baseChance >= chance;
}

void
Triangel::calculatePrefetch(const PrefetchInfo &pfi,
    std::vector<AddrPriority> &addresses)
{
    // This prefetcher requires a PC
    if (!pfi.hasPC()) {
        return;
    }

    bool is_secure = pfi.isSecure();
    Addr pc = pfi.getPC()>>2; //Shifted by 2 to help Arm indexing
    Addr addr = blockIndex(pfi.getAddr());

    // Looks up the last address at this PC
    TrainingUnitEntry *entry = trainingUnit.findEntry(pc, is_secure);
    bool correlated_addr_found = false;
    Addr index = 0;
    Addr target = 0;
    
    const int upperHistory=8;
    const int superHistory=14;
    
    const int upperReuse=8;
    
    //const int globalThreshold = 9;
    
    bool should_pf=false;
    bool should_hawk=false;
    bool should_pf_twice = false;
    bool should_sample = false;
    if (entry != nullptr) {
        trainingUnit.accessEntry(entry);
        correlated_addr_found = true;
        index = entry->lastAddress;

        if(addr == entry->lastAddress) return; // to avoid repeat trainings on sequence.
	if(entry->highHistoryConfidence >= superHistory) entry->currently_twodist_pf=true;
	if(entry->historyConfidence < upperHistory) entry->currently_twodist_pf=false; 
        //if very sure, index should be lastLastAddress. TODO: We could also try to learn timeliness here, by tracking PCs at the MSHRs.
        if(entry->currently_twodist_pf && should_lookahead) index = entry->lastLastAddress;
        target = addr;
        should_pf = entry->reuseConfidence > upperReuse && entry->historyConfidence > upperHistory; //8 is the reset point.

        if(entry->reuseConfidence > upperReuse) global_timestamp++;

    }

    if(entry==nullptr && randomChance(8,8)){ //only start adding samples for frequent entries.
	//TODO: should this instead just be tagless?
    	should_sample = true;
        entry = trainingUnit.findVictim(pc);
        DPRINTF(HWPrefetch, "Replacing Training Entry %x\n",pc);
        assert(entry != nullptr);
        assert(!entry->isValid());
        trainingUnit.insertEntry(pc, is_secure, entry);
        //if(globalHistoryConfidence>upperHistory) {while(entry->historyConfidence <= upperHistory) entry->historyConfidence++;}
        //if(globalReuseConfidence>upperReuse) {while(entry->reuseConfidence <= upperReuse) entry->reuseConfidence++;}
    }


    if(correlated_addr_found) {
    	//Check test unit for a recent history...
    	TestEntry* tentry = testUnit.findEntry(addr, is_secure);
    	if(tentry!= nullptr && !tentry->used) {
    		tentry->used=true;
    		entry->historyConfidence++;
    		entry->historyConfidence++;// unbias
    		entry->historyConfidence++;
    		
    		for(int x=0;x<6;x++)entry->highHistoryConfidence++;
    	}
    	
    	//Check sample table for entry.
    	SampleEntry *sentry = sampleUnit.findEntry(addr, is_secure);
    	if(sentry != nullptr && sentry->pc == pc) {
    		sentry->reused = true;
    		
    		int64_t distance = entry->local_timestamp - sentry->local_timestamp;
		if(distance > 0 && distance < max_size) { entry->reuseConfidence++; }else { entry->reuseConfidence--;}


     	    	DPRINTF(HWPrefetch, "Found reuse for addr %x, PC %x, distance %ld (train %ld vs sample %ld) confidence %d\n",addr, pc, distance, entry->local_timestamp, sentry->local_timestamp, entry->reuseConfidence+0);
    		
    		//TODO: What benefit do we get from checking both? Do we need to check last three?
    		
    		if(entry->lastAddress == sentry->last || entry->lastLastAddress == sentry->last || (owntags->findBlock(sentry->last<<lBlkSize,is_secure) && !owntags->findBlock(sentry->last<<lBlkSize,is_secure)->wasPrefetched())) {
    			entry->historyConfidence++;
    			entry->highHistoryConfidence++;
    			if(entry->replaceRate < 8) entry->replaceRate.reset();
    		}
    		
    		else {
    			entry->historyConfidence--;
    			entry->historyConfidence--;//bias
    			for(int x=0;x<5;x++)entry->highHistoryConfidence--;
			TestEntry* tentry = testUnit.findVictim(addr);
    			testUnit.insertEntry(sentry->last, is_secure, tentry);
    			tentry->pc = pc;
    			tentry->used = false;
    		}
    		
        	if(entry->lastAddress == sentry->last) DPRINTF(HWPrefetch, "Match for previous address %x confidence %d\n",entry->lastAddress, entry->historyConfidence+0);
    		if(entry->lastLastAddress == sentry->last) DPRINTF(HWPrefetch, "Match for previous previous address %x confidence %d\n",entry->lastLastAddress, entry->historyConfidence+0);
    	}
    	else if(should_sample || randomChance(entry->reuseConfidence,entry->replaceRate)) {
    		//Fill sample table
    		sentry = sampleUnit.findVictim(addr);
    		assert(sentry != nullptr);
    		if(sentry->pc !=0) {
    			    TrainingUnitEntry *pentry = trainingUnit.findEntry(sentry->pc, is_secure);
    			    if(pentry != nullptr) {
    			    	    int64_t distance = pentry->local_timestamp - sentry->local_timestamp;
    			    	    DPRINTF(HWPrefetch, "Replacing PC %x with PC %x, old distance %d\n",sentry->pc, pc, distance);
    			    	    if(distance > max_size) {
    			    	        //TODO: Change max size to be relative, based on current tracking set?
    			            	trainingUnit.accessEntry(pentry);
    			            	if(!sentry->reused) { 
    			            		pentry->reuseConfidence--;
    			            	}
    			            	entry->replaceRate++; //only replacing oldies -- can afford to be more aggressive.
    			            } else if(distance > 0) { //distance goes -ve due to lack of training-entry space
    			            	entry->replaceRate--;
    			            }
    			    } else entry->replaceRate++;
    		}
    		assert(!sentry->isValid());
    		sentry->clear();
    		sampleUnit.insertEntry(addr, is_secure, sentry);
    		sentry->pc = pc;
    		sentry->reused = false;
    		sentry->local_timestamp = entry->local_timestamp+1;
    		sentry->last = entry->lastAddress;
    	}
    }
    
    for(int x=0;x<64;x++) {
	int res =    	sizeDuels[x].checkAndInsert(addr,should_pf); //TODO: combine with hawk?
	if(res==0)continue;
	const int ratioNumer=3;
	const int ratioDenom=4;
	bool cache_hit = res > 0;
	if(!cache_hit) res = -res;
	res--;
	assert(res<setPrefetch.size() && res>=0);
	if(cache_hit) setPrefetch[setPrefetch.size()-1-res]--;
	else for(int y=0;y<(ratioNumer*sizeDuels[x].temporalModMax)/ratioDenom;y++) setPrefetch[res]++;
	
	//if(cache_hit) printf("Cache hit\n");
	//else printf("Prefetch hit\n");
    }
    
    int duelTargetSizeUpper = 0;
    //int duelTargetSizeHysteresis = 0;
    for(int x=0;x<setPrefetch.size();x++) {
    	if(setPrefetch[x]>192) duelTargetSizeUpper+= size_increment;
    	//if(setPrefetch[x]<128) break;
    }
    
    int curTargetSize = duelTargetSizeUpper > max_size? max_size:  duelTargetSizeUpper; //minimum
    
    if(curTargetSize > target_size) target_size = curTargetSize;
    //if(duelTargetSizeHysteresis <= current_size && duelTargetSizeHysteresis >= target_size) target_size = duelTargetSizeHysteresis;
    
    bool should_shrink = false;
    if(global_timestamp > 2000000) {
    	if(current_size > target_size) {
    		should_shrink=true;
    	}
    	//Reset after 2 million prefetch accesses -- not quite the same as after 30 million insts but close enough
     }
    
    if(target_size > current_size || should_shrink) {
    	current_size = target_size;
	printf("size: %d, tick %ld \n",current_size,curTick());
	assert(current_size >= 0);
	
	uint64_t newMask = 1;
	while(newMask < current_size) newMask=newMask<<1;
	for(int x=0;x<64;x++) {
		hawksets[x].setMask = newMask;
		hawksets[x].reset();
	}
	std::vector<AddressMapping> ams;
		     	if(should_rearrange) {		    	
				for(AddressMapping am: addressMappingCache) {
				    		if(am.isValid()) ams.push_back(am);
				}
				for(AddressMapping& am: addressMappingCache) {
			    		am.invalidate(); //for RRIP's sake
			    	}
		    	}
		    	TriangelHashedSetAssociative* thsa = dynamic_cast<TriangelHashedSetAssociative*>(addressMappingCache.indexingPolicy);
	  				if(thsa) { thsa->ways = current_size/size_increment; }
	  				else assert(0);
		    	//rearrange conditionally
		        if(should_rearrange) {        	
			    	if(current_size >0) {
				      	for(AddressMapping am: ams) {
				    		   AddressMapping *mapping = getHistoryEntry(am.index, am.isSecure(),true,false,true,true);
				    		   mapping->address = am.address;
				    		   mapping->index=am.index;
				    		   mapping->confident = am.confident;
				    		   mapping->lookupIndex=am.lookupIndex;
				    		   addressMappingCache.weightedAccessEntry(mapping,1,false); //For RRIP, touch
				    	}    	
			    	}
		    	}
		    	
		    	for(AddressMapping& am: addressMappingCache) {
			    if(thsa->ways==0 || (thsa->extractSet(am.index) % maxWays)>=thsa->ways)  am.invalidate();
			}
		    	cachetags->setWayAllocationMax(setPrefetch.size()-thsa->ways);  	
    } 

    if(global_timestamp > 2000000) {
    	target_size=0;
    	global_timestamp=0;
    	//Reset after 2 million prefetch accesses -- not quite the same as after 30 million insts but close enough
     }


    if(useHawkeye && correlated_addr_found && should_pf) {
        	for(int x=0; x<64; x++)hawksets[x].add(addr,pc,&trainingUnit);
        	should_hawk = entry->hawkConfidence>7;
    }
    

 /*   if(correlated_addr_found && should_pf) {
    	if(bloomset==-1) bloomset = index&127;
    	if((index&127)==bloomset) {
        	int add = bloom_add(&bl, &index, sizeof(Addr));
        	if(!add) target_size+=192;
        }

    }
    
    while(target_size > current_size
            		     && target_size > size_increment / 8 && current_size < max_size) {
            		        //check for size_increment to leave empty if unlikely to be useful.
            			current_size += size_increment;
            			printf("size: %d, tick %ld \n",current_size,curTick());
            			assert(current_size <= max_size);
            			assert(cachetags->getWayAllocationMax()>1);
            			cachetags->setWayAllocationMax(cachetags->getWayAllocationMax()-1);

            	std::vector<AddressMapping> ams;
                if(should_rearrange) {        	
		     	for(AddressMapping am: addressMappingCache) {
		    		if(am.isValid()) ams.push_back(am);
		    	}
			for(AddressMapping& am: addressMappingCache) {
		    		am.invalidate(); //for RRIP's sake
		    	}
            	}
            	TriangelHashedSetAssociative* thsa = dynamic_cast<TriangelHashedSetAssociative*>(addressMappingCache.indexingPolicy);
  		if(thsa) { thsa->ways++; thsa->max_ways = maxWays; assert(thsa->ways <= thsa->max_ways);}
  		else assert(0);
            	//TODO: rearrange conditionally
                if(should_rearrange) {        	            	
			for(AddressMapping am: ams) {
			    		   AddressMapping *mapping = getHistoryEntry(am.index, am.isSecure(),true,false, true, true);
			    		   mapping->address = am.address;
			    		   mapping->index=am.index;
			    		   mapping->confident = am.confident;
			    		   mapping->lookupIndex=am.lookupIndex;
			    		   addressMappingCache.weightedAccessEntry(mapping,1,false); //For RRIP, touch
			} 
		}  
            			//increase associativity of the set structure by 1!
            			//Also, decrease LLC cache associativity by 1.
    }

        if(global_timestamp > 2000000) {
    	//Reset after 2 million prefetch accesses -- not quite the same as after 30 million insts but close enough

    	while((target_size <= current_size - size_increment  || target_size < size_increment / 8)  && current_size >=size_increment) {
    		//reduce the assoc by 1.
    		//Also, increase LLC cache associativity by 1.
    		current_size -= size_increment;
    		printf("size: %d, tick %ld \n",current_size,curTick());
	    	assert(current_size >= 0);
	    	std::vector<AddressMapping> ams;
             	if(should_rearrange) {		    	
			for(AddressMapping am: addressMappingCache) {
			    		if(am.isValid()) ams.push_back(am);
			}
			for(AddressMapping& am: addressMappingCache) {
		    		am.invalidate(); //for RRIP's sake
		    	}
            	}
	    	TriangelHashedSetAssociative* thsa = dynamic_cast<TriangelHashedSetAssociative*>(addressMappingCache.indexingPolicy);
  				if(thsa) { assert(thsa->ways >0); thsa->ways--; }
  				else assert(0);
            	//rearrange conditionally
                if(should_rearrange) {        	
		    	if(current_size >0) {
			      	for(AddressMapping am: ams) {
			    		   AddressMapping *mapping = getHistoryEntry(am.index, am.isSecure(),true,false,true,true);
			    		   mapping->address = am.address;
			    		   mapping->index=am.index;
			    		   mapping->confident = am.confident;
			    		   mapping->lookupIndex=am.lookupIndex;
			    		   addressMappingCache.weightedAccessEntry(mapping,1,false); //For RRIP, touch
			    	}    	
		    	}
            	}
            	
            	for(AddressMapping& am: addressMappingCache) {
		    if(thsa->ways==0 || (thsa->extractSet(am.index) % maxWays)>=thsa->ways)  am.invalidate();
		}
            	
            	
            	

    		cachetags->setWayAllocationMax(cachetags->getWayAllocationMax()+1);
    	}
    	target_size = 0;
    	global_timestamp=0;
    	bloom_reset(&bl);
    	bloomset=-1;
    }

*/
    if (correlated_addr_found && should_pf && (current_size>0)) {
        // If a correlation was found, update the History table accordingly
	//DPRINTF(HWPrefetch, "Tabling correlation %x to %x, PC %x\n", index << lBlkSize, target << lBlkSize, pc);
	AddressMapping *mapping = getHistoryEntry(index, is_secure,false,false,false, should_hawk);
	if(mapping == nullptr) {
        	mapping = getHistoryEntry(index, is_secure,true,false,false, should_hawk);
        	mapping->address = target;
        	mapping->index=index; //for HawkEye
        	mapping->confident = entry->highHistoryConfidence > 12;
        }
        assert(mapping != nullptr);
        bool confident = mapping->address == target; 
        bool wasConfident = mapping->confident;
        mapping->confident = confident;
        if(!wasConfident) {
        	mapping->address = target;
        }
        if(wasConfident && confident) {
        	AddressMapping *cached_entry =
        		prefetchedCache.findEntry(index, is_secure);
        	if(cached_entry != nullptr) {
        		prefetchStats.metadataAccesses--;
        		//No need to access L3 again, as no updates to be done.
        	}
        }
        
        int index=0;
        uint64_t time = -1;
        if(lookupAssoc>0){
		int lookupMask = (1024/lookupAssoc)-1;
		int set = (target>>lookupOffset)&lookupMask;
		for(int x=lookupAssoc*set;x<lookupAssoc*(set+1);x++) {
			if(target>>lookupOffset == lookupTable[x]) {
				index=x;
				break;
			}
			if(time > lookupTick[x]) {
				time = lookupTick[x];
				index=x;
			}
		}
		
		lookupTable[index]=target>>lookupOffset;
		lookupTick[index]=curTick();
		mapping->lookupIndex=index;
        }
        
    }

    if(target != 0 && should_pf && (current_size>0)) {
  	 AddressMapping *pf_target = getHistoryEntry(target, is_secure,false,true,false, should_hawk);
   	 unsigned deg = 0;
  	 unsigned delay = cacheDelay;
  	 should_pf_twice = pf_target != nullptr
  	         && entry->highHistoryConfidence>superHistory /*&& pf_target->confident*/;
   	 unsigned max = should_pf_twice? degree : (should_pf? 1 : 0);
   	 //if(pf_target == nullptr && should_pf) DPRINTF(HWPrefetch, "Target not found for %x, PC %x\n", target << lBlkSize, pc);
   	 while (pf_target != nullptr && deg < max  /*&& (pf_target->confident || entry->highHistoryConfidence>upperHistory)*/
   	 ) { //TODO: do we always pf at distance 1 if not confident?
    		DPRINTF(HWPrefetch, "Prefetching %x on miss at %x, PC \n", pf_target->address << lBlkSize, addr << lBlkSize, pc);
    		int extraDelay = cacheDelay;
    		if(lastAccessFromPFCache) {
    			Cycles time = curCycle() - pf_target->cycle_issued;
    			if(time >= cacheDelay) extraDelay = 0;
    			else if (time < cacheDelay) extraDelay = time;
    		}
    		
    		Addr lookup = pf_target->address;
   	        if(lookupAssoc>0){
	   	 	int index=pf_target->lookupIndex;
	   	 	int lookupMask = (1<<lookupOffset)-1;
	   	 	lookup = (lookupTable[index]<<lookupOffset) + ((pf_target->address)&lookupMask);
	   	 	lookupTick[index]=curTick();
	   	 	if(lookup == pf_target->address)prefetchStats.lookupCorrect++;
	    		else prefetchStats.lookupWrong++;
    		}
    		
    		if(extraDelay == cacheDelay) addresses.push_back(AddrPriority(lookup << lBlkSize, delay));
    		delay += extraDelay;
    		//if(extraDelay < cacheDelay && should_pf_twice && max<4) max++;
    		deg++;
    		
    		if(deg<max /*&& pf_target->confident*/) pf_target = getHistoryEntry(lookup, is_secure,false,true,false, should_hawk);
    		else pf_target = nullptr;

   	 }
    }

        // Update the entry
    if(entry != nullptr) {
    	entry->lastLastAddress = entry->lastAddress;
    	entry->lastLastAddressSecure = entry->lastAddressSecure;
    	entry->lastAddress = addr;
    	entry->lastAddressSecure = is_secure;
    	entry->local_timestamp ++;
    }


}

Triangel::AddressMapping*
Triangel::getHistoryEntry(Addr paddr, bool is_secure, bool add, bool readonly, bool clearing, bool hawk)
{
  	    TriangelHashedSetAssociative* thsa = dynamic_cast<TriangelHashedSetAssociative*>(addressMappingCache.indexingPolicy);
	  				if(!thsa)  assert(0);  

    	cachetags->clearSetWay(thsa->extractSet(paddr)/maxWays, thsa->extractSet(paddr)%maxWays); 


    if(should_rearrange) {    

	    int index= paddr % (way_idx.size()); //Not quite the same indexing strategy, but close enough.
	    
	    if(way_idx[index] != thsa->ways) {
	    	if(way_idx[index] !=0) prefetchStats.metadataAccesses+= thsa->ways + way_idx[index];
	    	way_idx[index]=thsa->ways;
	    }
    }

    if(readonly) { //check the cache first.
        AddressMapping *pf_entry =
        	prefetchedCache.findEntry(paddr, is_secure);
        if (pf_entry != nullptr) {
        	lastAccessFromPFCache = true;
        	return pf_entry;
        }
        lastAccessFromPFCache = false;
    }

    AddressMapping *ps_entry =
        addressMappingCache.findEntry(paddr, is_secure);
    if(readonly || !add) prefetchStats.metadataAccesses++;
    if (ps_entry != nullptr) {
        // A PS-AMC line already exists
        addressMappingCache.weightedAccessEntry(ps_entry,hawk?1:0,false);
    } else {
        if(!add) return nullptr;
        ps_entry = addressMappingCache.findVictim(paddr);
        assert(ps_entry != nullptr);
        if(useHawkeye && !clearing) for(int x=0;x<64;x++) hawksets[x].decrementOnLRU(ps_entry->index,&trainingUnit);
	assert(!ps_entry->isValid());
        addressMappingCache.insertEntry(paddr, is_secure, ps_entry);
        addressMappingCache.weightedAccessEntry(ps_entry,hawk?1:0,true);
    }

    if(readonly) {
    	    AddressMapping *pf_entry = prefetchedCache.findVictim(paddr);
    	    prefetchedCache.insertEntry(paddr, is_secure, pf_entry);
    	    pf_entry->address = ps_entry->address;
    	    pf_entry->confident = ps_entry->confident;
    	    pf_entry->cycle_issued = curCycle();
    	    //This adds access time, to set delay appropriately.
    }

    return ps_entry;
}




uint32_t
TriangelHashedSetAssociative::extractSet(const Addr addr) const
{
	//Input is already blockIndex so no need to remove block again.
    Addr offset = addr;
    
   /* const Addr hash1 = offset & ((1<<16)-1);
    const Addr hash2 = (offset >> 16) & ((1<<16)-1);
        const Addr hash3 = (offset >> 32) & ((1<<16)-1);
    */
        offset = ((offset) * max_ways) + (extractTag(addr) % ways);
        return offset & setMask;   //setMask is numSets-1

}


Addr
TriangelHashedSetAssociative::extractTag(const Addr addr) const
{
    //Input is already blockIndex so no need to remove block again.

    //Description in Triage-ISR confuses whether the index is just the 16 least significant bits,
    //or the weird index above. The tag can't be the remaining bits if we use the literal representation!


    Addr offset = addr / (numSets/max_ways); 
    int result = 0;
    
    const int shiftwidth=10;

    for(int x=0; x<64; x+=shiftwidth) {
       result ^= (offset & ((1<<shiftwidth)-1));
       offset = offset >> shiftwidth;
    }
    return result;
}



} // namespace prefetch
} // namespace gem5
