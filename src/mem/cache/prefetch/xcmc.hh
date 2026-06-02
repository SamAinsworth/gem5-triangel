#ifndef __MEM_CACHE_PREFETCH_CMC_HH__
#define __MEM_CACHE_PREFETCH_CMC_HH__
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
#include "base/random.hh"
#include "params/XCMCPF.hh"

namespace gem5
{
struct XCMCPFParams;
GEM5_DEPRECATED_NAMESPACE(Prefetcher, prefetch);

namespace prefetch
{


class XCMCPF : public Queued
{
    BaseTags* cachetags;
    const bool fix_indexing;
  public:

         static int current_ways;
        static int target_ways;
      static int64_t global_timestamp;
    static std::vector<uint32_t> setPrefetch;
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
                    if(should_pf && ((addr)^(pf_offset<<8)) == temporalAddrs[x]) {


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
                    temporalAddrs[idx]=((addr)^(pf_offset<<8));
  			temporalAddrTick[idx]=curTick();
  		}
  		return ret;
  	}

  };
  SizeDuel sizeDuels[64];
  static SizeDuel* sizeDuelPtr;


    class StorageEntry;
    class RecordEntry
    {
        public:
            Addr pc;
            Addr addr;
            bool is_secure;
            RecordEntry(Addr p, Addr a, bool s)
                : pc(p), addr(a), is_secure(s) {}
            RecordEntry() : addr(0), is_secure(true) {}
    };
    class Recorder
    {
        public:
            std::vector<Addr> entries;
            int index;
            Recorder() : entries(), index(0) {}
            bool entry_empty() { return entries.empty(); }
            Addr get_base_addr() { return entries[0]; }

            bool train_entry(Addr, bool, bool*);
            void reset();
            const int nr_entry = 16;
        private:
    };

    class StorageEntry : public TaggedEntry
    {
        public:
            std::vector<Addr> addresses;
            int refcnt;
            uint64_t id;
            void invalidate() override;
    };
  private:
    Recorder *recorder;
    AssociativeSet<StorageEntry> storage;
        static AssociativeSet<StorageEntry>* storagePtr;

    uint64_t acc_id = 1;

  public:
    XCMCPF(const XCMCPFParams &p);
    void calculatePrefetch(const PrefetchInfo &pfi,
                           std::vector<AddrPriority> &addresses) override;
    void sizer(Addr,Addr,bool);
  private:
    uint64_t hash(Addr addr, Addr pc) {
        return addr ^ (pc<<8);
    }

    static const int STACK_SIZE = 4;
    std::deque<RecordEntry> trigger;
    // RecordEntry trigger_stack[STACK_SIZE];
};


}  // namespace prefetch
}  // namespace gem5

#endif  // GEM5_SMS_HH
