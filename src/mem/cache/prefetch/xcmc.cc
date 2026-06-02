
//Adapted from https://github.com/OpenXiangShan/GEM5/blob/xs-dev/src/mem/cache/prefetch
#include "mem/cache/prefetch/xcmc.hh"

#include "debug/HWPrefetch.hh"
#include "mem/cache/prefetch/associative_set_impl.hh"
#include "params/XCMCPF.hh"

namespace gem5
{
namespace prefetch
{
int64_t XCMCPF::global_timestamp=0;
int XCMCPF::target_ways=0;
int XCMCPF::current_ways=0;

XCMCPF::SizeDuel* XCMCPF::sizeDuelPtr=nullptr;
AssociativeSet<XCMCPF::StorageEntry>* XCMCPF::storagePtr=nullptr;

std::vector<uint32_t> XCMCPF::setPrefetch(17,0);

XCMCPF::XCMCPF(const XCMCPFParams &p)
: Queued(p),
      cachetags(p.cachetags),
      fix_indexing(p.fix_indexing),
    recorder(new Recorder()),
    storage(p.storage_assoc, p.storage_entries, p.storage_indexing_policy,
            p.storage_replacement_policy, StorageEntry()),
    trigger()
{
    setPrefetch.resize(p.storage_assoc+1,0);
    current_ways = 0;
    target_ways=0;
                    trigger.clear();
    storagePtr = &storage;

    sizeDuelPtr= sizeDuels;
    for(int x=0; x<64; x++) {
        sizeDuelPtr[x].reset((p.storage_entries/p.storage_assoc)-1,16,cachetags->getWayAllocationMax());
    }
}

void
XCMCPF::sizer(Addr block_addr, Addr pc, bool is_write) {
	//This is commented out because it's a Triangel-based sizer to borrow space from the cache.
	    global_timestamp++;
    //Dueller from Triangel
        bool should_pf =!is_write;
    for(int x=0; x<64; x++) {
        //Here we update the size duellers, to work out for each cache set whether it is better to be markov table or L3 cache.
        int res =    	sizeDuelPtr[x].checkAndInsert(block_addr,should_pf, pc);
        if(res==0)continue;
        const int ratioNumer=2;
        const int ratioDenom= 4;//look_check && should_pf && countentry->highPatternConfidence >=highUpperHistory? 2 : 4;
        int cache_hit = res%128; //This is just bit encoding of cache hits.
        int pref_hit = res/128; //This is just bit encoding of prefetch hits.
        int cache_set = cache_hit-1; //Encodes which nth most used replacement-state we hit at, if any.
        int pref_set = pref_hit-1; //Encodes which nth most used replacement-state we hit at, if any.
        assert(!cache_hit || (cache_set<setPrefetch.size()-1 && cache_set>=0));
        assert(!pref_hit || (pref_set<setPrefetch.size()-1 && pref_set>=0));
        if(cache_hit) for(int y= setPrefetch.size()-2-cache_set; y>=0; y--) setPrefetch[y]++;
        // cache partition hit at this size or bigger. So hit in way 14 = y=17-2-14=1 and 0: would hit with 0 ways reserved or 1, not 2.
        if(pref_hit)for(int y=pref_set+1; y<setPrefetch.size(); y++) setPrefetch[y]+=(ratioNumer*sizeDuelPtr[x].temporalModMax)/ratioDenom;
        // ^ pf hit at this size or bigger. one-indexed (since 0 is an alloc on 0 ways). So hit in way 0 = y=1--16 ways reserved, not 0.

    }

    if(global_timestamp >500000) {
        //Here we choose the size of the Markov table based on the optimum for the last epoch
        int64_t counterSizeSeen = 0;

        for(int x=0; x<(setPrefetch.size()-1)/2+1; x++) {
            if(setPrefetch[x]>counterSizeSeen) {
                                target_ways= x;
                counterSizeSeen = setPrefetch[x];
            }
        }

        int64_t currentscore = setPrefetch[current_ways];
        int64_t targetscore = setPrefetch[target_ways];

        if(target_ways != current_ways && targetscore>currentscore) {
            current_ways = target_ways;
            printf("size: %d, tick %ld \n",current_ways,curTick());
            for(int x=0; x<setPrefetch.size(); x++) {
                printf("%d: score %ld\n", x, setPrefetch[x]);
            }
            assert(current_ways >= 0);

            storagePtr->setWayAllocationMax(current_ways);
            cachetags->setWayAllocationMax(setPrefetch.size()-1-current_ways);
        }
        printf("End of epoch:\n");
        global_timestamp=0;
        for(int x=0; x<setPrefetch.size(); x++) {
            setPrefetch[x]=0;
        }

    }

}

void
XCMCPF::calculatePrefetch(const PrefetchInfo &pfi,
                            std::vector<AddrPriority> &addresses)
{


   //PC isolation here lowers coverage...
    Addr pc = pfi.hasPC() ? pfi.getPC() : 0;

    Addr addr = pfi.getAddr();
    Addr block_addr = blockIndex(addr); // takes off 6 Least Significant Bits for cache line
    bool is_secure = pfi.isSecure();


    DPRINTF(HWPrefetch, "CMC train: pc: %lx, addr: %lx\n", pc, block_addr);


	sizer(block_addr,pc,pfi.isWrite()); // sets size based on Triangel method


    if(current_ways==0 || pfi.isWrite()) return;



    // Prefetch: check if there is a match
    StorageEntry *match_entry = storagePtr->findEntry(hash(block_addr, pc), is_secure);
    prefetchStats.metadataAccesses++;
    if (match_entry) {
        storagePtr->accessEntry(match_entry);
        // prefetch on cache miss or on prefetch hit
        DPRINTF(HWPrefetch, "Storage hit, trigger pc: %lx, addr: %lx\n",
                pc, block_addr);
      //printf("=== Storage hit, trigger addr: %lx\n", block_addr);

        for (auto addr: match_entry->addresses) {
            addresses.push_back(AddrPriority(addr, 25));
        }
    }

    // Train: update temporal access chain
    bool finished = false;


    /* 1. Train trigger */
    //This will try to reuse an existing trigger that has been seen recently, should one exist.
    //Otherwise, it will find a new trigger.
    //Intuition is that if you didn't aggressively reuse triggers, you'd pollute your cache with an entry per address, which adds massive redundancy,
    //evicts other useful metadata, and will take ages to replace old, potentially wrong entries.
    bool train_trigger =
        (trigger.size()<1 || match_entry) && trigger.size()<STACK_SIZE;
    bool do_training =
        !trigger.empty() && (!train_trigger || fix_indexing);
    if (train_trigger) {
        //printf("train_trigger index: %d, addr: %lx\n",
        //        trigger.size(), block_addr);
        assert(trigger.size()<STACK_SIZE);

        trigger.push_back(RecordEntry(pc, block_addr, is_secure));
    }

    /* 2. Train entry */
    if (do_training) {
        bool trained = recorder->train_entry(addr, is_secure, &finished);
        auto &trigger_head = trigger.front();
        if (trained) {
            //printf("trained %x\n", block_addr);
        }
        if (finished) {
			//finished gets set once a coalesced set of 16 targets get stored in the training entry.
            //printf("trigger train finished, pc: %lx, addr: %lx\n",
               //     trigger_head.pc, trigger_head.addr);

            StorageEntry *entry = storagePtr->findEntry(hash(trigger_head.addr, trigger_head.pc), trigger_head.is_secure);
            if (entry) {
                // storage.accessEntry(entry); do not update replacement
                DPRINTF(HWPrefetch, "CMC: enter the same trigger, pc: %lx, addr: %lx\n",
                                    trigger_head.pc, trigger_head.addr);
                entry->addresses = recorder->entries;


            } else {
                entry = storagePtr->findVictim(hash(trigger_head.addr, trigger_head.pc));
                entry->addresses = recorder->entries;



                storagePtr->insertEntry(
                    hash(trigger_head.addr, trigger_head.pc),
                    trigger_head.is_secure,
                    entry
                );
            }

            for (auto addr: recorder->entries) {
                DPRINTF(HWPrefetch, "entry addr: 0x%lx\n",
                        addr);
            }
            prefetchStats.metadataAccesses++;
            trigger.pop_front();

			if(fix_indexing && trigger.size()<1) {
				//This is fixed: if we don't have this here, we end up with triggers and trainings as distinct,
				//meaning we never reach 100% coverage.
				//This makes the trigger the last addr of the previous group, provided no other trigger already
				// is prepared, because it was a recent reused addr.
				trigger.push_back(RecordEntry(pc, block_addr, is_secure));
			}

            recorder->reset();

        }
    }
}

bool
XCMCPF::Recorder::train_entry(
    Addr addr,
    bool is_secure,
    bool *finished
) {

        entries.push_back(addr);
        index++;
        //There was an off-by-one error here: it stored 17 entries

    if (index >= nr_entry) {
        // entry full
        *finished = true;
    }

    return true;
}



void
XCMCPF::Recorder::reset() {
    index = 0;
    entries.clear();
}

void
XCMCPF::StorageEntry::invalidate() {
    if (false) {
        if (this->isValid()) {
            printf("entry victim: refcnt = %d\n", this->refcnt);
        }
    }
    TaggedEntry::invalidate();
}

}  // prefetch
}  // gem5
