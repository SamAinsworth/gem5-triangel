
//Adapted from https://github.com/OpenXiangShan/GEM5/blob/xs-dev/src/mem/cache/prefetch
#include "mem/cache/prefetch/tetrahedrangel.hh"

#include "debug/HWPrefetch.hh"
#include "mem/cache/prefetch/associative_set_impl.hh"
#include "params/TetrahedrangelPrefetcher.hh"

namespace gem5
{
namespace prefetch
{
int64_t TetrahedrangelPrefetcher::global_timestamp=0;
int TetrahedrangelPrefetcher::target_ways=0;
int TetrahedrangelPrefetcher::current_ways=0;
int TetrahedrangelPrefetcher::attached_cores=0;

TetrahedrangelPrefetcher::SizeDuel* TetrahedrangelPrefetcher::sizeDuelPtr=nullptr;
AssociativeSet<TetrahedrangelPrefetcher::StorageEntry>* TetrahedrangelPrefetcher::storagePtr=nullptr;

std::vector<uint32_t> TetrahedrangelPrefetcher::setPrefetch(17,0);

TetrahedrangelPrefetcher::TetrahedrangelPrefetcher(const TetrahedrangelPrefetcherParams &p)
    : Queued(p),
      cachetags(p.cachetags),
      sctags(p.sctags),
      always_prefetch(p.always_prefetch),
      pc_training(p.pc_training),
      use_remap(p.use_remap),
      remap_scatter(p.use_scatter),
      use_sleep(p.use_sleep),
      ignore_ptag(p.ignore_ptag),
      aggressive(p.aggressive),
      no_diffPC(p.no_diffPC),
      no_samePC(p.no_samePC),
      lastTimeliness(0),
      lastAccurate(0),
      lastAccInacc(0),
      current_scatter(4),
      current_degree(4),
      duelaccurate(0),
      dueltotal(0),
      sleepmode(false),
      reuseBlocked(0),
      patternBlocked(0),
      passed(0),
      samepf(0),
      uncategorised(0),
      counterUnit(p.counter_unit_entries,CounterEntry()),
      samplerTrainer(16,SamplerTrainer()),
      secondChanceUnit(p.secondchance_assoc,
                       p.secondchance_entries,
                       p.secondchance_indexing_policy,
                       p.secondchance_replacement_policy),
      ghostMarkovTable(p.storage_assoc/2,
                       p.storage_entries*8,
                       p.ghost_address_map_cache_indexing_policy,
                       p.ghost_address_map_cache_replacement_policy,
                       GhostMarkovMapping()),
      remapTable(p.remap_assoc,
                 p.remap_entries,
                 p.remap_indexing_policy,
                 p.remap_replacement_policy),
      prefetchTable(p.demap_assoc,
                    p.demap_entries,
                    p.demap_indexing_policy,
                    p.demap_replacement_policy),
      recorder (p.training_table_size,Recorder()),
      storage(p.storage_assoc, p.storage_entries, p.storage_indexing_policy,
              p.storage_replacement_policy, StorageEntry()),

      trigger(p.training_table_size,RecordEntry())
{
    setPrefetch.resize(p.storage_assoc+1,0);
    current_ways = 0;
    target_ways=0;

    storagePtr = &storage;
    sizeDuelPtr= sizeDuels;
    attached_cores++;
    printf("attached cores %d\n",attached_cores);
    for(int x=0; x<64; x++) {
        sizeDuelPtr[x].reset((p.storage_entries/p.storage_assoc)-1,16,cachetags->getWayAllocationMax());
    }
    for(int x=0; x<64; x++) {
        simplerSamplerDuelledSets[x] = random_mt.random<uint64_t>(0,ghostMarkovTable.indexingPolicy->numSets-1);
    }
}

void
TetrahedrangelPrefetcher::sizer(Addr block_addr, Addr pc, bool should_pf) {
    //This is  a Triangel-based sizer to borrow space from the cache.
    //Dueller from Triangel
    for(int x=0; x<64; x++) {
        //Here we update the size duellers, to work out for each cache set whether it is better to be markov table or L3 cache.
        int res =    	sizeDuelPtr[x].checkAndInsert(block_addr,should_pf, pc);
        if(res==0)continue;
        const int ratioNumer=2;
        const int ratioDenom= aggressive? 2 :4;//look_check && should_pf && countentry->highPatternConfidence >=highUpperHistory? 2 : 4;
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

        int64_t current_used = usefulPrefetches - duelaccurate;
        int64_t current_total = usefulPrefetches + uselessPrefetches - dueltotal;


        bool wasSleep = sleepmode;

        if(10* current_used < 6*current_total && target_ways > 1 && current_total > 10 && use_sleep) {
			//discount the Triangel Sampler's PFs, which may be more accurate.
			sleepmode=true;
			printf("sleep mode, tick %ld, %ld out of %ld accurate \n",curTick(), current_used, current_total);
		} else sleepmode=false;

		duelaccurate = usefulPrefetches;
		dueltotal = uselessPrefetches + usefulPrefetches;

        if(target_ways != current_ways && targetscore>currentscore) {
            current_ways = target_ways;
            printf("size: %d, tick %ld \n",current_ways,curTick());
            for(int x=0; x<setPrefetch.size(); x++) {
                printf("%d: score %ld\n", x, setPrefetch[x]);
            }
            assert(current_ways >= 0);

            //attached cores thing a bit of a hack. Should work out if all cores are in sleep mode and act accordingly -- on a timestamp per-core.

			if(wasSleep && !sleepmode && attached_cores<2) storagePtr->setWayAllocationMax(0); //To empty
            storagePtr->setWayAllocationMax(current_ways);
            if(!sleepmode || attached_cores>1) cachetags->setWayAllocationMax(setPrefetch.size()-1-current_ways);
            else cachetags->setWayAllocationMax(setPrefetch.size()-1);
        }
        printf("End of epoch:\n");
        printf("passed: %ld, reuseblocked: %ld, patternblocked %ld, same_passed %ld uncat %ld\n",passed, reuseBlocked, patternBlocked, samepf, uncategorised);
        passed=0;
        reuseBlocked=0;
        patternBlocked=0;
        samepf=0;
        global_timestamp=0;
        uncategorised=0;
        for(int x=0; x<setPrefetch.size(); x++) {
            setPrefetch[x]=0;
        }

    }

}

void
TetrahedrangelPrefetcher::notifyFill(const PacketPtr &pkt)
{

			Addr addr = pkt->getAddr();
		Addr blockID = blockIndex(addr);

		SecondChanceEntry *sentry =
        secondChanceUnit.findEntry(blockID, false);

		if(sentry) {
			sentry->in_cache=true;
		}

}

void
TetrahedrangelPrefetcher::notifyEvict(const BaseCache::DataUpdate &info)
{
    Addr addr = info.addr;
    Addr blockID = blockIndex(addr);

   SecondChanceEntry *sentry =
        secondChanceUnit.findEntry(blockID, false);

		if(sentry) {

			if(sentry->in_cache) {
				            sentry->in_cache=false;
				dueltotal++; //to cancel out for sampler
				if(!sentry->used) {
					CounterEntry *tcentry = &counterUnit[(sentry->pc ^ (sentry->pc/counterUnit.size()))%counterUnit.size()];
					for(int x=0; x<(aggressive? 1 :2); x++) {
						if(sentry->samePC) tcentry->samePCPatternConf--;
					}
					for(int x=0; x<(2); x++) {
						tcentry->patternConfidence--;
					}
				}
				sentry->pc=-1;
				sentry->invalidate();
			}
		}
}

bool
TetrahedrangelPrefetcher::shouldPrefetch (Addr pc, Addr addr, std::vector<AddrPriority> &addresses) {

    const int upperHistory=7;
    const int upperReuse=7;

    int cpcindex = (pc ^ (pc/counterUnit.size()))%counterUnit.size();
    CounterEntry *countentry = &counterUnit[cpcindex];


    //bool should_pf = (countentry->reuseConfidence > upperReuse) && (countentry->patternConfidence > upperHistory);
    bool should_pf = always_prefetch || ((countentry->reuseConfidence > upperReuse) && (countentry->patternConfidence > upperHistory) && !no_diffPC) ||
                     ((countentry->samePCPatternConf > upperHistory) && !no_samePC &&(countentry->reuseConfidence > upperReuse));
    // bool should_same_pf = should_pf && !always_prefetch && !((countentry->reuseConfidence > upperReuse) && (countentry->patternConfidence > upperHistory));


   SecondChanceEntry *sentry =
        secondChanceUnit.findEntry(addr, false);

        if(sentry) {
            sentry->used=true;
            CounterEntry *tcentry = &counterUnit[(sentry->pc ^ (sentry->pc/counterUnit.size()))%counterUnit.size()];
            tcentry->patternConfidence++;
            if(sentry->samePC) tcentry->samePCPatternConf++;
            sentry->pc=-1;
            duelaccurate++; //to cancel out for sampler
            dueltotal++;
            sentry->invalidate();
		}

    Addr lastAddr = 0;
    bool seenPCWaiting=false;
    for(int x=0; x<samplerTrainer.size(); x++) {
        if(samplerTrainer[x].pc == pc) {
            if(samplerTrainer[x].count==0) {
                lastAddr = samplerTrainer[x].lastAddr;
                samplerTrainer[x].lastAddr=0;
                samplerTrainer[x].pc=-1;
                samplerTrainer[x].tick=0;
            } else {
                samplerTrainer[x].count--;
                seenPCWaiting=true;
            }
        }
    }


    SetAssociative* ghsa = dynamic_cast<SetAssociative*>(ghostMarkovTable.indexingPolicy);
    assert(ghsa != nullptr);
    int idx=-1;
    bool fullSample = countentry->reuseConfidence>0 && countentry->patternConfidence>0 && countentry->patternConfidence<15;
    int div = fullSample? 1 : countentry->reuseConfidence>0 ? 2 : 4;
    for(int x=0; x<64; x++) {
        if(simplerSamplerDuelledSets[x]==ghsa->extractSet(addr)) {
            idx=x;
            break;
        }
    }


    if(idx!=-1 && !seenPCWaiting) {
        int target=-1;
        uint64_t minTick=-1;
        for(int x=0; x<samplerTrainer.size(); x++) {
            if(samplerTrainer[x].pc==-1) {
                target=x;
                break;
            }
            if(samplerTrainer[x].tick < minTick) {
                minTick=samplerTrainer[x].tick;
                target=x;
            }
        }
        assert(target>=0);
        if(samplerTrainer[target].pc!=-1)printf("Missed sample for %ld\n",samplerTrainer[target].pc);
        samplerTrainer[target].pc=pc;
        samplerTrainer[target].lastAddr=addr;
        samplerTrainer[target].tick=curTick();
        samplerTrainer[target].count=15;
    }

    if(idx!=-1) {
        GhostMarkovMapping* sample = getSimplerSamplerEntry(addr, false, false, false);

        if(sample != nullptr) {
            SecondChanceEntry* tentry = secondChanceUnit.findEntry(sample->address, false);
            if(tentry==nullptr) {
                tentry = secondChanceUnit.findVictim(sample->address);
                if(!tentry->used && tentry->addr !=0 && tentry->pc !=-1)  {

                    //printf("Evicting unused Addr %ld PC %ld\n",tentry->addr, tentry->pc);
                }
                secondChanceUnit.insertEntry(sample->address, false, tentry);
				tentry->in_cache = false;
            } else {
				secondChanceUnit.accessEntry(tentry);
			}
            //printf("Prefetch attempt %ld at PC %ld\n",sample->address, pc);
            tentry->pc = pc;
            tentry->samePC = pc == sample->pc;
            tentry->addr=sample->address;
            tentry->used = false;

            addresses.push_back(AddrPriority(sample->address << lBlkSize, 0));
        }
    }


    if(lastAddr !=0) {
        bool fill = (ghsa->extractTag(lastAddr)&(div-1))==(ghsa->extractSet(lastAddr)&(div-1));
        GhostMarkovMapping* sample = getSimplerSamplerEntry(lastAddr, false, false, fill);

        if(sample != nullptr) {
            int scpcindex = (sample->pc ^ (sample->pc/counterUnit.size()))%counterUnit.size();
            CounterEntry *scountentry = &counterUnit[scpcindex];
            sample->used=true;
            //printf("Reuse %ld at PC %ld\n",sample->address, sample->pc);
            scountentry->reuseConfidence++;
            scountentry->reuseConfidence++;
            if((sample->pc==pc || (global_timestamp&1))) { //contrived, to stop same-pc starvation.
                sample->address=addr;
                sample->pc=pc;
            }

        } else if(fill) {
            //replace
            sample = getSimplerSamplerEntry(lastAddr, false, true,true);
            assert(sample != nullptr);
            if(sample->pc !=-1) {
                CounterEntry *sentry = &counterUnit[(sample->pc ^ (sample->pc/counterUnit.size()))%counterUnit.size()];
                if(sentry != nullptr) {
                    if(!sample->used) {
                        sentry->reuseConfidence--;

                    }
                }
            }
            sample->used=false;
            sample->pc=pc;
            sample->index=lastAddr;
            sample->address=addr;
        }
    }

    return should_pf;

}


TetrahedrangelPrefetcher::GhostMarkovMapping*
TetrahedrangelPrefetcher::getSimplerSamplerEntry(Addr paddr, bool is_secure, bool add, bool touch)
{

    GhostMarkovMapping *ps_entry =
        ghostMarkovTable.findEntry(paddr, false);
    if (ps_entry != nullptr) {
        // A PS-AMC line already exists
        if(touch) ghostMarkovTable.accessEntry(ps_entry);
    } else {
        if(!add) return nullptr;
        ps_entry = ghostMarkovTable.findVictim(paddr);
        assert(ps_entry != nullptr);
        assert(!ps_entry->isValid());
        ghostMarkovTable.insertEntry(paddr, false, ps_entry);
    }

    return ps_entry;
}

void TetrahedrangelPrefetcher::remap(Addr block_addr, Addr pc, bool same_pf, std::vector<AddrPriority> &addresses, bool is_secure, bool miss) {


    const int offset = current_degree;
    const int degree = current_degree;

    Addr lookup_addr =  hash(block_addr, same_pf ? pc : 0);
    Addr lookup_addr_alt = hash(block_addr, same_pf ? 0 : pc);


    PTag* ptag1 = prefetchTable.findEntry(block_addr,false);
    if(ptag1) {
        if(ptag1->used) lookup_addr=0;
        ptag1->used = true;
    } else {
        if(!miss && !ignore_ptag) {
            lookup_addr=0;
        }
    }


    StorageEntry *remap_entry = remapTable.findEntry(block_addr, false);
    if (remap_entry && remap_entry->valid) {
        assert(remap_entry->addresses.size()<=8);

        for (auto addre: remap_entry->addresses) {
            addresses.push_back(AddrPriority(addre, 0));
        }

        remap_entry->invalidate();
        remap_entry->valid=false;
    }
    // Prefetch: check if there is a match

		if(sleepmode && (lookup_addr & 255) != 0) return;
        StorageEntry *match_entry = lookup_addr == 0 ? NULL: storagePtr->findEntry(lookup_addr, false);
        if (match_entry) {
            //storagePtr->accessEntry(match_entry);
        }
        if(!match_entry && lookup_addr !=0) {
            match_entry = storagePtr->findEntry(lookup_addr_alt, false);
        }
        if((match_entry || !miss || ignore_ptag) && lookup_addr !=0) prefetchStats.metadataAccesses++;
        if (match_entry) {
            if(!ptag1 && lookup_addr !=0) {
                ptag1 = prefetchTable.findVictim(block_addr);
                prefetchTable.insertEntry(block_addr,
                                          false,
                                          ptag1
                                         );
                ptag1->used = true;
            }
            // prefetch on cache miss or on prefetch hit
            DPRINTF(HWPrefetch, "Storage hit, trigger pc: %lx, addr: %lx\n",
                    pc, block_addr);
            //printf("=== Storage hit, trigger addr: %lx\n", block_addr);


            int iter = 0;
            for (auto addr: match_entry->addresses) {
                if(iter<offset)  {
					 addresses.push_back(AddrPriority(addr, 25));
				}
				for(int x=0; x<64; x++) {
					//Very fake... really you'd look up the index of anything matching except in upper bits
					if(storagePtr->findEntry(hash(blockIndex(addr),x),false)) {
						PTag *pentry = prefetchTable.findEntry(blockIndex(addr), false);
						if(!pentry) {

							pentry = prefetchTable.findVictim(blockIndex(addr));
							prefetchTable.insertEntry(blockIndex(addr),
													  false,
													  pentry
													 );
						}
						pentry->used = false;
					}
				}
                iter++;
            }

            for(int x=0; x<match_entry->addresses.size()-offset; x+=degree)
            {
                StorageEntry *entry = remapTable.findEntry(blockIndex(match_entry->addresses[x]), false);
                if (!entry) {
                    entry = remapTable.findVictim(blockIndex(match_entry->addresses[x]));
                    remapTable.insertEntry(
                        blockIndex(match_entry->addresses[x]),
                        false,
                        entry
                    );
                }

                std::vector<Addr> slice;

                for(int y=x+offset; y<x+offset+degree && y<match_entry->addresses.size(); y++) {
                    slice.push_back(match_entry->addresses[y]);
                }
                entry->addresses = slice;
                entry->valid=true;
                assert(entry->addresses.size()<=degree);



            }
        }

}


void TetrahedrangelPrefetcher::regular_pf(Addr block_addr, Addr pc, bool same_pf, std::vector<AddrPriority> &addresses, bool is_secure, bool miss) {

    Addr lookup_addr = /*pfi.isCacheMiss() ? */same_pf ? hash(block_addr, pc) : block_addr/* : 0*/;
		if(sleepmode && (lookup_addr & 255) != 0) return;

    PTag* ptag1 = prefetchTable.findEntry(block_addr,false);

    if(ptag1) {
        ptag1->used = true;

    }

    if(!miss && !ptag1 && !ignore_ptag) {
		lookup_addr = 0;
	}


    // Prefetch: check if there is a match
    StorageEntry *match_entry = lookup_addr == 0 ? NULL: storagePtr->findEntry(lookup_addr, false);
    if (match_entry) {
        //storagePtr->accessEntry(match_entry);
    }
    if(!match_entry) match_entry = lookup_addr == 0 ? NULL: storagePtr->findEntry(same_pf ?block_addr :hash(block_addr, pc), false);
    if((match_entry || !miss || ignore_ptag) && lookup_addr !=0) prefetchStats.metadataAccesses++;
    if (match_entry) {
        if(!ptag1) {
            ptag1 = prefetchTable.findVictim(block_addr);
            prefetchTable.insertEntry(block_addr,
                                      false,
                                      ptag1
                                     );
            ptag1->used = true;
        }

        // prefetch on cache miss or on prefetch hit
        DPRINTF(HWPrefetch, "Storage hit, trigger pc: %lx, addr: %lx\n",
                pc, block_addr);
        //printf("=== Storage hit, trigger addr: %lx\n", block_addr);

        for (auto addr: match_entry->addresses) {
            addresses.push_back(AddrPriority(addr, 25));
				for(int x=0; x<64; x++) {
					//Very fake... really you'd look up the index of anything matching except in upper bits
					if(storagePtr->findEntry(hash(blockIndex(addr),x),false)) {
						PTag *pentry = prefetchTable.findEntry(blockIndex(addr), false);
						if(!pentry) {

							pentry = prefetchTable.findVictim(blockIndex(addr));
							prefetchTable.insertEntry(blockIndex(addr),
													  false,
													  pentry
													 );
						}
						pentry->used = false;
					}
				}
        }

   /*          PTag *pentry = prefetchTable.findEntry(match_entry->next, false);
            if(!pentry) {

                pentry = prefetchTable.findVictim(match_entry->next);
                prefetchTable.insertEntry(match_entry->next,
                                          false,
                                          pentry
                                         );
            }
            pentry->used = false;
  */
    }
}



void
TetrahedrangelPrefetcher::calculatePrefetch(const PrefetchInfo &pfi,
                                 std::vector<AddrPriority> &addresses)
{


    //PC isolation here lowers coverage...
    Addr pc = pfi.hasPC() ? pfi.getPC() : 0;




        uint64_t timely = usefulPrefetches - lastTimeliness;
        uint64_t total = usefulPrefetches+latePrefetches-lastAccurate;

        uint64_t plusinaccurate = usefulPrefetches+latePrefetches+uselessPrefetches-lastAccInacc;

        int64_t current_tot = 950*total;
        int64_t upper_tot = 980*total;
        int64_t current_time = 1000*timely;

        prefetchStats.stateNotifCount[current_ways]++;

        if(usefulPrefetches+latePrefetches-lastAccurate >= 10000 || (usefulPrefetches+latePrefetches-lastAccurate >=1000 && current_time < current_tot && current_degree < 8) || plusinaccurate-total >= 1000) {

			/*if(current_degree<4 && total*100 > 95*plusinaccurate) current_degree++;
        	else
        	*/
        	if((current_time >= upper_tot || total*100 < 66*plusinaccurate) && current_degree > 4 && use_remap)  {

        			current_degree--;
        			printf("degree:  %d  %ld vs %ld, accuracy %ld vs %ld \n",current_degree, timely, total, total, plusinaccurate);
        	} else if (current_time < current_tot && current_degree < 8 && total*100 > 85*plusinaccurate && use_remap) {
        		current_degree++;
        			printf("degree:  %d  %ld vs %ld, accuracy %ld vs %ld \n",current_degree, timely, total, total, plusinaccurate);
        	}


        			lastTimeliness = usefulPrefetches;
        	lastAccurate = usefulPrefetches+latePrefetches;
        	lastAccInacc = lastAccurate + uselessPrefetches;
        }




    Addr addr = pfi.getAddr();
    Addr block_addr = blockIndex(addr); // takes off 6 Least Significant Bits for cache line

    DPRINTF(HWPrefetch, "Tetrahedrangel train: pc: %lx, addr: %lx\n", pc, block_addr);
    global_timestamp++;



    if(pfi.isWrite())sizer(block_addr,pc,false); // sets size based on Triangel method



    if(pfi.isWrite()) return;
    // if(!pfi.hasPC()) return; // sampler assumes PC


    bool should_pf = always_prefetch || shouldPrefetch(pc,block_addr,addresses);
    int cpcindex = (pc ^ (pc/counterUnit.size()))%counterUnit.size();
    CounterEntry *countentry = &counterUnit[cpcindex];
    bool should_same_pf = should_pf /*&& !always_prefetch*/ && (no_diffPC || always_prefetch || !((countentry->reuseConfidence > 7) && (countentry->patternConfidence > 7)));

    if(should_pf) passed++;

    if(should_same_pf) samepf++;
    else if (countentry->reuseConfidence < 8) reuseBlocked++;
    else if (countentry->patternConfidence < 8) patternBlocked++;

    if(countentry->patternConfidence ==8 || countentry->reuseConfidence==8) uncategorised++;

    sizer(block_addr,should_same_pf?pc:0,should_pf); // sets size based on Triangel method

    if(!should_pf || current_ways==0) return;

    if(use_remap) remap(block_addr,pc,should_same_pf,addresses,false,pfi.isCacheMiss());
    else regular_pf(block_addr,pc,should_same_pf,addresses,false,pfi.isCacheMiss());


    // Prefetch: check if there is a match
    bool match_entry = prefetchTable.findEntry(block_addr,false) != nullptr;

    // Train: update temporal access chain
    bool finished = false;


    if(pc_training) {
		current_idx = -1;
	    uint64_t table_time = curTick()+1;
		for(int x=0; x<trigger.size(); x++) {
			if(trigger[x].pc==pc) {
				current_idx = x;
				break;
			}
			if(table_time > trigger[x].lastTick) {
				table_time = trigger[x].lastTick;
				current_idx = x;
			}
		}
		    trigger[current_idx].lastTick = curTick();
		}


    /* 1. Train trigger */
    //This will try to reuse an existing trigger that has been seen recently, should one exist.
    //Otherwise, it will find a new trigger.
    //Intuition is that if you didn't aggressively reuse triggers, you'd pollute your cache with an entry per address, which adds massive redundancy,
    //evicts other useful metadata, and will take ages to replace old, potentially wrong entries.
    bool train_trigger =
        (!trigger[current_idx].valid);// || match_entry) && trigger.size()<STACK_SIZE;
    bool do_training =
        trigger[current_idx].valid; //This is fixed: we should still train the Markov table on a newly seen trigger, as long as there is another trigger in the list already
    if (train_trigger) {
        //printf("train_trigger index: %d, addr: %lx\n",
        //        trigger.size(), block_addr);

        trigger[current_idx]= RecordEntry(pc, block_addr, should_same_pf, false,curTick());
    }

    /* 2. Train entry */
    if (do_training) {
        bool trained = recorder[current_idx].train_entry(addr, pc, should_same_pf, match_entry, current_degree, false, &finished);
        auto &trigger_head = trigger[current_idx];
        if (trained) {
            //printf("trained %x\n", block_addr);
        }
        if (finished) {
            //finished gets set once a coalesced set of 16 targets get stored in the training entry.
            //printf("trigger train finished, pc: %lx, addr: %lx\n",
            //     trigger_head.pc, trigger_head.addr);

            StorageEntry *entry = storagePtr->findEntry(hash(trigger_head.addr, trigger_head.should_xor? trigger_head.pc: 0), false);
            StorageEntry *alt_entry = storagePtr->findEntry(hash(trigger_head.addr, trigger_head.should_xor? 0:trigger_head.pc), false);


            if (entry) {
                storagePtr->accessEntry(entry); //do update replacement
                DPRINTF(HWPrefetch, "Tetrahedrangel: enter the same trigger, pc: %lx, addr: %lx\n",
                        trigger_head.pc, trigger_head.addr);
                entry->addresses = recorder[current_idx].entries;
                entry->next = blockIndex(recorder[current_idx].logAddr);
                entry->nextPC = recorder[current_idx].logPC;
                entry->should_xor = recorder[current_idx].should_xor;
                entry->valid = true;


                if(alt_entry) {
					storagePtr->accessEntry(entry);
					alt_entry->invalidate();
				}

            } else if(!sleepmode || (trigger_head.addr & 255) == 0){
				 if(alt_entry) {
					 alt_entry->invalidate();
				}
                entry = storagePtr->findVictim(hash(trigger_head.addr, trigger_head.should_xor? trigger_head.pc: 0));
                entry->addresses = recorder[current_idx].entries;
                entry->next = blockIndex(recorder[current_idx].logAddr);
                entry->nextPC = recorder[current_idx].logPC;
                entry->should_xor = recorder[current_idx].should_xor;
                entry->valid = true;


                storagePtr->insertEntry(
                    hash(trigger_head.addr, trigger_head.should_xor? trigger_head.pc: 0),
                    false,
                    entry
                );

                if(alt_entry) {
					storagePtr->accessEntry(entry);
				}
            }

            for (auto addr: recorder[current_idx].entries) {
                DPRINTF(HWPrefetch, "entry addr: 0x%lx\n",
                        addr);
            }
            prefetchStats.metadataAccesses++;

            //This is fixed: if we don't have this here, we end up with triggers and trainings as distinct,
            //meaning we never reach 100% coverage.
            //This makes the trigger the last addr of the previous group, provided no other trigger already
            // is prepared, because it was a recent reused addr.
            trigger[current_idx]=(RecordEntry(recorder[current_idx].logPC, blockIndex(recorder[current_idx].logAddr), recorder[current_idx].should_xor, false,curTick())); // recorder->entries[recorder->entries.size()/2] // block_addr
            recorder[current_idx].reset();

        }
    }

    if(!pc_training && use_remap && remap_scatter) current_idx = (current_idx+1)%current_scatter;
}


bool
TetrahedrangelPrefetcher::Recorder::train_entry(
    Addr addr, Addr pc, bool samePC, bool matchEntry, int current_degree,
    bool is_secure,
    bool *finished
) {

    entries.push_back(addr);
    index++;
    //There was an off-by-one error here: it stored 17 entries



    if (((logAddr == 0 && index == nr_entry-current_degree) || matchEntry) && (!matched_entry || (matchEntry && triangelhash2(global_timestamp,2)))) {
        logPC=pc;
        logAddr=addr;
        should_xor = samePC;
    }
    if(matchEntry) matched_entry=true;

    if (index >= nr_entry) {
        // entry full
        *finished = true;
    }

    return true;
}



void
TetrahedrangelPrefetcher::Recorder::reset() {
    index = 0;
    logPC=0;
    logAddr=0;
    matched_entry = false;
    entries.clear();
}

void
TetrahedrangelPrefetcher::StorageEntry::invalidate() {
    addresses.clear();
    next=0;
    valid = false;

    TaggedEntry::invalidate();
}

}  // prefetch
}  // gem5
