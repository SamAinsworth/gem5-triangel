
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
    store_unreliable(p.store_unreliable),
    max_size(p.address_map_actual_entries),
    size_increment(p.address_map_actual_entries/(p.address_map_actual_cache_assoc/p.address_map_line_assoc)),
    global_timestamp(0),
    current_size(0),
    target_size(0),
    historyLineAssoc(p.address_map_line_assoc),
    maxLineAssoc(p.address_map_actual_cache_assoc),
    hawkeyeThreshold(p.hawkeye_threshold),
    bl(),
    trainingUnit(p.training_unit_assoc, p.training_unit_entries,
                 p.training_unit_indexing_policy,
                 p.training_unit_replacement_policy),
    addressMappingCache(p.address_map_rounded_cache_assoc,
                          p.address_map_rounded_entries,
                          p.address_map_cache_indexing_policy,
                          p.address_map_cache_replacement_policy,
                          AddressMapping())
{
	for(int x=0;x<64;x++) {
		hawksets[x].sampleHistory=p.sample_history;
		hawksets[x].sampleTwoHistory=p.sample_two_history;
		hawksets[x].setMask = p.address_map_rounded_entries/ hawksets[x].maxElems - 1;
		hawksets[x].reset();
	}
	addressMappingCache.setWayAllocationMax(0);
	assert(cachetags->numBlocks * historyLineAssoc == max_size * 2);
	assert(cachetags->getWayAllocationMax() * historyLineAssoc == maxLineAssoc * 2);

	assert(p.address_map_rounded_entries / p.address_map_rounded_cache_assoc == p.address_map_actual_entries / p.address_map_actual_cache_assoc);

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
    bool sequential = true;


    if (entry != nullptr) {
        trainingUnit.accessEntry(entry);
        correlated_addr_found = true;
        index = entry->lastAddress;
        Addr prev = entry->lastLastAddress;
            
    	for(int x=0; x<64; x++)hawksets[x].add(addr,index,prev,pc,&trainingUnit);
        temporal = entry->temporal>=hawkeyeThreshold;
        sequential = entry->sequence>=hawkeyeThreshold;

        if(addr == entry->lastAddress) return; // to avoid repeat trainings on sequence.

        target = addr;

    } else {
        entry = trainingUnit.findVictim(pc);
        DPRINTF(HWPrefetch, "Replacing Training Entry %x\n",pc);
        assert(entry != nullptr);
        assert(!entry->isValid());
        trainingUnit.insertEntry(pc, is_secure, entry);
    }

    global_timestamp++;


    if(correlated_addr_found && ((temporal && sequential) || store_unreliable)) {
        int add = bloom_add(&bl, &addr, sizeof(Addr));
        if(!add) target_size++;
   	while(target_size > current_size
            		     && current_size < max_size) {
            		        //check for size_increment to leave empty if unlikely to be useful.
            			current_size += size_increment;
            			assert(current_size <= max_size);
            			assert(cachetags->getWayAllocationMax()>1);
            			cachetags->setWayAllocationMax(cachetags->getWayAllocationMax()-1);
            			addressMappingCache.setWayAllocationMax(addressMappingCache.getWayAllocationMax()+historyLineAssoc);
            			assert(addressMappingCache.getWayAllocationMax()<=maxLineAssoc);
            			//increase associativity of the set structure by 1!
            			//Also, decrease LLC cache associativity by 1.
        }

        //TODO: add error expansion. Not that major though -- our bloom filter is big and this is a ceiling function, so who cares?
    }

    if(global_timestamp > 2000000) {
    	//Reset after 2 million prefetch accesses -- not quite the same as after 30 million insts but close enough

    	while(target_size < current_size && current_size >=size_increment) {
    		//reduce the assoc by 1.
    		//Also, increase LLC cache associativity by 1.
    		current_size -= size_increment;
	    	assert(current_size >= 0);
	    	assert(addressMappingCache.getWayAllocationMax()>=historyLineAssoc);
    		cachetags->setWayAllocationMax(cachetags->getWayAllocationMax()+1);
    		addressMappingCache.setWayAllocationMax(addressMappingCache.getWayAllocationMax()-historyLineAssoc);
    	}
    	target_size = 0;
    	global_timestamp=0;
    	bloom_reset(&bl);
    }
    
    if((!temporal || !sequential) && !store_unreliable) return;

    if (correlated_addr_found && (addressMappingCache.getWayAllocationMax()>0)) {
        // If a correlation was found, update the History table accordingly
	//DPRINTF(HWPrefetch, "Tabling correlation %x to %x, PC %x\n", index << lBlkSize, target << lBlkSize, pc);
	AddressMapping *mapping = getHistoryEntry(index, is_secure,false,false,temporal);
	if(mapping == nullptr) {
        	mapping = getHistoryEntry(index, is_secure,true,false,temporal);
        	mapping->address = target;
        	mapping->confident = false;
        }
        assert(mapping != nullptr);
        bool confident = mapping->address == target;
        bool wasConfident = mapping->confident;
        mapping->confident = confident;
        if(!wasConfident) {
        	mapping->address = target;
        }
    }

    if(target != 0 && (addressMappingCache.getWayAllocationMax()>0)) {
  	 AddressMapping *pf_target = getHistoryEntry(target, is_secure,false,true,temporal);
   	 unsigned deg = 0;
  	 unsigned delay = cacheDelay;
     	 unsigned max = degree;
   	 while (pf_target != nullptr && deg < max) //TODO: and confident? not clear from paper. public implementation suggests no
   	 {
    		DPRINTF(HWPrefetch, "Prefetching %x on miss at %x, PC \n", pf_target->address << lBlkSize, addr << lBlkSize, pc);
    		addresses.push_back(AddrPriority(pf_target->address << lBlkSize, delay));
    		delay += cacheDelay;
    		deg++;
    		if(deg<max /*&& pf_target->confident*/) pf_target = getHistoryEntry(pf_target->address, is_secure,false,true,temporal);

   	 }
    }

    // Update the entry
    if(entry != nullptr) {
    	entry->lastAddress = addr;
    }


}

Triage::AddressMapping*
Triage::getHistoryEntry(Addr paddr, bool is_secure, bool add, bool readonly, bool temporal)
{

    AddressMapping *ps_entry =
        addressMappingCache.findEntry(paddr, is_secure);
    if(readonly || !add) prefetchStats.metadataAccesses++;
    if (ps_entry != nullptr) {
        // A PS-AMC line already exists
        addressMappingCache.weightedAccessEntry(ps_entry,temporal?1:0); //Higher weight is higher priority.
    } else {
        if(!add) return nullptr;
        ps_entry = addressMappingCache.findVictim(paddr);
        assert(ps_entry != nullptr);
	assert(!ps_entry->isValid());
        addressMappingCache.insertEntry(paddr, is_secure, ps_entry);
        addressMappingCache.weightedAccessEntry(ps_entry,temporal?1:0);
    }

    return ps_entry;
}




uint32_t
TriageHashedSetAssociative::extractSet(const Addr addr) const
{
	//Input is already blockIndex so no need to remove block again.
    const Addr offset = addr;
        return offset & setMask;   //setMask is numSets-1

    //The below is a literal interpretation of Triage-ISR. It complicates things here (and doesn't help performance without stream) so I avoid it.
   /* const Addr hash1 = offset & ((1<<16)-1);
    const Addr hash2 = (offset >> 16) & ((1<<16)-1);
        const Addr hash3 = (offset >> 32) & ((1<<16)-1);
    return (hash1 ^ hash2 ^ hash3) & setMask;   //setMask is numSets-1
    */
}

Addr
TriageHashedSetAssociative::extractTag(const Addr addr) const
{
    //Input is already blockIndex so no need to remove block again.
    //TODO: match the tag size sig of the chosen format. Currently hardcoded to 127.

    //Description in Triage-ISR confuses whether the index is just the 16 least significant bits,
    //or the weird index above. The tag can't be the remaining bits if we use the literal representation!

    //Here I just remove the set offset and xor together the rest.
    //Really, xoring in the set bits is innocuous if they're all the same.

    Addr offset = addr >> tagShift;
    int result = 0;

    const int shiftwidth=10;

    for(int x=0; x<64-tagShift; x+=shiftwidth) {
       result ^= (offset & ((1<<shiftwidth)-1));
       offset = offset >> shiftwidth;
    }
    return result;
}


} // namespace prefetch
} // namespace gem5
