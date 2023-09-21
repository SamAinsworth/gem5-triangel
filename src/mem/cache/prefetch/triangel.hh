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

#include "params/TriangelHashedSetAssociative.hh"
#include "params/LookupHashedSetAssociative.hh"

#include "bloom.h"



namespace gem5
{

struct TriangelPrefetcherParams;

GEM5_DEPRECATED_NAMESPACE(Prefetcher, prefetch);
namespace prefetch
{

/**
 * Override the default set associative to apply a specific hash function
 * when extracting a set.
 */
class TriangelHashedSetAssociative : public SetAssociative
{
  protected:
    uint32_t extractSet(const Addr addr) const override;
    Addr extractTag(const Addr addr) const override;

  public:
    TriangelHashedSetAssociative(
        const TriangelHashedSetAssociativeParams &p)
      : SetAssociative(p)
    {
    }
    ~TriangelHashedSetAssociative() = default;
};

class LookupHashedSetAssociative : public SetAssociative
{
  protected:
    uint32_t extractSet(const Addr addr) const override;
    Addr extractTag(const Addr addr) const override;

  public:
    LookupHashedSetAssociative(
        const LookupHashedSetAssociativeParams &p)
      : SetAssociative(p)
    {
    }
    ~LookupHashedSetAssociative() = default;
};


class Triangel : public Queued
{

    /** Number of maximum prefetches requests created when predicting */
    const unsigned degree;

    /**
     * Training Unit Entry datatype, it holds the last accessed address and
     * its secure flag
     */

    BaseTags* cachetags;
    const unsigned cacheDelay;
    
    BaseTags* owntags;

    bool randomChance(int r, int s);
    const bool aggressive;
    const int max_size;
    const int size_increment;
    int64_t global_timestamp;
    int64_t reuse_timer;
    uint64_t lowest_blocked_entry;
    int current_size;
    int target_size;
    const int historyLineAssoc;
    const int maxLineAssoc;
    int sum_deviation;
    int paths;
    SatCounter8 historyNonHistory;

    bloom bl;

    struct TrainingUnitEntry : public TaggedEntry
    {
        Addr lastAddress;
        Addr lastLastAddress;
        int64_t local_timestamp;
        int reuseDistance;
        bool reuseSet;
        int64_t globalReuseDistance;
        int deviation;
        SatCounter8  reuseConfidence;
        SatCounter8  historyConfidence;
        SatCounter8 replaceRate;
        bool lastAddressSecure;
        bool lastLastAddressSecure;
        bool historied_this_round;
        bool currently_blocking;
        bool currently_twodist_pf;
        bool was_twodist_pf;



        TrainingUnitEntry() : lastAddress(0), lastLastAddress(0), local_timestamp(0), reuseDistance(0), reuseSet(false), globalReuseDistance(-1), deviation(0), reuseConfidence(4,8), historyConfidence(4,8), replaceRate(4,8), lastAddressSecure(false), lastLastAddressSecure(false),historied_this_round(false),currently_blocking(false)
        {}

        void
        invalidate() override
        {
        	TaggedEntry::invalidate();
                lastAddress = 0;
                lastLastAddress = 0;
                local_timestamp=0;
                reuseDistance = 0;
                reuseSet = false;
                globalReuseDistance = -1;
                reuseConfidence.reset();
                historyConfidence.reset();
                replaceRate.reset();
                historied_this_round = false;
                currently_blocking = false;
                currently_twodist_pf = false;
                was_twodist_pf = false;
        }
    };
    /** Map of PCs to Training unit entries */
    AssociativeSet<TrainingUnitEntry> trainingUnit;

    /** Address Mapping entry, holds an address and a confidence counter */
    struct AddressMapping : public TaggedEntry
    {
        Addr address;
        bool confident;
        Cycles cycle_issued; // only for prefetched cache and only in simulation
        AddressMapping() : address(0), confident(false), cycle_issued(0)
        {}


        void
        invalidate() override
        {
                    TaggedEntry::invalidate();
                address = 0;
                confident = false;
                cycle_issued=Cycles(0);
        }
    };
    
    struct LookupMapping : public TaggedEntry
    {
        Addr address;
        LookupMapping() : address(0)
        {}


        void
        invalidate() override
        {
                    TaggedEntry::invalidate();
                address = 0;
        }
    };

    /** Sample unit entry, tagged by data address, stores PC, timestamp, next element **/
    struct SampleEntry : public TaggedEntry
    {
    	Addr pc;
    	bool reused;
    	uint64_t local_timestamp;
    	uint64_t globalReuseDistance;
    	Addr last;

    	SampleEntry() : pc(0), reused(false), local_timestamp(0), globalReuseDistance(0), last(0)
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
    AssociativeSet<SampleEntry> sampleUnit;

    /** Test pf entry, tagged by data address**/
    struct TestEntry: public TaggedEntry
    {
    	Addr pc;
    	bool used;
    };
    AssociativeSet<TestEntry> testUnit;


    /** History mappings table */
    AssociativeSet<AddressMapping> addressMappingCache;
    
    
    AssociativeSet<LookupMapping> lookupCache;

    AssociativeSet<AddressMapping> prefetchedCache;
    bool lastAccessFromPFCache;

    AddressMapping* getHistoryEntry(Addr index, bool is_secure, bool replace, bool readonly);

  public:
    Triangel(const TriangelPrefetcherParams &p);
    ~Triangel() = default;

    void calculatePrefetch(const PrefetchInfo &pfi,
                           std::vector<AddrPriority> &addresses) override;
};

} // namespace prefetch
} // namespace gem5

#endif // __MEM_CACHE_PREFETCH_TRIANGEL_HH__
