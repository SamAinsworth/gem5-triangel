
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
#include "mem/cache/prefetch/simpletriangel.hh"

#include "debug/HWPrefetch.hh"
#include "mem/cache/prefetch/associative_set_impl.hh"
#include "params/SimpleTriangelPrefetcher.hh"
#include <cmath>


namespace gem5
{

GEM5_DEPRECATED_NAMESPACE(Prefetcher, prefetch);
namespace prefetch
{

int SimpleTriangel::target_size=0;
int SimpleTriangel::current_size=0;
int64_t SimpleTriangel::global_timestamp=0;
AssociativeSet<SimpleTriangel::MarkovMapping>* SimpleTriangel::markovTablePtr=NULL;
std::vector<uint32_t> SimpleTriangel::setPrefetch(17,0);
SimpleTriangel::SizeDuel* SimpleTriangel::sizeDuelPtr=nullptr;

SimpleTriangel::SimpleTriangel(
    const SimpleTriangelPrefetcherParams &p)
  : Queued(p),
    degree(p.degree),
    cachetags(p.cachetags),
    cacheDelay(p.cache_delay),
    should_lookahead(p.should_lookahead),
    should_rearrange(p.should_rearrange),
    use_scs(p.use_scs),
    sctags(p.sctags),
    max_size(p.address_map_actual_entries),
    size_increment(p.address_map_actual_entries/p.address_map_max_ways),
    second_chance_timestamp(0),
    maxWays(p.address_map_max_ways),    
    way_idx(p.address_map_actual_entries/(p.address_map_max_ways*p.address_map_actual_cache_assoc),0),
    globalReuseConfidence(7,64),
    globalPatternConfidence(7,64),
    globalHighPatternConfidence(7,64),
    trainingUnit(p.training_unit_assoc, p.training_unit_entries,
                 p.training_unit_indexing_policy,
                 p.training_unit_replacement_policy),     
    historySampler(p.sample_assoc,
    		  p.sample_entries,
    		  p.sample_indexing_policy,
    		  p.sample_replacement_policy),
    secondChanceUnit(p.secondchance_assoc,
    		  p.secondchance_entries,
    		  p.secondchance_indexing_policy,
    		  p.secondchance_replacement_policy),
    markovTable(p.address_map_rounded_cache_assoc,
                          p.address_map_rounded_entries,
                          p.address_map_cache_indexing_policy,
                          p.address_map_cache_replacement_policy,
                          MarkovMapping()),                   
    metadataReuseBuffer(p.metadata_reuse_assoc,
                          p.metadata_reuse_entries,
                          p.metadata_reuse_indexing_policy,
                          p.metadata_reuse_replacement_policy,
                          MarkovMapping()),
    lastAccessFromPFCache(false)
{	
	markovTablePtr = &markovTable;

	setPrefetch.resize(cachetags->getWayAllocationMax()+1,0);
	assert(p.address_map_rounded_entries / p.address_map_rounded_cache_assoc == p.address_map_actual_entries / p.address_map_actual_cache_assoc);
	markovTable.setWayAllocationMax(p.address_map_actual_cache_assoc);
	assert(cachetags->getWayAllocationMax()> maxWays);

		sizeDuelPtr= sizeDuels;	
	for(int x=0;x<64;x++) {
		sizeDuelPtr[x].reset(size_increment/p.address_map_actual_cache_assoc - 1 ,p.address_map_actual_cache_assoc,cachetags->getWayAllocationMax());
	}

	
	current_size = 0;
	target_size=0;

}


bool
SimpleTriangel::randomChance(int reuseConf, int replaceRate) {
	replaceRate -=8;

	uint64_t baseChance = 1000000000l * historySampler.numEntries / markovTable.numEntries;
	baseChance = replaceRate>0? (baseChance << replaceRate) : (baseChance >> (-replaceRate));
	baseChance = reuseConf < 3 ? baseChance / 16 : baseChance;
	uint64_t chance = random_mt.random<uint64_t>(0,1000000000ul);

	return baseChance >= chance;
}

void
SimpleTriangel::calculatePrefetch(const PrefetchInfo &pfi,
    std::vector<AddrPriority> &addresses)
{

    Addr addr = blockIndex(pfi.getAddr());
    second_chance_timestamp++;
    
    // This prefetcher requires a PC
    if (!pfi.hasPC() || pfi.isWrite()) {
	//To update the set dueller with elements int the L3 that are accessed but nothing to do with the prefetcher.
	    for(int x=0;x<64;x++) {
		int res =    	sizeDuelPtr[x].checkAndInsert(addr,false);
		if(res==0)continue;
		int cache_hit = res%128; //Because we encode cache hits as the first 7 bits of this result here.
		int cache_set = cache_hit-1;
		assert(!cache_hit || (cache_set<setPrefetch.size()-1 && cache_set>=0));
		if(cache_hit) for(int y= setPrefetch.size()-2-cache_set; y>=0; y--) setPrefetch[y]++; 
		// cache partition hit at this size or bigger. So hit in way 14 = y=17-2-14=1 and 0: would hit with 0 ways reserved or 1, not 2.

	    }
        return;
    }

    bool is_secure = pfi.isSecure();
    Addr pc = pfi.getPC()>>2; //Shifted by 2 to help Arm indexing. Bit fake; really should xor in these bits with upper bits.


    // Looks up the last address at this PC
    TrainingUnitEntry *entry = trainingUnit.findEntry(pc, is_secure);
    bool correlated_addr_found = false;
    Addr index = 0;
    Addr target = 0;
    
    const int upperHistory=globalPatternConfidence>64?7:8;
    const int highUpperHistory=globalHighPatternConfidence>64?7:8;
    const int superHistory=14;
    
    const int upperReuse=globalReuseConfidence>64?7:8;
    
    //const int globalThreshold = 9;
    
    bool should_pf=false;
    bool should_sample = false;
    if (entry != nullptr) { //This accesses the training table at this PC.
        trainingUnit.accessEntry(entry);
        correlated_addr_found = true;
        index = entry->lastAddress;

        if(addr == entry->lastAddress) return; // to avoid repeat trainings on sequence.
	if(entry->highPatternConfidence >= superHistory) entry->currently_twodist_pf=true;
	if(entry->patternConfidence < upperHistory) entry->currently_twodist_pf=false; 
        //if very sure, index should be lastLastAddress. TODO: We could also try to learn timeliness here, by tracking PCs at the MSHRs.
        if(entry->currently_twodist_pf && should_lookahead) index = entry->lastLastAddress;
        target = addr;
        should_pf = (entry->reuseConfidence > upperReuse) && (entry->patternConfidence > upperHistory); //8 is the reset point.

        if(entry->reuseConfidence > upperReuse) global_timestamp++;

    }

    if(entry==nullptr && (randomChance(8,8) || ((globalReuseConfidence > 64) && (globalHighPatternConfidence > 64) && (globalPatternConfidence > 64)))){ //only start adding samples for frequent entries.
	//TODO: should this instead just be tagless?
    	if(!((globalReuseConfidence > 64) && (globalHighPatternConfidence > 64)  && (globalPatternConfidence > 64)))should_sample = true;
        entry = trainingUnit.findVictim(pc);
        DPRINTF(HWPrefetch, "Replacing Training Entry %x\n",pc);
        assert(entry != nullptr);
        assert(!entry->isValid());
        trainingUnit.insertEntry(pc, is_secure, entry);
        if(globalHighPatternConfidence>96) entry->currently_twodist_pf=true;
    }


    if(correlated_addr_found) {
    	//Check second-chance sampler for a recent history. If so, update pattern confidence accordingly.
    	SecondChanceEntry* tentry = secondChanceUnit.findEntry(addr, is_secure);
    	if(tentry!= nullptr && !tentry->used) {
    		tentry->used=true;
    		TrainingUnitEntry *pentry = trainingUnit.findEntry(tentry->pc, is_secure);
    		 if(((tentry->global_timestamp + 512 > second_chance_timestamp)) && pentry != nullptr) {  
  			if(tentry->pc==pc) {
	  			pentry->patternConfidence++;
	    			pentry->highPatternConfidence++;
	    			globalPatternConfidence++;
	    			globalHighPatternConfidence++;
    			}
		} else if(pentry!=nullptr) {
				    			pentry->patternConfidence--;
			    			globalPatternConfidence--;
			    			pentry->patternConfidence--; globalPatternConfidence--; //bias 
			    			for(int x=0;x<5;x++) {pentry->highPatternConfidence--;  globalHighPatternConfidence--;}
		}
    	}
    	
    	//Check history sampler for entry.
    	SampleEntry *sentry = historySampler.findEntry(entry->lastAddress, is_secure);
    	if(sentry != nullptr && sentry->entry == entry) {    		
    		
		int64_t distance = sentry->entry->local_timestamp - sentry->local_timestamp;
		if(distance > 0 && distance < max_size) { entry->reuseConfidence++;globalReuseConfidence++;}
		else if(!sentry->reused){ entry->reuseConfidence--;globalReuseConfidence--;}
    		sentry->reused = true;

     	    	DPRINTF(HWPrefetch, "Found reuse for addr %x, PC %x, distance %ld (train %ld vs sample %ld) confidence %d\n",addr, pc, distance, entry->local_timestamp, sentry->local_timestamp, entry->reuseConfidence+0);
     	    	
    		
    		if(addr == sentry->next ||  (use_scs && sctags->findBlock(sentry->next<<lBlkSize,is_secure) && !sctags->findBlock(sentry->next<<lBlkSize,is_secure)->wasPrefetched())) {
    			if(addr == sentry->next) {
    				entry->patternConfidence++;
    				entry->highPatternConfidence++;
    				globalPatternConfidence++;
	    			globalHighPatternConfidence++;
    			}
    			//if(entry->replaceRate < 8) entry->replaceRate.reset();
    		}
    		
    		else {
    			//We haven't spotted the (x,y) pattern we expect, on seeing y. So put x in the SCS.
			if(use_scs) {
				SecondChanceEntry* tentry = secondChanceUnit.findVictim(addr);
				if(tentry->pc !=0 && !tentry->used) {
    			   		TrainingUnitEntry *pentry = trainingUnit.findEntry(tentry->pc, is_secure);	
    			   		if(pentry != nullptr) {
			    			pentry->patternConfidence--;
			    			globalPatternConfidence--;
			    			pentry->patternConfidence--; globalPatternConfidence--; //bias 
			    			for(int x=0;x<5;x++) {pentry->highPatternConfidence--;  globalHighPatternConfidence--;  	 }		   		
    			   		}			
				}
	    			secondChanceUnit.insertEntry(sentry->next, is_secure, tentry);
	    			tentry->pc = pc;
	    			tentry->global_timestamp = second_chance_timestamp;
	    			tentry->used = false;
    			} else {
    				entry->patternConfidence--;
    				globalPatternConfidence--;
			    	 entry->patternConfidence--; globalPatternConfidence--;
			    	for(int x=0;x<5;x++) {entry->highPatternConfidence--;  globalHighPatternConfidence--;  	 }	 			   		
    			}
    		}
    		
        	if(addr == sentry->next) DPRINTF(HWPrefetch, "Match for address %x confidence %d\n",addr, entry->patternConfidence+0);
    		     	    	if(sentry->entry == entry) sentry->next=addr;
    	}
    	else if(should_sample || randomChance(entry->reuseConfidence,entry->replaceRate)) {
    		//Fill sample table, as we're taking a sample at this PC. Should_sample also set by randomChance earlier, on first insert of PC intro training table.
    		sentry = historySampler.findVictim(entry->lastAddress);
    		assert(sentry != nullptr);
    		if(sentry->entry !=nullptr) {
    			    TrainingUnitEntry *pentry = sentry->entry;
    			    if(pentry != nullptr) {
    			    	    int64_t distance = pentry->local_timestamp - sentry->local_timestamp;
    			    	    DPRINTF(HWPrefetch, "Replacing Entry %x with PC %x, old distance %d\n",sentry->entry, pc, distance);
    			    	    if(distance > max_size) {
    			    	        //TODO: Change max size to be relative, based on current tracking set?
    			            	trainingUnit.accessEntry(pentry);
    			            	if(!sentry->reused) { 
    			            	    	//Reuse conf decremented, as very old.
    			            		pentry->reuseConfidence--;
						globalReuseConfidence--;
    			            	}
    			            	entry->replaceRate++; //only replacing oldies -- can afford to be more aggressive.
    			            } else if(distance > 0 && !sentry->reused) { //distance goes -ve due to lack of training-entry space
    			            	entry->replaceRate--;
    			            }
    			    } else entry->replaceRate++;
    		}
    		assert(!sentry->isValid());
    		sentry->clear();
    		historySampler.insertEntry(entry->lastAddress, is_secure, sentry);
    		sentry->entry = entry;
    		sentry->reused = false;
    		sentry->local_timestamp = entry->local_timestamp+1;
    		sentry->next = addr;
    		sentry->confident=false;
    	}
    }
    
	    
	    for(int x=0;x<64;x++) {
	    	//Here we update the size duellers, to work out for each cache set whether it is better to be markov table or L3 cache.
		int res =    	sizeDuelPtr[x].checkAndInsert(addr,should_pf);
		if(res==0)continue;
		const int ratioNumer=2;
		const int ratioDenom=4;//should_pf && entry->highHistoryConfidence >=upperHistory? 4 : 8;
		int cache_hit = res%128; //This is just bit encoding of cache hits.
		int pref_hit = res/128; //This is just bit encoding of prefetch hits.
		int cache_set = cache_hit-1; //Encodes which nth most used replacement-state we hit at, if any.
		int pref_set = pref_hit-1; //Encodes which nth most used replacement-state we hit at, if any.
		assert(!cache_hit || (cache_set<setPrefetch.size()-1 && cache_set>=0));
		assert(!pref_hit || (pref_set<setPrefetch.size()-1 && pref_set>=0));
		if(cache_hit) for(int y= setPrefetch.size()-2-cache_set; y>=0; y--) setPrefetch[y]++; 
		// cache partition hit at this size or bigger. So hit in way 14 = y=17-2-14=1 and 0: would hit with 0 ways reserved or 1, not 2.
		if(pref_hit)for(int y=pref_set+1;y<setPrefetch.size();y++) setPrefetch[y]+=(ratioNumer*sizeDuelPtr[x].temporalModMax)/ratioDenom; 
		// ^ pf hit at this size or bigger. one-indexed (since 0 is an alloc on 0 ways). So hit in way 0 = y=1--16 ways reserved, not 0.
		
	    }
	    

	    if(global_timestamp > 500000) {
	    //Here we choose the size of the Markov table based on the optimum for the last epoch
	    int counterSizeSeen = 0;

	    for(int x=0;x<setPrefetch.size() && x*size_increment <= max_size;x++) {
	    	if(setPrefetch[x]>counterSizeSeen) {
	    		 target_size= size_increment*x;
	    		 counterSizeSeen = setPrefetch[x];
	    	}
	    }

	    int currentscore = setPrefetch[current_size/size_increment];
	    currentscore = currentscore + (currentscore>>4); //Slight bias against changing for minimal benefit.
	    int targetscore = setPrefetch[target_size/size_increment];

	    if(target_size != current_size && targetscore>currentscore) {
	    	current_size = target_size;
		printf("size: %d, tick %ld \n",current_size,curTick());

		assert(current_size >= 0);
		

		std::vector<MarkovMapping> ams;
			     	if(should_rearrange) {		    	
					for(MarkovMapping am: *markovTablePtr) {
					    		if(am.isValid()) ams.push_back(am);
					}
					for(MarkovMapping& am: *markovTablePtr) {
				    		am.invalidate(); //for RRIP's sake
				    	}
			    	}
			    	SimpleTriangelHashedSetAssociative* thsa = dynamic_cast<SimpleTriangelHashedSetAssociative*>(markovTablePtr->indexingPolicy);
		  				if(thsa) { thsa->ways = current_size/size_increment; thsa->max_ways = maxWays; assert(thsa->ways <= thsa->max_ways);}
		  				else assert(0);
			    	//rearrange conditionally
				if(should_rearrange) {        	
				    	if(current_size >0) {
					      	for(MarkovMapping am: ams) {
					    		   MarkovMapping *mapping = getHistoryEntry(am.index, am.isSecure(),true,false,true,true);
					    		   mapping->address = am.address;
					    		   mapping->index=am.index;
					    		   mapping->confident = am.confident;
					    		   markovTablePtr->weightedAccessEntry(mapping,1,false); //For RRIP, touch
					    	}    	
				    	}
			    	}
			    	
			    	for(MarkovMapping& am: *markovTablePtr) {
				    if(thsa->ways==0 || (thsa->extractSet(am.index) % maxWays)>=thsa->ways)  am.invalidate();
				}
			    	cachetags->setWayAllocationMax(setPrefetch.size()-1-thsa->ways);  	
	    } 

	    	global_timestamp=0;
		for(int x=0;x<setPrefetch.size();x++) {
		    	setPrefetch[x]=0;
		}

	     }
	         
    
    if (correlated_addr_found && should_pf && (current_size>0)) {
        // If a correlation was found, update the Markov table accordingly
	//DPRINTF(HWPrefetch, "Tabling correlation %x to %x, PC %x\n", index << lBlkSize, target << lBlkSize, pc);
	MarkovMapping *mapping = getHistoryEntry(index, is_secure,false,false,false, false);
	if(mapping == nullptr) {
        	mapping = getHistoryEntry(index, is_secure,true,false,false, false);
        	mapping->address = target;
        	mapping->index=index; //for HawkEye
        	mapping->confident = false;
        }
        assert(mapping != nullptr);
        bool confident = mapping->address == target; 
        bool wasConfident = mapping->confident;
        mapping->confident = confident; //Confidence is just used for replacement. I haven't tested how important it is for performance to use it; this is inherited from Triage.
        if(!wasConfident) {
        	mapping->address = target;
        }
        if(wasConfident && confident) {
        	MarkovMapping *cached_entry =
        		metadataReuseBuffer.findEntry(index, is_secure);
        	if(cached_entry != nullptr) {
        		prefetchStats.metadataAccesses--;
        		//No need to access L3 again, as no updates to be done.
        	}
        }
        
        
    }

    if(target != 0 && should_pf && (current_size>0)) {
  	 MarkovMapping *pf_target = getHistoryEntry(target, is_secure,false,true,false, false);
   	 unsigned deg = 0;
  	 unsigned delay = cacheDelay;
  	 bool high_degree_pf = pf_target != nullptr
  	         && (entry->highPatternConfidence>highUpperHistory );
   	 unsigned max = high_degree_pf? degree : (should_pf? 1 : 0);

   	 while (pf_target != nullptr && deg < max) 
   	 { 
    		DPRINTF(HWPrefetch, "Prefetching %x on miss at %x, PC \n", pf_target->address << lBlkSize, addr << lBlkSize, pc);
    		int extraDelay = cacheDelay;
    		if(lastAccessFromPFCache) {
    			Cycles time = curCycle() - pf_target->cycle_issued;
    			if(time >= cacheDelay) extraDelay = 0;
    			else if (time < cacheDelay) extraDelay = time;
    		}
    		
    		Addr lookup = pf_target->address;

    		
    		if(extraDelay == cacheDelay) addresses.push_back(AddrPriority(lookup << lBlkSize, delay));
    		delay += extraDelay; // if cached, less delay. Not always none: might be a partially complete access still to arrive from L3.
    		deg++;
    		
    		if(deg<max) pf_target = getHistoryEntry(lookup, is_secure,false,true,false, false);
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

SimpleTriangel::MarkovMapping*
SimpleTriangel::getHistoryEntry(Addr paddr, bool is_secure, bool add, bool readonly, bool clearing, bool hawk)
{
	//The weird parameters above control whether we replace entries, and how the number of metadata accesses are updated, for instance. They're basically a simulation thing.
  	    SimpleTriangelHashedSetAssociative* thsa = dynamic_cast<SimpleTriangelHashedSetAssociative*>(markovTablePtr->indexingPolicy);
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
        MarkovMapping *pf_entry =
        	 metadataReuseBuffer.findEntry(paddr, is_secure);
        if (pf_entry != nullptr) {
        	lastAccessFromPFCache = true;
        	return pf_entry;
        }
        lastAccessFromPFCache = false;
    }

    MarkovMapping *ps_entry =
        markovTablePtr->findEntry(paddr, is_secure);
    if(readonly || !add) prefetchStats.metadataAccesses++;
    if (ps_entry != nullptr) {
        // A PS-AMC line already exists
        markovTablePtr->weightedAccessEntry(ps_entry,0,false); //This is only complicated because it behaves differently to update state for LRU vs RRIP vs HawkEye.
    } else {
        if(!add) return nullptr;
        ps_entry = markovTablePtr->findVictim(paddr);
        assert(ps_entry != nullptr);
	assert(!ps_entry->isValid());
        markovTablePtr->insertEntry(paddr, is_secure, ps_entry);
        markovTablePtr->weightedAccessEntry(ps_entry,0,true);
    }

    if(readonly) {
    	    MarkovMapping *pf_entry = metadataReuseBuffer.findVictim(paddr);
    	    metadataReuseBuffer.insertEntry(paddr, is_secure, pf_entry);
    	    pf_entry->address = ps_entry->address;
    	    pf_entry->confident = ps_entry->confident;
    	    pf_entry->cycle_issued = curCycle();
    	    //This adds access time, to set delay appropriately.
    }

    return ps_entry;
}




uint32_t
SimpleTriangelHashedSetAssociative::extractSet(const Addr addr) const
{
	//Input is already blockIndex so no need to remove block again.
    Addr offset = addr;
    
        offset = ((offset) * max_ways) + (extractTag(addr) % ways);
        return offset & setMask;   //setMask is numSets-1

}


Addr
SimpleTriangelHashedSetAssociative::extractTag(const Addr addr) const
{
    //Input is already blockIndex so no need to remove block again.

    //Description in Triage-ISR confuses whether the index is just the 16 least significant bits,
    //or the weird index above. The tag can't be the remaining bits if we use the literal representation!


    Addr offset = addr / (numSets/max_ways); //Remove the index bits first. Not clear how important, but it seemed helpful experimentally.
    int result = 0;
    
    //This is a tag# as described in the Triangel paper.
    const int shiftwidth=10;

    for(int x=0; x<64; x+=shiftwidth) {
       result ^= (offset & ((1<<shiftwidth)-1));
       offset = offset >> shiftwidth;
    }
    return result;
}



} // namespace prefetch
} // namespace gem5
