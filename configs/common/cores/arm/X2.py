#-----------------------------------------------------------------------
#                X2 big core from
# https://developer.arm.com/documentation/PJDOC-466751330-14955/latest
# https://en.wikichip.org/wiki/arm_holdings/microarchitectures/cortex-x1
#https://en.wikichip.org/wiki/arm_holdings/microarchitectures/cortex-a78
#https://en.wikichip.org/wiki/arm_holdings/microarchitectures/cortex-a77
#https://www.anandtech.com/show/16693/arm-announces-mobile-armv9-cpu-microarchitectures-cortexx2-cortexa710-cortexa510/2
#-----------------------------------------------------------------------

from m5.objects import *


# Simple ALU Instructions have a latency of 1
class X2_Simple_Int(FUDesc):
    opList = [ OpDesc(opClass='IntAlu', opLat=1) ]
    count = 2

# Complex ALU instructions have a variable latencies
class X2_Complex_Int(FUDesc):
    opList = [ OpDesc(opClass='IntAlu', opLat=1), #Really should be integer shift ALU, ie this one does shift-adds at two cycles, and both do simples at 1 cycle
               OpDesc(opClass='IntMult', opLat=2, pipelined=True),
               OpDesc(opClass='IntDiv', opLat=8, pipelined=False), #actually, 5 to 12 for W or 5 to 20 for X
               OpDesc(opClass='IprAccess', opLat=1, pipelined=True) ]
    count = 2

# Floating point and SIMD instructions
class X2_FP(FUDesc):
    opList = [ OpDesc(opClass='SimdAdd', opLat=2),
               OpDesc(opClass='SimdAddAcc', opLat=2),
               OpDesc(opClass='SimdAlu', opLat=2),
               OpDesc(opClass='SimdCmp', opLat=2),
               OpDesc(opClass='SimdCvt', opLat=2),
               OpDesc(opClass='SimdMisc', opLat=2),
               OpDesc(opClass='SimdMult',opLat=4),
               OpDesc(opClass='SimdMultAcc',opLat=5),
               OpDesc(opClass='SimdShift',opLat=2),
               OpDesc(opClass='SimdShiftAcc', opLat=5),
               OpDesc(opClass='SimdSqrt', opLat=6),
               OpDesc(opClass='SimdFloatAdd',opLat=2),
               OpDesc(opClass='SimdFloatAlu',opLat=2),
               OpDesc(opClass='SimdFloatCmp', opLat=2),
               OpDesc(opClass='SimdFloatCvt', opLat=2),
               OpDesc(opClass='SimdFloatDiv', opLat=10, pipelined=False),
               OpDesc(opClass='SimdFloatMisc', opLat=2),
               OpDesc(opClass='SimdFloatMult', opLat=3),
               OpDesc(opClass='SimdFloatMultAcc',opLat=5),
               OpDesc(opClass='SimdFloatSqrt', opLat=12, pipelined=False),
               OpDesc(opClass='FloatAdd', opLat=2),
               OpDesc(opClass='FloatCmp', opLat=2),
               OpDesc(opClass='FloatCvt', opLat=2),
               OpDesc(opClass='FloatDiv', opLat=12, pipelined=False),
               OpDesc(opClass='FloatSqrt', opLat=12, pipelined=False),
               OpDesc(opClass='FloatMult', opLat=3) ]
    count = 4


# Load/Store Units
class X2_Load(FUDesc):
    opList = [ OpDesc(opClass='MemRead',opLat=1) ]
    count = 1

#Actually, separate "store address calculation/load and store-data units
class X2_Load_Store(FUDesc):
    opList = [OpDesc(opClass='MemWrite',opLat=1),
              OpDesc(opClass='MemRead',opLat=1)] #Actually 4, but we'll make all 4 of those l1d cache cycles?
    count = 2

# Functional Units for this CPU
class X2_FUP(FUPool):
    FUList = [X2_Simple_Int(), X2_Complex_Int(),
              X2_Load(), X2_Load_Store(), X2_FP()]


class X2(ArmO3CPU):
    LQEntries = 85
    SQEntries = 90
    LSQDepCheckShift = 0
    LFSTSize = 1024
    SSITSize = 1024
    decodeToFetchDelay = 1
    renameToFetchDelay = 1
    iewToFetchDelay = 1
    commitToFetchDelay = 1
    renameToDecodeDelay = 1
    iewToDecodeDelay = 1
    commitToDecodeDelay = 1
    iewToRenameDelay = 1
    commitToRenameDelay = 1
    commitToIEWDelay = 1
    fetchWidth = 5
    fetchBufferSize = 32
    fetchToDecodeDelay = 1
    decodeWidth = 5
    decodeToRenameDelay = 1
    renameWidth = 5
    renameToIEWDelay = 1
    issueToExecuteDelay = 1
    dispatchWidth = 10
    issueWidth = 10
    wbWidth = 10
    fuPool = X2_FUP()
    iewToCommitDelay = 1
    renameToROBDelay = 1
    commitWidth = 10
    squashWidth = 10
    trapLatency = 10
    backComSize = 5
    forwardComSize = 5
    numPhysIntRegs = 150
    numPhysFloatRegs = 256
    numIQEntries = 120
    numROBEntries = 288

    switched_out = False
    branchPred = MultiperspectivePerceptronTAGE64KB()

class L1Cache(Cache):
    tag_latency = 2
    data_latency = 2
    response_latency = 2
    tgts_per_mshr = 8
    # Consider the L2 a victim cache also for clean lines
    writeback_clean = True

# Instruction Cache
class L1I(L1Cache):
    mshrs = 16
    size = '64kB'
    assoc = 4
    is_read_only = True

class X2_ICache(L1I):
    pass

# Data Cache
class L1D(L1Cache):
    tag_latency = 4
    data_latency = 4
    response_latency = 4
    mshrs = 16
    size = '64kB'
    assoc = 4
    write_buffers = 32
    prefetch_on_access = True
    prefetcher = StridePrefetcher(degree=8, latency = 1)    
    # Simple stride prefetcher


class X2_DCache(L1D):
    pass

# L2 Cache
class L2(Cache):
    tag_latency = 9
    data_latency = 9
    response_latency = 9
    mshrs = 32
    tgts_per_mshr = 8
    size = '512kB'
    assoc = 8
    write_buffers = 8
    prefetch_on_pf_hit = True
    #prefetcher = TriangelPrefetcher(cache_delay=9, triage_mode = False, address_map_actual_entries="49152", address_map_actual_cache_assoc=48,address_map_rounded_entries="65536",address_map_rounded_cache_assoc=64)
    # Simple stride prefetcher
    #prefetcher = StridePrefetcher(degree=8, latency = 1)
    tags = BaseSetAssoc()
    replacement_policy = LRURP()

class X2_L2(L2):
    pass
