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
  protected:
    uint32_t extractSet(const Addr addr) const override;
    Addr extractTag(const Addr addr) const override;

  public:
    TriageHashedSetAssociative(
        const TriageHashedSetAssociativeParams &p)
      : SetAssociative(p)
    {
    }
    ~TriageHashedSetAssociative() = default;
};


class Triage : public Queued
{

    /** Number of maximum prefetches requests created when predicting */
   // const unsigned degree; //TODO: currently ignored

    /**
     * Training Unit Entry datatype, it holds the last accessed address and
     * its secure flag
     */

    BaseTags* cachetags;
    const unsigned cacheDelay;


    const int max_size;
    const int size_increment;
    int64_t global_timestamp;
    int current_size;
    int target_size;
    const int historyLineAssoc;
    const int maxLineAssoc;

    bloom bl;

    struct TrainingUnitEntry : public TaggedEntry
    {
        Addr lastAddress;

        TrainingUnitEntry() : lastAddress(0)
        {}

        void
        invalidate() override
        {
        	TaggedEntry::invalidate();
                lastAddress = 0;
        }
    };
    /** Map of PCs to Training unit entries */
    AssociativeSet<TrainingUnitEntry> trainingUnit;

    /** Address Mapping entry, holds an address and a confidence counter */
    struct AddressMapping : public TaggedEntry
    {
        Addr address;
        bool confident;
        AddressMapping() : address(0), confident(false)
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
    AssociativeSet<AddressMapping> addressMappingCache;

    AddressMapping* getHistoryEntry(Addr index, bool is_secure, bool replace, bool readonly);

  public:
    Triage(const TriagePrefetcherParams &p);
    ~Triage() = default;

    void calculatePrefetch(const PrefetchInfo &pfi,
                           std::vector<AddrPriority> &addresses) override;
};

} // namespace prefetch
} // namespace gem5

#endif // __MEM_CACHE_PREFETCH_TRIAGE_HH__
