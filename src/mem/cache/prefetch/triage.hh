/**
 * @file
 * Describes a history prefetcher.
 */

#ifndef __MEM_CACHE_PREFETCH_Triage_HH__
#define __MEM_CACHE_PREFETCH_Triage_HH__

#include <string>
#include <unordered_map>
#include <vector>

#include "base/sat_counter.hh"
#include "base/types.hh"
#include "mem/cache/tags/base.hh"
#include "mem/cache/prefetch/associative_set.hh"
#include "mem/cache/prefetch/queued.hh"
#include "mem/cache/replacement_policies/replaceable_entry.hh"
#include "mem/cache/tags/indexing_policies/set_associative.hh"
#include "mem/packet.hh"
#include "base/random.hh"

#include "params/TriageHashedSetAssociative.hh"

#include "bloom.h"



namespace gem5
{

struct TriagePrefetcherParams;

GEM5_DEPRECATED_NAMESPACE(Prefetcher, prefetch);
namespace prefetch
{

/**
 * Override the default set associative to apply a specific hash function
 * when extracting a set.
 */
class TriageHashedSetAssociative : public SetAssociative
{
  public:
    uint32_t extractSet(const Addr addr) const override;
   protected:
    Addr extractTag(const Addr addr) const override;

  public:
    int ways;
    int max_ways;
    TriageHashedSetAssociative(
        const TriageHashedSetAssociativeParams &p)
      : SetAssociative(p), ways(0),max_ways(8)
    {
    }
    ~TriageHashedSetAssociative() = default;
};


class Triage : public Queued
{

    /** Number of maximum prefetches requests created when predicting */
    const unsigned degree;

    /**
     * Training Unit Entry datatype, it holds the last accessed address and
     * its secure flag
     */

    BaseTags* cachetags;
    const unsigned cacheDelay;
    const bool should_rearrange;
    const bool store_unreliable;


    const int max_size;
    const int size_increment;
    int64_t global_timestamp;
    int current_size;
    int target_size;
    const int maxWays;

    const int hawkeyeThreshold;

    bloom bl;

    std::vector<int> way_idx;

    struct TrainingUnitEntry : public TaggedEntry
    {
        Addr lastAddress;
        SatCounter8  temporal;

        TrainingUnitEntry() : lastAddress(0), temporal(4,8)
        {}

        void
        invalidate() override
        {
        	TaggedEntry::invalidate();
                lastAddress = 0;
                temporal.reset();
        }


    };
    /** Map of PCs to Training unit entries */
    AssociativeSet<TrainingUnitEntry> trainingUnit;

    Addr lookupTable[1024];
    uint64_t lookupTick[1024];
    const int lookupAssoc;
    const int lookupOffset;

    struct Hawkeye
    {
      int iteration;
      uint64_t set;
      uint64_t setMask; // address_map_rounded_entries/ maxElems - 1
      Addr logaddrs[64];
      Addr logpcs[64];
      int logsize[64];
      int maxElems = 8;

      Hawkeye(uint64_t mask) : iteration(0), set(0), setMask(mask)
        {
           reset();
        }

      Hawkeye() : iteration(0), set(0)
        {       }

      void reset() {
        iteration=0;
        for(int x=0;x<64;x++) {
        	logsize[x]=0;
        	logaddrs[x]=0;
        	logpcs[x]=0;
        }
        set = random_mt.random<uint64_t>(0,setMask);
      }

      void decrementOnLRU(Addr addr,AssociativeSet<TrainingUnitEntry>* trainer) {
      	 if((addr & setMask) != set) return;
         for(int y=iteration;y!=((iteration+1)&63);y=(y-1)&63) {
               if(addr==logaddrs[y]) {
               	    Addr pc = logpcs[y];
               	    TrainingUnitEntry *entry = trainer->findEntry(pc, false); //TODO: is secure
               	    if(entry!=nullptr) {
               	    	if(entry->temporal>=8) {
               	    		entry->temporal--;
               	    		//printf("%s evicted, pc %s, temporality %d\n",addr, pc,entry->temporal);
               	    	}

               	    }
               	    return;
               }
         }
      }

      void add(Addr addr, Addr pc,AssociativeSet<TrainingUnitEntry>* trainer) {
        if((addr & setMask) != set) return;
        logaddrs[iteration]=addr;
        logpcs[iteration]=pc;
        logsize[iteration]=0;


        TrainingUnitEntry *entry = trainer->findEntry(pc, false); //TODO: is secure
        if(entry!=nullptr) {
          for(int y=(iteration-1)&63;y!=iteration;y=(y-1)&63) {

               if(logsize[y] == maxElems) {
                 //no match
                 //printf("%s above max elems, pc %s, temporality %d\n",addr, pc,entry->temporal-1);
                 entry->temporal--;
                 break;
               }
               if(addr==logaddrs[y]) {
                 //found a match
                 //printf("%s fits, pc %s, temporality %d\n",addr, pc,entry->temporal+1);
                   entry->temporal++;
                   for(int z=y;z!=iteration;z=(z+1)&63){
                   	logsize[z]++;
                   }

                break;
               }
            }
        }
        iteration++;
        iteration = iteration % 64;
      }

    };





    Hawkeye hawksets[64];

    /** Address Mapping entry, holds an address and a confidence counter */
    struct MarkovMapping : public TaggedEntry
    {
    	Addr index; //Just for maintaining HawkEye easily. Not real.
        Addr address;
        int lookupIndex;
        bool confident;
        MarkovMapping() : index(0), address(0), confident(false)
        {}


        void
        invalidate() override
        {
                TaggedEntry::invalidate();

                address = 0;
                confident = false;
        }
    };

    /** History mappings table */
    AssociativeSet<MarkovMapping> markovTable;

    MarkovMapping* getHistoryEntry(Addr index, bool is_secure, bool replace, bool readonly, bool temporal, bool clearing);

  public:
    Triage(const TriagePrefetcherParams &p);
    ~Triage() = default;

    void calculatePrefetch(const PrefetchInfo &pfi,
                           std::vector<AddrPriority> &addresses) override;
};

} // namespace prefetch
} // namespace gem5

#endif // __MEM_CACHE_PREFETCH_TRIAGE_HH__
