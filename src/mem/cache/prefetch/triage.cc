
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
 * Triage prefetcher.
 */
#include "mem/cache/prefetch/triage.hh"

#include "debug/HWPrefetch.hh"
#include "mem/cache/prefetch/associative_set_impl.hh"
#include "params/TriagePrefetcher.hh"
#include <cmath>

namespace gem5
{

GEM5_DEPRECATED_NAMESPACE(Prefetcher, prefetch);
namespace prefetch
{

Triage::Triage(
    const TriagePrefetcherParams &p)
  : Queued(p),
    degree(p.degree),
    cachetags(p.cachetags),
    cacheDelay(p.cache_delay),
    should_rearrange(p.should_rearrange),    
    store_unreliable(p.store_unreliable),
    max_size(p.address_map_actual_entries),
    size_increment(p.address_map_actual_entries/p.address_map_max_ways),
    global_timestamp(0),
    current_size(0),
    target_size(0),
    maxWays(p.address_map_max_ways),
    hawkeyeThreshold(p.hawkeye_threshold),
    bl(),
    way_idx(p.address_map_actual_entries/(p.address_map_max_ways*p.address_map_actual_cache_assoc),0),
    trainingUnit(p.training_unit_assoc, p.training_unit_entries,
                 p.training_unit_indexing_policy,
                 p.training_unit_replacement_policy),
    lookupAssoc(p.lookup_assoc),
    lookupOffset(p.lookup_offset),
    addressMappingCache(p.address_map_rounded_cache_assoc,
                          p.address_map_rounded_entries,
                          p.address_map_cache_indexing_policy,
                          p.address_map_cache_replacement_policy,
                          AddressMapping())
{
	for(int x=0;x<64;x++) {
		hawksets[x].setMask = p.address_map_rounded_entries/ hawksets[x].maxElems - 1;
		hawksets[x].reset();
	}
	for(int x=0;x<1024;x++) {
		lookupTable[x]=0;
    		lookupTick[x]=0;
	}

	assert(p.address_map_rounded_entries / p.address_map_rounded_cache_assoc == p.address_map_actual_entries / p.address_map_actual_cache_assoc);
	addressMappingCache.setWayAllocationMax(p.address_map_actual_cache_assoc);
	assert(cachetags->getWayAllocationMax()> maxWays+1);

	bloom_init2(&bl,p.address_map_actual_entries, 0.01);
}


void
Triage::calculatePrefetch(const PrefetchInfo &pfi,
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


    bool temporal = true;


    if (entry != nullptr) {
        trainingUnit.accessEntry(entry);
        correlated_addr_found = true;
        index = entry->lastAddress;
            
    	for(int x=0; x<64; x++)hawksets[x].add(addr,pc,&trainingUnit);
        temporal = entry->temporal>=hawkeyeThreshold;

        if(addr == entry->lastAddress) return; // to avoid repeat trainings on sequence.

        target = addr;

    } else {
        entry = trainingUnit.findVictim(pc);
        DPRINTF(HWPrefetch, "Replacing Training Entry %x\n",pc);
        assert(entry != nullptr);
        assert(!entry->isValid());
        trainingUnit.insertEntry(pc, is_secure, entry);
    }




    if(correlated_addr_found /*&& (temporal || store_unreliable)*/) {
        global_timestamp++;
        int add = bloom_add(&bl, &addr, sizeof(Addr));
        if(!add) target_size++;
   	while(target_size > current_size
            		     && current_size < max_size) {
            	current_size += size_increment;
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
            	
            	TriageHashedSetAssociative* thsa = dynamic_cast<TriageHashedSetAssociative*>(addressMappingCache.indexingPolicy);
  		if(thsa) { thsa->ways++; thsa->max_ways = maxWays; assert(thsa->ways <= thsa->max_ways);}
  		else assert(0);
            	if(should_rearrange) {
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

        //TODO: add error expansion. Not that major though -- our bloom filter is big and this is a ceiling function, so who cares?
    }

    if(global_timestamp > 2000000) {
    	//Reset after 2 million prefetch accesses -- not quite the same as after 30 million insts but close enough

    	while(target_size <= current_size - size_increment && current_size >=size_increment) {
    		//reduce the assoc by 1.
    		//Also, increase LLC cache associativity by 1.
    		current_size -= size_increment;
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
	    	TriageHashedSetAssociative* thsa = dynamic_cast<TriageHashedSetAssociative*>(addressMappingCache.indexingPolicy);
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
    }
    
    if(!temporal && !store_unreliable) return;

    if (correlated_addr_found && (current_size>0)) {
        // If a correlation was found, update the History table accordingly
	//DPRINTF(HWPrefetch, "Tabling correlation %x to %x, PC %x\n", index << lBlkSize, target << lBlkSize, pc);
	AddressMapping *mapping = getHistoryEntry(index, is_secure,false,false,temporal, false);
	if(mapping == nullptr) {
        	mapping = getHistoryEntry(index, is_secure,true,false,temporal, false);
        	mapping->address = target;
        	mapping->index=index; //for HawkEye
        	mapping->confident = false;
        }
        assert(mapping != nullptr);
        bool confident = mapping->address == target;
        bool wasConfident = mapping->confident;
        mapping->confident = confident;
        if(!wasConfident) {
        	mapping->address = target;
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

    if(target != 0 && (current_size>0)) {
  	 AddressMapping *pf_target = getHistoryEntry(target, is_secure,false,true,temporal, false);
   	 unsigned deg = 0;
  	 unsigned delay = cacheDelay;
     	 unsigned max = degree;
   	 while (pf_target != nullptr && deg < max) //TODO: and confident? not clear from paper. public implementation suggests no
   	 {
   	 	Addr lookup = pf_target->address;
   	        if(lookupAssoc>0){
	   	 	int index=pf_target->lookupIndex;
	   	 	int lookupMask = (1<<lookupOffset)-1;
	   	 	lookup = (lookupTable[index]<<lookupOffset) + ((pf_target->address)&lookupMask);
	   	 	lookupTick[index]=curTick();
	   	 	if(lookup == pf_target->address)prefetchStats.lookupCorrect++;
	    		else prefetchStats.lookupWrong++;
    		}
    		
    		DPRINTF(HWPrefetch, "Prefetching %x on miss at %x, PC \n", lookup << lBlkSize, addr << lBlkSize, pc);
    		addresses.push_back(AddrPriority(lookup << lBlkSize, delay));
    		delay += cacheDelay;
    		deg++;
    		if(deg<max /*&& pf_target->confident*/) pf_target = getHistoryEntry(lookup, is_secure,false,true,temporal, false);

   	 }
    }

    // Update the entry
    if(entry != nullptr) {
    	entry->lastAddress = addr;
    }


}

Triage::AddressMapping*
Triage::getHistoryEntry(Addr paddr, bool is_secure, bool add, bool readonly, bool temporal, bool clearing)
{
	    TriageHashedSetAssociative* thsa = dynamic_cast<TriageHashedSetAssociative*>(addressMappingCache.indexingPolicy);
	  				if(!thsa)  assert(0);
    	cachetags->clearSetWay(thsa->extractSet(paddr)/maxWays, thsa->extractSet(paddr)%maxWays); 
    if(should_rearrange) {

	    int index= paddr % (way_idx.size()); //Not quite the same indexing strategy, but close enough.
	    
	    if(way_idx[index] != thsa->ways) {
	    	if(way_idx[index] !=0) prefetchStats.metadataAccesses+= thsa->ways + way_idx[index];
	    	way_idx[index]=thsa->ways;
	    }
    }

    AddressMapping *ps_entry =
        addressMappingCache.findEntry(paddr, is_secure);
    if(readonly || !add) prefetchStats.metadataAccesses++;
    if (ps_entry != nullptr) {
        // A PS-AMC line already exists
        addressMappingCache.weightedAccessEntry(ps_entry,temporal?1:0,false); //For RRIP, touch
    } else {
        if(!add) return nullptr;
        ps_entry = addressMappingCache.findVictim(paddr);
        assert(ps_entry != nullptr);
        if(!clearing) for(int x=0;x<64;x++) hawksets[x].decrementOnLRU(ps_entry->index,&trainingUnit);
	assert(!ps_entry->isValid());
        addressMappingCache.insertEntry(paddr, is_secure, ps_entry);
        addressMappingCache.weightedAccessEntry(ps_entry,temporal?1:0, true); //For RRIP, don't touch
    }

    return ps_entry;
}




uint32_t
TriageHashedSetAssociative::extractSet(const Addr addr) const
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
TriageHashedSetAssociative::extractTag(const Addr addr) const
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
