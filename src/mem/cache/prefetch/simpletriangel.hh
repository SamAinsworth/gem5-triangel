/**
 * @file
 * Describes a history prefetcher.
 */

#ifndef __MEM_CACHE_PREFETCH_TRIANGEL_HH__
#define __MEM_CACHE_PREFETCH_TRIANGEL_HH__

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

#include "params/SimpleTriangelHashedSetAssociative.hh"

#include "bloom.h"



namespace gem5
{

struct SimpleTriangelPrefetcherParams;

GEM5_DEPRECATED_NAMESPACE(Prefetcher, prefetch);
namespace prefetch
{

/**
 * Override the default set associative to apply a specific hash function
 * when extracting a set.
 */
class SimpleTriangelHashedSetAssociative : public SetAssociative
{
  public:
    uint32_t extractSet(const Addr addr) const override;
    Addr extractTag(const Addr addr) const override;

  public:
    int ways;
    int max_ways;
    SimpleTriangelHashedSetAssociative(
        const SimpleTriangelHashedSetAssociativeParams &p)
      : SetAssociative(p), ways(0),max_ways(8)
    {
    }
    ~SimpleTriangelHashedSetAssociative() = default;
};



class SimpleTriangel : public Queued
{

    /** Number of maximum prefetches requests created when predicting */
    const unsigned degree;

    /**
     * Training Unit Entry datatype, it holds the last accessed address and
     * its secure flag
     */

    BaseTags* cachetags;
    const unsigned cacheDelay;
    const bool should_lookahead;
    const bool should_rearrange;
    
    const bool use_scs;

    
    BaseTags* sctags;

    bool randomChance(int r, int s);
    const int max_size;
    const int size_increment;
    static int64_t global_timestamp;
    uint64_t lowest_blocked_entry;
    static int current_size;
    static int target_size;
    const int maxWays;    
    
    std::vector<int> way_idx;
   

    struct TrainingUnitEntry : public TaggedEntry
    {
        Addr lastAddress;
        Addr lastLastAddress;
        int64_t local_timestamp;
        SatCounter8  reuseConfidence;
        SatCounter8  patternConfidence;
        SatCounter8 highHistoryConfidence;
        SatCounter8 replaceRate;
        SatCounter8 hawkConfidence;
        bool lastAddressSecure;
        bool lastLastAddressSecure;
        bool currently_twodist_pf;



        TrainingUnitEntry() : lastAddress(0), lastLastAddress(0), local_timestamp(0),reuseConfidence(4,8), patternConfidence(4,8), highHistoryConfidence(4,8), replaceRate(4,8), hawkConfidence(4,8), lastAddressSecure(false), lastLastAddressSecure(false),currently_twodist_pf(false)
        {}

        void
        invalidate() override
        {
        	TaggedEntry::invalidate();
                lastAddress = 0;
                lastLastAddress = 0;
                local_timestamp=0;
                reuseConfidence.reset();
                patternConfidence.reset();
                replaceRate.reset();
                currently_twodist_pf = false;
        }
    };
    /** Map of PCs to Training unit entries */
    AssociativeSet<TrainingUnitEntry> trainingUnit;
    

   
  static std::vector<uint32_t> setPrefetch; 
 
public:
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
  	
  	int checkAndInsert(Addr addr, bool should_pf) {
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
  			if(should_pf && addr == temporalAddrs[x]) {
  				
  				
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
  			temporalAddrs[idx]=addr;
  			temporalAddrTick[idx]=curTick();
  		}  
  		return ret;		
  	}
  
  };
  SizeDuel sizeDuels[256];
  static SizeDuel* sizeDuelPtr;


    /** Address Mapping entry, holds an address and a confidence counter */
    struct MarkovMapping : public TaggedEntry
    {
      	Addr index; //Just for maintaining HawkEye easily. Not real.
        Addr address;
        bool confident;
        Cycles cycle_issued; // only for prefetched cache and only in simulation
        MarkovMapping() : index(0), address(0), confident(false), cycle_issued(0)
        {}


        void
        invalidate() override
        {
                TaggedEntry::invalidate();
                address = 0;
                index = 0;
                confident = false;
                cycle_issued=Cycles(0);
        }
    };
    

    /** Sample unit entry, tagged by data address, stores PC, timestamp, next element **/
    struct SampleEntry : public TaggedEntry
    {
    	Addr pc;
    	bool reused;
    	uint64_t local_timestamp;
    	Addr last;

    	SampleEntry() : pc(0), reused(false), local_timestamp(0), last(0)
        {}

        void
        invalidate() override
        {
            TaggedEntry::invalidate();
        }

        void clear() {
                pc = 0;
                reused = false;
                local_timestamp=0;
                last = 0;
        }
    };
    AssociativeSet<SampleEntry> historySampler;

    /** Test pf entry, tagged by data address**/
    struct SecondChanceEntry: public TaggedEntry
    {
    	Addr pc;
    	uint64_t local_timestamp;
    	bool used;
    };
    AssociativeSet<SecondChanceEntry> secondChanceUnit;


    /** History mappings table */
    AssociativeSet<MarkovMapping> markovTable;
    static AssociativeSet<MarkovMapping>* markovTablePtr;
    

    AssociativeSet<MarkovMapping> metadataReuseBuffer;
    bool lastAccessFromPFCache;

    MarkovMapping* getHistoryEntry(Addr index, bool is_secure, bool replace, bool readonly, bool clearing, bool hawk);

  public:
    SimpleTriangel(const SimpleTriangelPrefetcherParams &p);
    ~SimpleTriangel() = default;

    void calculatePrefetch(const PrefetchInfo &pfi,
                           std::vector<AddrPriority> &addresses) override;
};

} // namespace prefetch
} // namespace gem5

#endif // __MEM_CACHE_PREFETCH_TRIANGEL_HH__
