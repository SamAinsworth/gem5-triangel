#ifndef __MEM_CACHE_PREFETCH_TETRAHEDRANGEL_HH__
#define __MEM_CACHE_PREFETCH_TETRAHEDRANGEL_HH__
//#include <boost/circular_buffer.hpp>
//#include <boost/compute/detail/lru_cache.hpp>

//Adapted from https://github.com/OpenXiangShan/GEM5/blob/xs-dev/src/mem/cache/prefetch
#include <string>
#include <unordered_map>
#include <vector>

#include "base/sat_counter.hh"
#include "base/types.hh"
#include "mem/cache/prefetch/associative_set.hh"
#include "mem/cache/prefetch/queued.hh"
#include "mem/cache/replacement_policies/replaceable_entry.hh"
#include "mem/cache/tags/indexing_policies/set_associative.hh"
#include "mem/packet.hh"
#include "params/TetrahedrangelPrefetcher.hh"
#include "base/random.hh"
#include "base/statistics.hh"


namespace gem5
{
struct TetrahedrangelPrefetcherParams;
GEM5_DEPRECATED_NAMESPACE(Prefetcher, prefetch);

namespace prefetch
{


class TetrahedrangelPrefetcher : public Queued
{
    BaseTags* cachetags;
            BaseTags* sctags;
        const bool always_prefetch;
     const bool pc_training;
     const bool use_remap;
     const bool remap_scatter;
     const bool use_sleep;
     const bool ignore_ptag;
     const bool aggressive;
     const bool no_samePC;
     const bool no_diffPC;
            uint64_t lastTimeliness;
    uint64_t lastAccurate;
    uint64_t lastAccInacc;
    int current_scatter;
    int current_degree;



    uint64_t duelaccurate;
    uint64_t dueltotal;

    bool sleepmode;


  public:
    static uint64_t hash(Addr addr, Addr pc) {
        return addr  ^ (((pc ^ (pc/64))&63)<<31);
    }

         static int current_ways;
        static int target_ways;
      static int64_t global_timestamp;


        uint64_t reuseBlocked;
        uint64_t patternBlocked;
        uint64_t passed;
        uint64_t samepf;
        uint64_t uncategorised;

    static std::vector<uint32_t> setPrefetch;




            struct CounterEntry
        {
            SatCounter8  reuseConfidence;
            SatCounter8  patternConfidence;
            SatCounter8 samePCPatternConf;

            CounterEntry() : reuseConfidence(4,8), patternConfidence(4,8), samePCPatternConf(4,8)
            {}

        };
        /** Map of PCs to Training unit entries */
        std::vector<CounterEntry> counterUnit;

    struct SamplerTrainer
    {
		Addr pc;
		Addr lastAddr;
		uint64_t tick;
		uint64_t count;

		SamplerTrainer() : pc(-1), lastAddr(0), tick(0),count(0) {}
	};

        std::vector<SamplerTrainer> samplerTrainer;



        static bool
triangelhash2(unsigned int x, unsigned int y) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return ((x%y)==0);
}


        /** Owned by the Simpler Sampler TM*/
        struct GhostMarkovMapping : public TaggedEntry
        {
            Addr index;
            Addr address;
            Addr pc;
            bool used;
            GhostMarkovMapping() : index(0), address(0), pc(-1), used(false)
            {}

        };

        /** History mappings table */
        AssociativeSet<GhostMarkovMapping> ghostMarkovTable;
        uint64_t simplerSamplerDuelledSets[64];

        /** Test pf entry, tagged by data address**/
        struct SecondChanceEntry: public TaggedEntry
        {
			Addr addr;
            Addr pc;
            bool used;
            bool samePC;
            bool in_cache;
        };
        AssociativeSet<SecondChanceEntry> secondChanceUnit;

        GhostMarkovMapping* getSimplerSamplerEntry(Addr index, bool is_secure, bool replace, bool fill);


  struct SizeDuel
  {
  	int idx;
  	uint64_t set;
  	uint64_t setMask;
  	uint64_t temporalMod; //[0..12] Entries Per Line

  	uint64_t temporalModMax; //12 by default
  	uint64_t cacheMaxAssoc;


  	std::vector<Addr> cacheAddrs; // [0..16] should be set by the nsets of the L3 cache.
  	std::vector<uint64_t> cacheAddrTick;
  	std::vector<Addr> temporalAddrs;
  	std::vector<uint64_t> temporalAddrTick;
  	std::vector<bool> inserted;

  	SizeDuel()
        {
        }
        void reset(uint64_t mask, uint64_t modMax, uint64_t cacheAssoc) {
        	setMask=mask;
        	temporalModMax = modMax;
        	cacheMaxAssoc=cacheAssoc;
        	cacheAddrTick.resize(cacheMaxAssoc);
        	temporalAddrs.resize(cacheMaxAssoc);
        	cacheAddrs.resize(cacheMaxAssoc);
        	temporalAddrTick.resize(cacheMaxAssoc);
        	inserted.resize(cacheMaxAssoc,false);
        	for(int x=0;x<cacheMaxAssoc;x++) {
			cacheAddrTick[x]=0;
			temporalAddrs[x]=0;
			cacheAddrs[x]=0;
			temporalAddrTick[x]=0;
		}
		set = random_mt.random<uint64_t>(0,setMask);
		temporalMod = random_mt.random<uint64_t>(0,modMax-1); // N-1, as range is inclusive.
        }

            int checkAndInsert(Addr addr, bool should_pf, Addr pf_offset) {
	  	int ret = 0;
	  	bool foundInCache=false;
	  	bool foundInTemp=false;
	  	if((addr & setMask) != set) return ret;
  		for(int x=0;x<cacheMaxAssoc;x++) {
  			if(addr == cacheAddrs[x]) {
  				foundInCache=true;
	  				int index=cacheMaxAssoc-1;
	  				for(int y=0;y<cacheMaxAssoc;y++) {
	  					if(cacheAddrTick[x]>cacheAddrTick[y]) index--;
	  					assert(index>=0);
	  				}
	  				cacheAddrTick[x] = curTick();
	  				ret += index+1;
  			}
                    if(should_pf && TetrahedrangelPrefetcher::hash(addr,pf_offset) == temporalAddrs[x]) {


  				foundInTemp=true;

	  				int index=cacheMaxAssoc-1;
	  				for(int y=0;y<cacheMaxAssoc;y++) {
	  					if(temporalAddrTick[x]>temporalAddrTick[y]) index--;
	  					assert(index>=0);
	  				}

	  				ret += 128*(index+1);

	  			temporalAddrTick[x] = curTick();
	  			inserted[x]=true;
	  		}
  		}
  		if(!foundInCache) {
  			uint64_t oldestTick = (uint64_t)-1;
  			int idx = -1;
  			for(int x=0; x<cacheMaxAssoc;x++) {
  				if(cacheAddrTick[x]<oldestTick) {idx = x; oldestTick = cacheAddrTick[x];}
  			}
  			assert(idx>=0);
  			cacheAddrs[idx]=addr;
  			cacheAddrTick[idx]=curTick();
  		}
  		if(!foundInTemp && should_pf && (((addr / (setMask+1)) % temporalModMax) == temporalMod)) {
  			uint64_t oldestTick = (uint64_t)-1;
  			int idx = -1;
  			for(int x=0; x<cacheMaxAssoc;x++) {
  				if(temporalAddrTick[x]<oldestTick) {idx = x; oldestTick = temporalAddrTick[x]; }
			}
assert(idx>=0);
                    temporalAddrs[idx]=TetrahedrangelPrefetcher::hash(addr,pf_offset);
  			temporalAddrTick[idx]=curTick();
  		}
  		return ret;
  	}

  };
  SizeDuel sizeDuels[64];
  static SizeDuel* sizeDuelPtr;
  static int attached_cores;


    class StorageEntry;
    class RecordEntry
    {
        public:
			uint64_t lastTick;
            Addr pc;
            Addr addr;
            bool is_secure;
            bool should_xor;
            bool valid;
            RecordEntry(Addr p, Addr a, bool s, bool x, uint64_t tick)
                : pc(p), addr(a), is_secure(s), should_xor(x), valid(true), lastTick(tick) {}
            RecordEntry() : pc(0), addr(0), should_xor(false), is_secure(true), valid(false),lastTick(0) {}
    };
    class Recorder
    {
        public:
            std::vector<Addr> entries;
            int index;
            Addr logPC;
            Addr logAddr;
            bool should_xor;
            bool matched_entry;
            Recorder() : entries(), index(0), logPC(0), logAddr(0), should_xor(false), matched_entry(false) {}
            bool entry_empty() { return entries.empty(); }
            Addr get_base_addr() { return entries[0]; }

            bool train_entry(Addr, Addr, bool, bool, int, bool, bool*);
            void reset();
            const int nr_entry = 16;
        private:
    };

    class StorageEntry : public TaggedEntry
    {
        public:
            std::vector<Addr> addresses;
            Addr next;
            Addr nextPC;
            bool should_xor;
            bool valid;
            StorageEntry() : addresses(), next(0), nextPC(0), should_xor(false), valid(false) {}
            void invalidate() override;
    };
  private:

      int current_idx=0;

    std::vector<Recorder> recorder;
    AssociativeSet<StorageEntry> storage;
    static AssociativeSet<StorageEntry>* storagePtr;
    uint64_t acc_id = 1;

     class PTag : public TaggedEntry
    {
        public:
            bool used;
    };

    AssociativeSet<StorageEntry> remapTable;
    AssociativeSet<PTag> prefetchTable;

        void notifyFill(const PacketPtr &pkt) override;
        void notifyEvict(const BaseCache::DataUpdate &info) override;
  public:
    TetrahedrangelPrefetcher(const TetrahedrangelPrefetcherParams &p);
    void calculatePrefetch(const PrefetchInfo &pfi,
                           std::vector<AddrPriority> &addresses) override;
    void sizer(Addr,Addr,bool);
    void remap(Addr pc, Addr addr, bool, std::vector<AddrPriority> &addresses, bool,bool);
    void regular_pf(Addr pc, Addr addr, bool, std::vector<AddrPriority> &addresses, bool, bool);
    bool shouldPrefetch (Addr pc, Addr addr, std::vector<AddrPriority> &addresses);

    std::vector<RecordEntry> trigger;
    // RecordEntry trigger_stack[STACK_SIZE];
};


}  // namespace prefetch
}  // namespace gem5

#endif  // GEM5_SMS_HH
