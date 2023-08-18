
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
//    degree(p.degree),
    cachetags(p.cachetags),
    cacheDelay(p.cache_delay),
    triage_mode(p.triage_mode),
    max_size(p.address_map_actual_entries),
    size_increment(p.address_map_actual_entries/(p.address_map_actual_cache_assoc/p.address_map_line_assoc)),
    global_timestamp(0),
    reuse_timer(0),
    current_size(0),
    target_size(0),
    historyLineAssoc(p.address_map_line_assoc),
    maxLineAssoc(p.address_map_actual_cache_assoc),
    sum_deviation(0),
    paths(0),
    historyNonHistory(8,128),
    bl(),
    trainingUnit(p.training_unit_assoc, p.training_unit_entries,
                 p.training_unit_indexing_policy,
                 p.training_unit_replacement_policy),
    sampleUnit(p.sample_assoc,
    		  p.sample_entries,
    		  p.sample_indexing_policy,
    		  p.sample_replacement_policy),
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
	addressMappingCache.setWayAllocationMax(0);
	assert(cachetags->numBlocks * historyLineAssoc == max_size * 2);
	assert(cachetags->getWayAllocationMax() * historyLineAssoc == maxLineAssoc * 2);

	assert(p.address_map_rounded_entries / p.address_map_rounded_cache_assoc == p.address_map_actual_entries / p.address_map_actual_cache_assoc);

	bloom_init2(&bl,p.address_map_actual_entries, 0.01);
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

    bool should_pf=false;
    bool should_pf_twice = false;
    reuse_timer++;

    if (entry != nullptr) {
        trainingUnit.accessEntry(entry);
        correlated_addr_found = true;
        index = entry->lastAddress;

        if(addr == entry->lastAddress) return; // to avoid repeat trainings on sequence.

        //if very sure, index should be lastLastAddress. TODO: We could also try to learn timeliness here, by tracking PCs at the MSHRs.
        if(entry->currently_twodist_pf) index = entry->lastLastAddress;
        target = addr;
        should_pf = (entry->reuseConfidence > 12 && entry->historyConfidence > 8) || triage_mode; //8 is the reset point.
        
        if(entry->reuseConfidence > 12 && randomChance(8,8)) {
           if(should_pf) historyNonHistory++;
           else historyNonHistory--;
        }
    }

    if(entry==nullptr && randomChance(8,8)){ //only start adding samples for frequent entries.
        entry = trainingUnit.findVictim(pc);
        DPRINTF(HWPrefetch, "Replacing Training Entry %x\n",pc);
        assert(entry != nullptr);
        assert(!entry->isValid());
        trainingUnit.insertEntry(pc, is_secure, entry);
    }


    if(correlated_addr_found) {
    	//Check sample table for entry.
    	SampleEntry *sentry = sampleUnit.findEntry(addr, is_secure);
    	if(sentry != nullptr && sentry->pc == pc) {
    		sentry->reused = true;
    		entry->reuseConfidence++;
    		int64_t distance = entry->local_timestamp - sentry->local_timestamp;
    		int64_t gdistance = reuse_timer - sentry->globalReuseDistance;
    		if(distance > 0) { //sometimes it's lower - perhaps because entry replaced???
    			entry->reuseDistance = entry->reuseDistance == -1?
    		  	    distance : (distance >> 3) + ((7*entry->reuseDistance)>>3);
    		  	    
    		  	entry->globalReuseDistance = entry->globalReuseDistance == -1?
    		  	    gdistance : (gdistance >> 3) + ((7*entry->globalReuseDistance)>>3);
    		  	
    			int absdist = distance - entry->reuseDistance;
    			if(absdist < 0) absdist = -absdist;
    			if(absdist < entry->reuseDistance)
    		 	    entry->deviation = (absdist >> 3) + ((7* entry->deviation) >> 3);
    		}

     	    	DPRINTF(HWPrefetch, "Found reuse for addr %x, PC %x, distance %ld (train %ld vs sample %ld) confidence %d\n",addr, pc, distance, entry->local_timestamp, sentry->local_timestamp, entry->reuseConfidence+0);
    		
    		//TODO: What benefit do we get from checking both? Do we need to check last three?

    		if(entry->lastAddress == sentry->last || entry->lastLastAddress == sentry->last || cachetags->findBlock(sentry->last<<lBlkSize,is_secure)) {
    			entry->historyConfidence++;
    			if(entry->replaceRate < 8) entry->replaceRate.reset();
    		}
    		
    		else entry->historyConfidence--;
    		
        	if(entry->lastAddress == sentry->last) DPRINTF(HWPrefetch, "Match for previous address %x confidence %d\n",entry->lastAddress, entry->historyConfidence+0);
    		if(entry->lastLastAddress == sentry->last) DPRINTF(HWPrefetch, "Match for previous previous address %x confidence %d\n",entry->lastLastAddress, entry->historyConfidence+0);
    	}
    	else if(randomChance(entry->reuseConfidence,entry->replaceRate)) {
    		//Fill sample table
    		sentry = sampleUnit.findVictim(addr);
    		assert(sentry != nullptr);
    		if(sentry->pc !=0) {
    			    TrainingUnitEntry *pentry = trainingUnit.findEntry(sentry->pc, is_secure);
    			    if(pentry != nullptr) {
    			    	    uint64_t distance = pentry->local_timestamp - sentry->local_timestamp;
    			    	    DPRINTF(HWPrefetch, "Replacing PC %x with PC %x, old distance %d\n",sentry->pc, pc, distance);
    			    	    if(distance > max_size || (pentry->reuseDistance !=-1 && distance > pentry->reuseDistance*4)) {
    			    	        //TODO: Change max size to be relative, based on current tracking set?
    			            	trainingUnit.accessEntry(pentry);
    			            	if(!sentry->reused) pentry->reuseConfidence--;
    			            	entry->replaceRate++; //only replacing oldies -- can afford to be more aggressive.
    			            	if(triage_mode) {
    			            	    	pentry->reuseDistance = pentry->reuseDistance == -1? distance : pentry->reuseDistance;
    			            	    	//As a mock-up for Triage looking at unique accesses regardless of in repeated sequence.
    			            	}
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
    		sentry->globalReuseDistance = reuse_timer;
    		sentry->last = entry->lastAddress;
    	}
    }


    if(!triage_mode && correlated_addr_found && should_pf) {
	    global_timestamp++;
            if(!entry->historied_this_round) {
                int block_sd = 2*(sum_deviation + entry->deviation) / sqrt(paths+1);
            	if((entry->reuseDistance + block_sd + target_size>max_size || (entry->historyConfidence < 12 && entry->deviation > entry->reuseDistance)) && !triage_mode) {
            		entry->currently_blocking = true;
            		DPRINTF(HWPrefetch, "Blocking PC %x with distance %d, too big\n",pc, entry->reuseDistance);
            	} else {

            	        sum_deviation+= entry->deviation;
            	        paths++;
            		target_size += entry->reuseDistance;

            		DPRINTF(HWPrefetch, "Starting to PF PC %x with distance %d\n",pc, entry->reuseDistance);
            	        entry->was_twodist_pf = entry->currently_twodist_pf;
            	        entry->currently_twodist_pf = !triage_mode &&
            	             ((entry->reuseConfidence >14 && entry->historyConfidence > 14)
            	             || (entry->was_twodist_pf && entry->reuseConfidence >13
            	             && entry->historyConfidence > 13));
            	        if(entry->replaceRate < 8) entry->replaceRate.reset();
            		entry->historied_this_round = true;

            		while(target_size + block_sd > current_size + size_increment / 8 
            		     && target_size > size_increment / 8 && historyNonHistory > 16 && current_size < max_size) {
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
            	}

            }
    }

    if(triage_mode && correlated_addr_found && should_pf) {
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

        //TODO: add shrink and error
    }


    if(!triage_mode && global_timestamp > current_size+size_increment) {
    	//reset the timestamp and forgive on currently blocking.

    	while((current_size > target_size + size_increment / 4 + 2* sum_deviation / sqrt(paths) || historyNonHistory < 8) && current_size >= size_increment) {
    		//reduce the assoc by 1.
    		//Also, increase LLC cache associativity by 1.
    		//Do we want more hysteresis than this?
    		current_size -= size_increment;
	    	assert(current_size >= 0);
	    	assert(addressMappingCache.getWayAllocationMax()>=historyLineAssoc);
    		cachetags->setWayAllocationMax(cachetags->getWayAllocationMax()+1);
    		addressMappingCache.setWayAllocationMax(addressMappingCache.getWayAllocationMax()-historyLineAssoc);

    	}

    	for(TrainingUnitEntry& ent : trainingUnit) {
    		ent.historied_this_round = false;
    		ent.currently_blocking = false;
    		    //periodic forgiveness of heavy-traffic entries.
    		if(randomChance(13,13)) ent.replaceRate.reset();
    	}

    	global_timestamp = 0;
    	target_size = 0;
	sum_deviation = 0;
	paths=0;
    }

    if(entry!= nullptr && entry->currently_blocking && !triage_mode) should_pf = false;

    if (correlated_addr_found && should_pf && (addressMappingCache.getWayAllocationMax()>0)) {
        // If a correlation was found, update the History table accordingly
	//DPRINTF(HWPrefetch, "Tabling correlation %x to %x, PC %x\n", index << lBlkSize, target << lBlkSize, pc);
	AddressMapping *mapping = getHistoryEntry(index, is_secure,false,false);
	if(mapping == nullptr) {
        	mapping = getHistoryEntry(index, is_secure,true,false);
        	mapping->address = target;
        	mapping->confident = triage_mode || entry->historyConfidence > 12;
        }
        assert(mapping != nullptr);
        bool confident = mapping->address == target || mapping->address == entry->lastAddress; //second one covers a shift to 2-off.
        bool wasConfident = mapping->confident;
        mapping->confident = confident;
        if(!wasConfident || entry->currently_twodist_pf != entry->was_twodist_pf || confident) { //confident case covers shift to 2-off
        	mapping->address = target;
        }
    }

    if(target != 0 && should_pf && (addressMappingCache.getWayAllocationMax()>0)) {
  	 AddressMapping *pf_target = getHistoryEntry(target, is_secure,false,true);
   	 unsigned deg = 0;
  	 unsigned delay = cacheDelay;
  	 should_pf_twice = !triage_mode && pf_target != nullptr
  	         && entry->historyConfidence > 14 && entry->reuseConfidence>14 && pf_target->confident;
   	 unsigned max = should_pf_twice? 2 : (should_pf? 1 : 0);
   	 //if(pf_target == nullptr && should_pf) DPRINTF(HWPrefetch, "Target not found for %x, PC %x\n", target << lBlkSize, pc);
   	 while (pf_target != nullptr && deg < max &&
   	 (pf_target->confident || (!triage_mode && entry->historyConfidence > 13))
   	 ) { //TODO: do we always pf at distance 1 if not confident?
    		DPRINTF(HWPrefetch, "Prefetching %x on miss at %x, PC \n", pf_target->address << lBlkSize, addr << lBlkSize, pc);
    		int extraDelay = cacheDelay;
    		if(lastAccessFromPFCache) {
    			Cycles time = curCycle() - pf_target->cycle_issued;
    			if(time >= cacheDelay) extraDelay = 0;
    			else if (time < cacheDelay) extraDelay = time;
    		}
    		if(extraDelay == cacheDelay) addresses.push_back(AddrPriority(pf_target->address << lBlkSize, delay));
    		delay += extraDelay;
    		//if(extraDelay < cacheDelay && should_pf_twice && max<4) max++;
    		deg++;
    		if(deg<max) pf_target = getHistoryEntry(pf_target->address, is_secure,false,true);

   	 }
    }

        // Update the entry
    if(entry != nullptr) {
    	entry->lastLastAddress = triage_mode? 0 : entry->lastAddress;
    	entry->lastLastAddressSecure = entry->lastAddressSecure;
    	entry->lastAddress = addr;
    	entry->lastAddressSecure = is_secure;
    	entry->local_timestamp ++;
    }


}

Triangel::AddressMapping*
Triangel::getHistoryEntry(Addr paddr, bool is_secure, bool add, bool readonly)
{
    if(readonly && !triage_mode) { //check the cache first.
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
        addressMappingCache.accessEntry(ps_entry);
    } else {
        if(!add) return nullptr;
        ps_entry = addressMappingCache.findVictim(paddr);
        assert(ps_entry != nullptr);
	assert(!ps_entry->isValid());
        addressMappingCache.insertEntry(paddr, is_secure, ps_entry);
    }

    if(readonly && !triage_mode) {
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
TriangelHashedSetAssociative::extractTag(const Addr addr) const
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
