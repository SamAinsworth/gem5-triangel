# Copyright (c) 2012-2013, 2015-2016 ARM Limited
# Copyright (c) 2020 Barkhausen Institut
# All rights reserved
#
# The license below extends only to copyright in the software and shall
# not be construed as granting a license to any other intellectual
# property including but not limited to intellectual property relating
# to a hardware implementation of the functionality of the software
# licensed hereunder.  You may use the software subject to the license
# terms below provided that you ensure that this notice is replicated
# unmodified and in its entirety in all distributions of the software,
# modified or unmodified, in source code or in binary form.
#
# Copyright (c) 2010 Advanced Micro Devices, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Configure the M5 cache hierarchy config in one place
#

import m5
from m5.objects import *
from gem5.isas import ISA
from gem5.runtime import get_runtime_isa

from common.Caches import *
from common import ObjectList


def _get_hwp(hwp_option):
    if hwp_option == None:
        return NULL

    hwpClass = ObjectList.hwp_list.get(hwp_option)
    return hwpClass()


def _get_cache_opts(level, options):
    opts = {}

    size_attr = f"{level}_size"
    if hasattr(options, size_attr):
        opts["size"] = getattr(options, size_attr)

    assoc_attr = f"{level}_assoc"
    if hasattr(options, assoc_attr):
        opts["assoc"] = getattr(options, assoc_attr)

    prefetcher_attr = f"{level}_hwp_type"
    if hasattr(options, prefetcher_attr):
        opts["prefetcher"] = _get_hwp(getattr(options, prefetcher_attr))

    return opts


def config_cache(options, system):
    if options.external_memory_system and (options.caches or options.l2cache):
        print("External caches and internal caches are exclusive options.\n")
        sys.exit(1)

    if options.external_memory_system:
        ExternalCache = ExternalCacheFactory(options.external_memory_system)

    if options.cpu_type == "O3_ARM_v7a_3":
        try:
            import cores.arm.O3_ARM_v7a as core
        except:
            print("O3_ARM_v7a_3 is unavailable. Did you compile the O3 model?")
            sys.exit(1)

        dcache_class, icache_class, l2_cache_class, walk_cache_class = (
            core.O3_ARM_v7a_DCache,
            core.O3_ARM_v7a_ICache,
            core.O3_ARM_v7aL2,
            None,
        )
    elif options.cpu_type == "HPI":
        try:
            import cores.arm.HPI as core
        except:
            print("HPI is unavailable.")
            sys.exit(1)

        dcache_class, icache_class, l2_cache_class, walk_cache_class = (
            core.HPI_DCache,
            core.HPI_ICache,
            core.HPI_L2,
            None,
        )
    elif options.cpu_type == "A510":
        try:
            import cores.arm.A510 as core
        except:
            print("A510 is unavailable.")
            sys.exit(1)

        dcache_class, icache_class, l2_cache_class, walk_cache_class = (
            core.A510_DCache,
            core.A510_ICache,
            core.A510_L2,
            None,
        )
    #elif options.cpu_type == "X2":
    #    try:
    #        import cores.arm.X2 as core
    #    except:
    #        print("X2 is unavailable.")
    #        sys.exit(1)

     #   dcache_class, icache_class, l2_cache_class, walk_cache_class = (
     #       core.X2_DCache,
     #       core.X2_ICache,
     #       core.X2_L2,
     #       None,
     #   )
    else:
        dcache_class, icache_class, l2_cache_class, walk_cache_class = (
            L1_DCache,
            L1_ICache,
            L2Cache,
            None,
        )

        if get_runtime_isa() in [ISA.X86, ISA.RISCV]:
            walk_cache_class = PageTableWalkerCache
    # For L3
    if options.pl2sl3cache:
        l3_cache_class = L3Cache

    # Set the cache line size of the system
    system.cache_line_size = options.cacheline_size

    # If elastic trace generation is enabled, make sure the memory system is
    # minimal so that compute delays do not include memory access latencies.
    # Configure the compulsory L1 caches for the O3CPU, do not configure
    # any more caches.
    if options.l2cache and options.elastic_trace_en:
        fatal("When elastic trace is enabled, do not configure L2 caches.")

    if options.l2cache:
        # Provide a clock for the L2 and the L1-to-L2 bus here as they
        # are not connected using addTwoLevelCacheHierarchy. Use the
        # same clock as the CPUs.
        system.l2 = l2_cache_class(clk_domain=system.cpu_clk_domain)

        system.tol2bus = L2XBar(clk_domain=system.cpu_clk_domain)
        system.l2.cpu_side = system.tol2bus.mem_side_ports
        system.l2.mem_side = system.membus.cpu_side_ports

    if options.pl2sl3cache:
        # Provide a clock for the L3 and the L2-to-L3 bus here as they
        # are not connected using addTwoLevelCacheHierarchy. Use the
        # same clock as the CPUs.
        system.l3 = l3_cache_class(
            clk_domain=system.cpu_clk_domain, **_get_cache_opts("l3", options)
        )

        # TODO: config for L3 croassbar?
        system.tol3bus = L2XBar(clk_domain=system.cpu_clk_domain)
        system.l3.cpu_side = system.tol3bus.mem_side_ports
        system.l3.mem_side = system.membus.cpu_side_ports

    if options.memchecker:
        system.memchecker = MemChecker()

    for i in range(options.num_cpus):
        if options.caches:
            icache = icache_class(**_get_cache_opts("l1i", options))
            dcache = dcache_class(**_get_cache_opts("l1d", options))

            # If we have a walker cache specified, instantiate two
            # instances here
            if walk_cache_class:
                iwalkcache = walk_cache_class()
                dwalkcache = walk_cache_class()
            else:
                iwalkcache = None
                dwalkcache = None

            if options.memchecker:
                dcache_mon = MemCheckerMonitor(warn_only=True)
                dcache_real = dcache

                # Do not pass the memchecker into the constructor of
                # MemCheckerMonitor, as it would create a copy; we require
                # exactly one MemChecker instance.
                dcache_mon.memchecker = system.memchecker

                # Connect monitor
                dcache_mon.mem_side = dcache.cpu_side

                # Let CPU connect to monitors
                dcache = dcache_mon

            # When connecting the caches, the clock is also inherited
            # from the CPU in question
            system.cpu[i].addPrivateSplitL1Caches(
                icache, dcache, iwalkcache, dwalkcache
            )

            if options.memchecker:
                # The mem_side ports of the caches haven't been connected yet.
                # Make sure connectAllPorts connects the right objects.
                system.cpu[i].dcache = dcache_real
                system.cpu[i].dcache_mon = dcache_mon

        elif options.external_memory_system:
            # These port names are presented to whatever 'external' system
            # gem5 is connecting to.  Its configuration will likely depend
            # on these names.  For simplicity, we would advise configuring
            # it to use this naming scheme; if this isn't possible, change
            # the names below.
            if get_runtime_isa() in [ISA.X86, ISA.ARM, ISA.RISCV]:
                system.cpu[i].addPrivateSplitL1Caches(
                    ExternalCache("cpu%d.icache" % i),
                    ExternalCache("cpu%d.dcache" % i),
                    ExternalCache("cpu%d.itb_walker_cache" % i),
                    ExternalCache("cpu%d.dtb_walker_cache" % i),
                )
            else:
                system.cpu[i].addPrivateSplitL1Caches(
                    ExternalCache("cpu%d.icache" % i),
                    ExternalCache("cpu%d.dcache" % i),
                )
        elif options.pl2sl3cache:
            icache = icache_class()
            dcache = dcache_class()
            if options.triangel:
                        l2_cache = l2_cache_class(
		        prefetcher=TriangelPrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
		            degree=4,		            
		            address_map_max_ways=8,
		            address_map_actual_entries="196608",
		            address_map_actual_cache_assoc=12,
		            address_map_rounded_entries="262144",
		            address_map_rounded_cache_assoc=16,
		            prefetched_cache_entries="256",
		            test_entries="64",
		            sample_entries="512",
		            sample_assoc=2,
		            address_map_cache_replacement_policy=RRIPRP()         )
		        )
            elif options.triangeldeg1:
                        l2_cache = l2_cache_class(
		        prefetcher=TriangelPrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
		            address_map_max_ways=8,
		            address_map_actual_entries="196608",
		            address_map_actual_cache_assoc=12,
		            address_map_rounded_entries="262144",
		            address_map_rounded_cache_assoc=16,
		            prefetched_cache_entries="256",
		            test_entries="64",
		            sample_entries="512",
		            sample_assoc=2,
		            degree=1,
		            should_lookahead=False,	            		            
		            address_map_cache_replacement_policy=RRIPRP()         )
		        )
            elif options.triangeloff1:
                        l2_cache = l2_cache_class(
		        prefetcher=TriangelPrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
		            degree=4,	
		            should_lookahead=False,	            
		            address_map_max_ways=8,
		            address_map_actual_entries="196608",
		            address_map_actual_cache_assoc=12,
		            address_map_rounded_entries="262144",
		            address_map_rounded_cache_assoc=16,
		            prefetched_cache_entries="256",
		            test_entries="64",
		            sample_entries="512",
		            sample_assoc=2,
		            address_map_cache_replacement_policy=RRIPRP()         )
		        )		        
            elif options.triangelrrip:
                        l2_cache = l2_cache_class(
		        prefetcher=TriangelPrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
		            degree=4,		            
		            address_map_max_ways=8,
		            address_map_actual_entries="196608",
		            address_map_actual_cache_assoc=12,
		            address_map_rounded_entries="262144",
		            address_map_rounded_cache_assoc=16,
		            prefetched_cache_entries="256",
		            test_entries="64",
		            sample_entries="512",
		            sample_assoc=2,
		            address_map_cache_replacement_policy=RRIPRP() 	        )
		        ) 
            elif options.triangelnorearr:
                        l2_cache = l2_cache_class(
		        prefetcher=TriangelPrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
		            degree=4,		            
		            address_map_max_ways=8,
		            address_map_actual_entries="196608",
		            address_map_actual_cache_assoc=12,
		            address_map_rounded_entries="262144",
		            address_map_rounded_cache_assoc=16,
		            prefetched_cache_entries="256",
		            test_entries="64",
		            sample_entries="512",
		            sample_assoc=2,
		            address_map_cache_replacement_policy=RRIPRP(),
		            should_rearrange=False
		             	        )
		        )    		           
            elif options.triangel256:
                        l2_cache = l2_cache_class(
		        prefetcher=TriangelPrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
		            degree=4,		            
      		            address_map_max_ways=2,
		            address_map_actual_entries="49152",
		            address_map_actual_cache_assoc=12,
		            address_map_rounded_entries="65536",
		            address_map_rounded_cache_assoc=16,
		            prefetched_cache_entries="256",
		            test_entries="64",
		            sample_entries="512",
		            lookup_assoc=0,
		            sample_assoc=2,
		            use_hawkeye=True,
		            address_map_cache_replacement_policy=WeightedLRURP()	        )
		        )
            elif options.triangel256lru:
                        l2_cache = l2_cache_class(
		        prefetcher=TriangelPrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
		            degree=4,		            
		            address_map_max_ways=2,
		            address_map_actual_entries="49152",
		            address_map_actual_cache_assoc=12,
		            address_map_rounded_entries="65536",
		            address_map_rounded_cache_assoc=16,
		            test_entries="64",
		            sample_entries="512",
		            lookup_assoc=0,
		            sample_assoc=2,
		            use_hawkeye=False,
		            address_map_cache_replacement_policy=LRURP()	        )
		        )
            elif options.triangel256lut:
                        l2_cache = l2_cache_class(
		        prefetcher=TriangelPrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
		            degree=4,		            
		            address_map_max_ways=2,
		            address_map_actual_entries="65536",
		            address_map_actual_cache_assoc=16,
		            address_map_rounded_entries="65536",
		            address_map_rounded_cache_assoc=16,
		            test_entries="64",
		            sample_entries="512",
		            lookup_assoc=16,
		            sample_assoc=2,
		            use_hawkeye=False	        )
		        )	
            elif options.triangel256lutrrip:
                        l2_cache = l2_cache_class(
		        prefetcher=TriangelPrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
		            degree=4,		            
		            address_map_max_ways=2,
		            address_map_actual_entries="65536",
		            address_map_actual_cache_assoc=16,
		            address_map_rounded_entries="65536",
		            address_map_rounded_cache_assoc=16,
		            test_entries="64",
		            sample_entries="512",
		            lookup_assoc=16,
		            sample_assoc=2,
		            use_hawkeye=False,
		            address_map_cache_replacement_policy=RRIPRP() 	        )
		        )			        
            elif options.triangel256luthawk:
                        l2_cache = l2_cache_class(
		        prefetcher=TriangelPrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
		            degree=4,		            
		            address_map_max_ways=2,
		            address_map_actual_entries="65536",
		            address_map_actual_cache_assoc=16,
		            address_map_rounded_entries="65536",
		            address_map_rounded_cache_assoc=16,
		            test_entries="64",
		            sample_entries="512",
		            lookup_assoc=16,
		            sample_assoc=2,
		            use_hawkeye=True,
		            address_map_cache_replacement_policy=WeightedLRURP() 	        )
		        )			        	        	        				        		        
            elif options.triangelsmall:
                        l2_cache = l2_cache_class(
		        prefetcher=TriangelPrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
		            degree=2,		            
      		            address_map_max_ways=8,
		            address_map_actual_entries="196608",
		            address_map_actual_cache_assoc=12,
		            address_map_rounded_entries="262144",
		            address_map_rounded_cache_assoc=16,
		            prefetched_cache_entries="128",
		            test_entries="16",
		            sample_entries="128",
		            training_unit_entries="128",
		            address_map_cache_replacement_policy=RRIPRP()		        )
		        )
            elif options.triage:
                        l2_cache = l2_cache_class(
		        prefetcher=TriagePrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
		            address_map_max_ways=8,
		            address_map_actual_entries="262144",
		            address_map_actual_cache_assoc=16,
		            address_map_rounded_entries="262144",
		            address_map_rounded_cache_assoc=16,
		            store_unreliable=True
                        )
                        )
            elif options.triagenorearr:
                        l2_cache = l2_cache_class(
		        prefetcher=TriagePrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
		            address_map_max_ways=8,
		            address_map_actual_entries="262144",
		            address_map_actual_cache_assoc=16,
		            address_map_rounded_entries="262144",
		            address_map_rounded_cache_assoc=16,
		            store_unreliable=True,
		            should_rearrange=False
                        )
                        )                        
            elif options.triageideal:
                        l2_cache = l2_cache_class(
		        prefetcher=TriagePrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
		            address_map_max_ways=8,
		            address_map_actual_entries="262144",
		            address_map_actual_cache_assoc=16,
		            address_map_rounded_entries="262144",
		            address_map_rounded_cache_assoc=16,
		            lookup_assoc=0,
		            store_unreliable=True
                        )
                        )                        
            elif options.triagefalut:
                        l2_cache = l2_cache_class(
		        prefetcher=TriagePrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
		            address_map_max_ways=8,
		            address_map_actual_entries="262144",
		            address_map_actual_cache_assoc=16,
		            address_map_rounded_entries="262144",
		            address_map_rounded_cache_assoc=16,
		            lookup_assoc=1024,
		            store_unreliable=True
                        )
                        )       
            elif options.triage12:
                        l2_cache = l2_cache_class(
		        prefetcher=TriagePrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
      		            address_map_max_ways=8,
		            address_map_actual_entries="196608",
		            address_map_actual_cache_assoc=12,
		            address_map_rounded_entries="262144",
		            address_map_rounded_cache_assoc=16,
		            lookup_assoc=0,
		            store_unreliable=True
                        )
                        )           
            elif options.triage10boff:
                        l2_cache = l2_cache_class(
		        prefetcher=TriagePrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
		            address_map_max_ways=8,
		            address_map_actual_entries="262144",
		            address_map_actual_cache_assoc=16,
		            address_map_rounded_entries="262144",
		            address_map_rounded_cache_assoc=16,
		            lookup_offset=10,
		            store_unreliable=True
                        )
                        )                                                              
            elif options.triagenounrel:
                        l2_cache = l2_cache_class(
		        prefetcher=TriagePrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
		            address_map_max_ways=8,
		            address_map_actual_entries="262144",
		            address_map_actual_cache_assoc=16,
		            address_map_rounded_entries="262144",
		            address_map_rounded_cache_assoc=16,
		            store_unreliable=False
		        )
		        )                        
            elif options.triagelru:
                        l2_cache = l2_cache_class(
		        prefetcher=TriagePrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
		            address_map_max_ways=8,
		            address_map_actual_entries="262144",
		            address_map_actual_cache_assoc=16,
		            address_map_rounded_entries="262144",
		            address_map_rounded_cache_assoc=16,
		            address_map_cache_replacement_policy=LRURP()
		        )
		        )
            elif options.triage256:
                        l2_cache = l2_cache_class(
		        prefetcher=TriagePrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
		            address_map_max_ways=2,
		            address_map_actual_entries="65536",
		            address_map_actual_cache_assoc=16,
		            address_map_rounded_entries="65536",
		            address_map_rounded_cache_assoc=16,
		            store_unreliable=True
                        )
                        )
            elif options.triage256rrip:
                        l2_cache = l2_cache_class(
		        prefetcher=TriagePrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
		            address_map_max_ways=2,
		            address_map_actual_entries="65536",
		            address_map_actual_cache_assoc=16,
		            address_map_rounded_entries="65536",
		            address_map_rounded_cache_assoc=16,
		            store_unreliable=True,
		            address_map_cache_replacement_policy=RRIPRP() 
                        )
                        )
            elif options.triage256ideal:
                        l2_cache = l2_cache_class(
		        prefetcher=TriagePrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
		            address_map_max_ways=2,
		            address_map_actual_entries="65536",
		            address_map_actual_cache_assoc=16,
		            address_map_rounded_entries="65536",
		            address_map_rounded_cache_assoc=16,
		            store_unreliable=True,
		            lookup_assoc=0
                        )
                        )                        
            elif options.triage256a12:
                        l2_cache = l2_cache_class(
		        prefetcher=TriagePrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
      		            address_map_max_ways=2,
		            address_map_actual_entries="49152",
		            address_map_actual_cache_assoc=12,
		            address_map_rounded_entries="65536",
		            address_map_rounded_cache_assoc=16,
		            lookup_assoc=0,
		            store_unreliable=True
                        )
                        )                          
            elif options.triagenounrel256:
                        l2_cache = l2_cache_class(
		        prefetcher=TriagePrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
       		            address_map_max_ways=2,
		            address_map_actual_entries="65536",
		            address_map_actual_cache_assoc=16,
		            address_map_rounded_entries="65536",
		            address_map_rounded_cache_assoc=16,
		            store_unreliable=False
		        )
		        )                        
            elif options.triagelru256:
                        l2_cache = l2_cache_class(
		        prefetcher=TriagePrefetcher(
		            cachetags=system.l3.tags,
		            cache_delay=25,
       		            address_map_max_ways=2,
		            address_map_actual_entries="65536",
		            address_map_actual_cache_assoc=16,
		            address_map_rounded_entries="65536",
		            address_map_rounded_cache_assoc=16,
		            address_map_cache_replacement_policy=LRURP()
		        )
		        )		        
            else:
                        l2_cache = l2_cache_class(
		        )
            # If we have a walker cache specified, instantiate two
            # instances here
            if walk_cache_class:
                iwalkcache = walk_cache_class()
                dwalkcache = walk_cache_class()
            else:
                iwalkcache = None
                dwalkcache = None
            if options.memchecker:
                dcache_mon = MemCheckerMonitor(warn_only=True)
                dcache_real = dcache

                # Do not pass the memchecker into the constructor of
                # MemCheckerMonitor, as it would create a copy; we require
                # exactly one MemChecker instance.
                dcache_mon.memchecker = system.memchecker

                # Connect monitor
                dcache_mon.mem_side = dcache.cpu_side

                # Let CPU connect to monitors
                dcache = dcache_mon
            # When connecting the caches, the clock is also inherited
            # from the CPU in question
            system.cpu[i].addTwoLevelCacheHierarchy(
                icache, dcache, l2_cache, iwalkcache, dwalkcache
            )

            if options.memchecker:
                # The mem_side ports of the caches haven't been connected yet.
                # Make sure connectAllPorts connects the right objects.
                system.cpu[i].dcache = dcache_real
                system.cpu[i].dcache_mon = dcache_mon

        system.cpu[i].createInterruptController()
        if options.l2cache:
            system.cpu[i].connectAllPorts(
                system.tol2bus.cpu_side_ports,
                system.membus.cpu_side_ports,
                system.membus.mem_side_ports,
            )
        elif options.pl2sl3cache:
            system.cpu[i].connectAllPorts(
                system.tol3bus.cpu_side_ports,
                system.membus.cpu_side_ports,
                system.membus.mem_side_ports,
            )
        elif options.external_memory_system:
            system.cpu[i].connectUncachedPorts(
                system.membus.cpu_side_ports, system.membus.mem_side_ports
            )
        else:
            system.cpu[i].connectBus(system.membus)

    return system


# ExternalSlave provides a "port", but when that port connects to a cache,
# the connecting CPU SimObject wants to refer to its "cpu_side".
# The 'ExternalCache' class provides this adaptation by rewriting the name,
# eliminating distracting changes elsewhere in the config code.
class ExternalCache(ExternalSlave):
    def __getattr__(cls, attr):
        if attr == "cpu_side":
            attr = "port"
        return super(ExternalSlave, cls).__getattr__(attr)

    def __setattr__(cls, attr, value):
        if attr == "cpu_side":
            attr = "port"
        return super(ExternalSlave, cls).__setattr__(attr, value)


def ExternalCacheFactory(port_type):
    def make(name):
        return ExternalCache(
            port_data=name, port_type=port_type, addr_ranges=[AllMemory]
        )

    return make
