import sys
import argparse
import math

parser = argparse.ArgumentParser(description='Collect statistics')
parser.add_argument('--statFile', type=str, action='store',required=True,
                    help='the name of the input statistic file')
parser.add_argument('--templateFile', type=str, action='store',required=True,
                    help='the name of the input template file')
parser.add_argument('--numCores', type=int, action='store',required=True,
                    help='number of cores')

grep_stats_key = [
    "system.switch_cpus0.numCycles",
    "system.switch_cpus0.idleCycles", 
    "system.switch_cpus0.statIssuedInstType_0::Branch",
    "system.switch_cpus0.statIssuedInstType_0::FloatAdd",
    "system.switch_cpus0.statIssuedInstType_0::FloatCmp",
    "system.switch_cpus0.statIssuedInstType_0::FloatCvt",
    "system.switch_cpus0.statIssuedInstType_0::FloatMult",
    "system.switch_cpus0.statIssuedInstType_0::FloatMultAcc",
    "system.switch_cpus0.statIssuedInstType_0::FloatDiv",
    "system.switch_cpus0.statIssuedInstType_0::FloatMisc",
    "system.switch_cpus0.statIssuedInstType_0::FloatSqrt",
    "system.switch_cpus0.statIssuedInstType_0::SimdAdd",
    "system.switch_cpus0.statIssuedInstType_0::SimdAddAcc",
    "system.switch_cpus0.statIssuedInstType_0::SimdAlu",
    "system.switch_cpus0.statIssuedInstType_0::SimdCmp",
    "system.switch_cpus0.statIssuedInstType_0::SimdCvt",
    "system.switch_cpus0.statIssuedInstType_0::SimdMisc",
    "system.switch_cpus0.statIssuedInstType_0::SimdMult",
    "system.switch_cpus0.statIssuedInstType_0::SimdMultAcc",
    "system.switch_cpus0.statIssuedInstType_0::SimdShift",
    "system.switch_cpus0.statIssuedInstType_0::SimdShiftAcc",
    "system.switch_cpus0.statIssuedInstType_0::SimdDiv",
    "system.switch_cpus0.statIssuedInstType_0::SimdSqrt",
    "system.switch_cpus0.statIssuedInstType_0::SimdFloatAdd",
    "system.switch_cpus0.statIssuedInstType_0::SimdFloatAlu",
    "system.switch_cpus0.statIssuedInstType_0::SimdFloatCmp",
    "system.switch_cpus0.statIssuedInstType_0::SimdFloatCvt",
    "system.switch_cpus0.statIssuedInstType_0::SimdFloatDiv",
    "system.switch_cpus0.statIssuedInstType_0::SimdFloatMisc",
    "system.switch_cpus0.statIssuedInstType_0::SimdFloatMult",
    "system.switch_cpus0.statIssuedInstType_0::SimdFloatMultAcc",
    "system.switch_cpus0.statIssuedInstType_0::SimdFloatSqrt",
    "system.switch_cpus0.statIssuedInstType_0::SimdReduceAdd",
    "system.switch_cpus0.statIssuedInstType_0::SimdReduceAlu",
    "system.switch_cpus0.statIssuedInstType_0::SimdReduceCmp",
    "system.switch_cpus0.statIssuedInstType_0::SimdFloatReduceAdd",
    "system.switch_cpus0.statIssuedInstType_0::SimdFloatReduceCmp",
    "system.switch_cpus0.statIssuedInstType_0::SimdAes",
    "system.switch_cpus0.statIssuedInstType_0::SimdAesMix",
    "system.switch_cpus0.statIssuedInstType_0::SimdSha1Hash",
    "system.switch_cpus0.statIssuedInstType_0::SimdSha1Hash2",
    "system.switch_cpus0.statIssuedInstType_0::SimdSha256Hash",
    "system.switch_cpus0.statIssuedInstType_0::SimdSha256Hash2",
    "system.switch_cpus0.statIssuedInstType_0::SimdShaSigma2",
    "system.switch_cpus0.statIssuedInstType_0::SimdShaSigma3",
    "system.switch_cpus0.statIssuedInstType_0::SimdPredAlu",
    "system.switch_cpus0.statIssuedInstType_0::MemRead",
    "system.switch_cpus0.statIssuedInstType_0::MemWrite",
    "system.switch_cpus0.statIssuedInstType_0::FloatMemRead",
    "system.switch_cpus0.statIssuedInstType_0::FloatMemWrite",
    "system.switch_cpus0.statIssuedInstType_0::IprAccess",
    "system.switch_cpus0.statIssuedInstType_0::total",
    "system.switch_cpus0.branchPred.condIncorrect",
    "system.switch_cpus0.commit.committedInstType_0::No_OpClass",
    "system.switch_cpus0.commit.committedInstType_0::Branch",    
    "system.switch_cpus0.commit.committedInstType_0::IntAlu",    
    "system.switch_cpus0.commit.committedInstType_0::IntMult",   
    "system.switch_cpus0.commit.committedInstType_0::IntDiv",    
    "system.switch_cpus0.commit.committedInstType_0::FloatAdd",  
    "system.switch_cpus0.commit.committedInstType_0::FloatCmp",  
    "system.switch_cpus0.commit.committedInstType_0::FloatCvt",  
    "system.switch_cpus0.commit.committedInstType_0::FloatMult", 
    "system.switch_cpus0.commit.committedInstType_0::FloatMultAcc", 
    "system.switch_cpus0.commit.committedInstType_0::FloatDiv",     
    "system.switch_cpus0.commit.committedInstType_0::FloatMisc",    
    "system.switch_cpus0.commit.committedInstType_0::FloatSqrt",    
    "system.switch_cpus0.commit.committedInstType_0::SimdAdd",      
    "system.switch_cpus0.commit.committedInstType_0::SimdAddAcc",   
    "system.switch_cpus0.commit.committedInstType_0::SimdAlu",      
    "system.switch_cpus0.commit.committedInstType_0::SimdCmp",      
    "system.switch_cpus0.commit.committedInstType_0::SimdCvt",      
    "system.switch_cpus0.commit.committedInstType_0::SimdMisc",     
    "system.switch_cpus0.commit.committedInstType_0::SimdMult",     
    "system.switch_cpus0.commit.committedInstType_0::SimdMultAcc",  
    "system.switch_cpus0.commit.committedInstType_0::SimdShift",    
    "system.switch_cpus0.commit.committedInstType_0::SimdShiftAcc", 
    "system.switch_cpus0.commit.committedInstType_0::SimdDiv",      
    "system.switch_cpus0.commit.committedInstType_0::SimdSqrt",     
    "system.switch_cpus0.commit.committedInstType_0::SimdFloatAdd", 
    "system.switch_cpus0.commit.committedInstType_0::SimdFloatAlu", 
    "system.switch_cpus0.commit.committedInstType_0::SimdFloatCmp", 
    "system.switch_cpus0.commit.committedInstType_0::SimdFloatCvt", 
    "system.switch_cpus0.commit.committedInstType_0::SimdFloatDiv", 
    "system.switch_cpus0.commit.committedInstType_0::SimdFloatMisc",
    "system.switch_cpus0.commit.committedInstType_0::SimdFloatMult",
    "system.switch_cpus0.commit.committedInstType_0::SimdFloatMultAcc",  
    "system.switch_cpus0.commit.committedInstType_0::SimdFloatSqrt",     
    "system.switch_cpus0.commit.committedInstType_0::SimdReduceAdd",     
    "system.switch_cpus0.commit.committedInstType_0::SimdReduceAlu",     
    "system.switch_cpus0.commit.committedInstType_0::SimdReduceCmp",     
    "system.switch_cpus0.commit.committedInstType_0::SimdFloatReduceAdd",
    "system.switch_cpus0.commit.committedInstType_0::SimdFloatReduceCmp",
    "system.switch_cpus0.commit.committedInstType_0::SimdAes",           
    "system.switch_cpus0.commit.committedInstType_0::SimdAesMix",        
    "system.switch_cpus0.commit.committedInstType_0::SimdSha1Hash",      
    "system.switch_cpus0.commit.committedInstType_0::SimdSha1Hash2",     
    "system.switch_cpus0.commit.committedInstType_0::SimdSha256Hash",    
    "system.switch_cpus0.commit.committedInstType_0::SimdSha256Hash2",   
    "system.switch_cpus0.commit.committedInstType_0::SimdShaSigma2",     
    "system.switch_cpus0.commit.committedInstType_0::SimdShaSigma3",  
    "system.switch_cpus0.commit.committedInstType_0::SimdPredAlu", 
    "system.switch_cpus0.commit.committedInstType_0::MemRead",     
    "system.switch_cpus0.commit.committedInstType_0::MemWrite",    
    "system.switch_cpus0.commit.committedInstType_0::FloatMemRead",   
    "system.switch_cpus0.commit.committedInstType_0::FloatMemWrite",
    "system.switch_cpus0.commit.committedInstType_0::IprAccess",    
    "system.switch_cpus0.commit.committedInstType_0::InstPrefetch", 
    "system.switch_cpus0.commit.committedInstType_0::total",
    "system.switch_cpus0.committedInstType_0::No_OpClass",
    "system.switch_cpus0.committedInstType_0::Branch",    
    "system.switch_cpus0.committedInstType_0::IntAlu",    
    "system.switch_cpus0.committedInstType_0::IntMult",   
    "system.switch_cpus0.committedInstType_0::IntDiv",    
    "system.switch_cpus0.committedInstType_0::FloatAdd",  
    "system.switch_cpus0.committedInstType_0::FloatCmp",  
    "system.switch_cpus0.committedInstType_0::FloatCvt",  
    "system.switch_cpus0.committedInstType_0::FloatMult", 
    "system.switch_cpus0.committedInstType_0::FloatMultAcc", 
    "system.switch_cpus0.committedInstType_0::FloatDiv",     
    "system.switch_cpus0.committedInstType_0::FloatMisc",    
    "system.switch_cpus0.committedInstType_0::FloatSqrt",    
    "system.switch_cpus0.committedInstType_0::SimdAdd",      
    "system.switch_cpus0.committedInstType_0::SimdAddAcc",   
    "system.switch_cpus0.committedInstType_0::SimdAlu",      
    "system.switch_cpus0.committedInstType_0::SimdCmp",      
    "system.switch_cpus0.committedInstType_0::SimdCvt",      
    "system.switch_cpus0.committedInstType_0::SimdMisc",     
    "system.switch_cpus0.committedInstType_0::SimdMult",     
    "system.switch_cpus0.committedInstType_0::SimdMultAcc",  
    "system.switch_cpus0.committedInstType_0::SimdShift",    
    "system.switch_cpus0.committedInstType_0::SimdShiftAcc", 
    "system.switch_cpus0.committedInstType_0::SimdDiv",      
    "system.switch_cpus0.committedInstType_0::SimdSqrt",     
    "system.switch_cpus0.committedInstType_0::SimdFloatAdd", 
    "system.switch_cpus0.committedInstType_0::SimdFloatAlu", 
    "system.switch_cpus0.committedInstType_0::SimdFloatCmp", 
    "system.switch_cpus0.committedInstType_0::SimdFloatCvt", 
    "system.switch_cpus0.committedInstType_0::SimdFloatDiv", 
    "system.switch_cpus0.committedInstType_0::SimdFloatMisc",
    "system.switch_cpus0.committedInstType_0::SimdFloatMult",
    "system.switch_cpus0.committedInstType_0::SimdFloatMultAcc",  
    "system.switch_cpus0.committedInstType_0::SimdFloatSqrt",     
    "system.switch_cpus0.committedInstType_0::SimdReduceAdd",     
    "system.switch_cpus0.committedInstType_0::SimdReduceAlu",     
    "system.switch_cpus0.committedInstType_0::SimdReduceCmp",     
    "system.switch_cpus0.committedInstType_0::SimdFloatReduceAdd",
    "system.switch_cpus0.committedInstType_0::SimdFloatReduceCmp",
    "system.switch_cpus0.committedInstType_0::SimdAes",           
    "system.switch_cpus0.committedInstType_0::SimdAesMix",        
    "system.switch_cpus0.committedInstType_0::SimdSha1Hash",      
    "system.switch_cpus0.committedInstType_0::SimdSha1Hash2",     
    "system.switch_cpus0.committedInstType_0::SimdSha256Hash",    
    "system.switch_cpus0.committedInstType_0::SimdSha256Hash2",   
    "system.switch_cpus0.committedInstType_0::SimdShaSigma2",     
    "system.switch_cpus0.committedInstType_0::SimdShaSigma3",  
    "system.switch_cpus0.committedInstType_0::SimdPredAlu", 
    "system.switch_cpus0.committedInstType_0::MemRead",     
    "system.switch_cpus0.committedInstType_0::MemWrite",    
    "system.switch_cpus0.committedInstType_0::FloatMemRead",   
    "system.switch_cpus0.committedInstType_0::FloatMemWrite",
    "system.switch_cpus0.committedInstType_0::IprAccess",    
    "system.switch_cpus0.committedInstType_0::InstPrefetch", 
    "system.switch_cpus0.committedInstType_0::total",
    "system.switch_cpus0.fetch2.intInstructions",  
    "system.switch_cpus0.fetch2.fpInstructions",   
    "system.switch_cpus0.fetch2.vecInstructions",  
    "system.switch_cpus0.fetch2.loadInstructions", 
    "system.switch_cpus0.fetch2.storeInstructions",
    "system.switch_cpus0.fetch2.amoInstructions",  
    "system.switch_cpus0.rob.reads",
    "system.switch_cpus0.rob.writes",
    "system.switch_cpus0.rename.intLookups",
    "system.switch_cpus0.rename.vecLookups",
    "system.switch_cpus0.intInstQueueReads",
    "system.switch_cpus0.intInstQueueWrites",
    "system.switch_cpus0.intInstQueueWakeupAccesses",
    "system.switch_cpus0.fpInstQueueReads",
    "system.switch_cpus0.fpInstQueueWrites",          
    "system.switch_cpus0.fpInstQueueWakeupAccesses",  
    "system.switch_cpus0.vecInstQueueReads",          
    "system.switch_cpus0.vecInstQueueWrites",         
    "system.switch_cpus0.vecInstQueueWakeupAccesses", 
    "system.switch_cpus0.intRegfileReads",  
    "system.switch_cpus0.intRegfileWrites", 
    "system.switch_cpus0.vecRegfileReads",  
    "system.switch_cpus0.vecRegfileWrites",
    "system.switch_cpus0.ccRegfileReads",   
    "system.switch_cpus0.ccRegfileWrites",  
    "system.switch_cpus0.miscRegfileReads", 
    "system.switch_cpus0.miscRegfileWrites",
    "system.switch_cpus0.commit.functionCalls",
    "system.switch_cpus0.intAluAccesses",
    "system.switch_cpus0.fpAluAccesses",
    "system.switch_cpus0.vecAluAccesses",
    "system.cpu0.icache.overallAccesses::total",
    "system.cpu0.icache.overallMisses::total",
    "system.cpu0.dcache.ReadReq.accesses::total",
    "system.cpu0.dcache.WriteReq.accesses::total",
    "system.cpu0.dcache.ReadReq.misses::total",
    "system.cpu0.dcache.WriteReq.misses::total",
    "system.cpu0.l2cache.ReadCleanReq.accesses::total",
    "system.cpu0.l2cache.ReadExReq.accesses::total",
    "system.cpu0.l2cache.ReadSharedReq.accesses::total",
    "system.cpu0.l2cache.WritebackClean.accesses::total",
    "system.cpu0.l2cache.WritebackDirty.accesses::total",
    "system.cpu0.l2cache.ReadCleanReq.misses::total",
    "system.cpu0.l2cache.ReadExReq.misses::total",
    "system.cpu0.l2cache.ReadSharedReq.misses::total",
    "system.cpu0.l2cache.WritebackClean.misses::total",
    "system.cpu0.l2cache.WritebackDirty.misses::total",
    "system.switch_cpus0.branchPred.BTBLookups",
    "system.l3.ReadCleanReq.accesses::total",
    "system.l3.ReadExReq.accesses::total",
    "system.l3.WritebackDirty.accesses::total",
    "system.l3.ReadCleanReq.misses::total",
    "system.l3.ReadExReq.misses::total",
    "system.l3.WritebackDirty.misss::total",
    "system.membus.pktCount::total"
    ]

calc_stats_key = {
    "system.switch_cpus0.numCycles - system.switch_cpus0.idleCycles": "stats_value[\"system.switch_cpus0.numCycles\"] - stats_value[\"system.switch_cpus0.idleCycles\"]",
    "system.switch_cpus0.statIssuedInstType_0::total - Float* - Simd*": "stats_value[\"system.switch_cpus0.statIssuedInstType_0::total\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::FloatAdd\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::FloatCmp\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::FloatCvt\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::FloatMult\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::FloatMultAcc\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::FloatDiv\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::FloatMisc\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::FloatSqrt\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdAdd\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdAddAcc\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdAlu\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdCmp\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdCvt\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdMisc\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdMult\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdMultAcc\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdShift\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdShiftAcc\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdDiv\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdSqrt\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatAdd\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatAlu\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatCmp\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatCvt\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatDiv\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatMisc\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatMult\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatMultAcc\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatSqrt\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdReduceAdd\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdReduceAlu\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdReduceCmp\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatReduceAdd\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatReduceCmp\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdAes\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdAesMix\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdSha1Hash\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdSha1Hash2\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdSha256Hash\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdSha256Hash2\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdShaSigma2\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdShaSigma3\"] - stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdPredAlu\"]",
    "system.switch_cpus0.statIssuedInstType_0::Float* + Simd*": "stats_value[\"system.switch_cpus0.statIssuedInstType_0::FloatAdd\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::FloatCmp\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::FloatCvt\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::FloatMult\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::FloatMultAcc\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::FloatDiv\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::FloatMisc\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::FloatSqrt\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdAdd\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdAddAcc\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdAlu\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdCmp\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdCvt\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdMisc\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdMult\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdMultAcc\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdShift\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdShiftAcc\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdDiv\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdSqrt\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatAdd\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatAlu\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatCmp\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatCvt\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatDiv\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatMisc\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatMult\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatMultAcc\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatSqrt\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdReduceAdd\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdReduceAlu\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdReduceCmp\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatReduceAdd\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdFloatReduceCmp\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdAes\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdAesMix\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdSha1Hash\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdSha1Hash2\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdSha256Hash\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdSha256Hash2\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdShaSigma2\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdShaSigma3\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::SimdPredAlu\"]",
    "system.switch_cpus0.statIssuedInstType_0::MemRead + FloatMemRead": "stats_value[\"system.switch_cpus0.statIssuedInstType_0::MemRead\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::FloatMemRead\"]",
    "system.switch_cpus0.statIssuedInstType_0::MemWrite + FloatMemWrite": "stats_value[\"system.switch_cpus0.statIssuedInstType_0::MemWrite\"] + stats_value[\"system.switch_cpus0.statIssuedInstType_0::FloatMemWrite\"]",
    "system.switch_cpus0.commit.committedInstType_0::total - Float* - Simd*": "stats_value[\"system.switch_cpus0.commit.committedInstType_0::total\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::FloatAdd\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::FloatCmp\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::FloatCvt\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::FloatMult\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::FloatMultAcc\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::FloatDiv\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::FloatMisc\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::FloatSqrt\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdAdd\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdAddAcc\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdAlu\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdCmp\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdCvt\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdMisc\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdMult\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdMultAcc\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdShift\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdShiftAcc\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdDiv\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdSqrt\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatAdd\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatAlu\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatCmp\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatCvt\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatDiv\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatMisc\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatMult\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatMultAcc\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatSqrt\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdReduceAdd\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdReduceAlu\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdReduceCmp\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatReduceAdd\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatReduceCmp\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdAes\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdAesMix\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdSha1Hash\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdSha1Hash2\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdSha256Hash\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdSha256Hash2\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdShaSigma2\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdShaSigma3\"] - stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdPredAlu\"]",
    "system.switch_cpus0.commit.committedInstType_0::Float* + Simd*": "stats_value[\"system.switch_cpus0.commit.committedInstType_0::FloatAdd\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::FloatCmp\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::FloatCvt\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::FloatMult\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::FloatMultAcc\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::FloatDiv\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::FloatMisc\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::FloatSqrt\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdAdd\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdAddAcc\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdAlu\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdCmp\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdCvt\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdMisc\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdMult\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdMultAcc\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdShift\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdShiftAcc\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdDiv\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdSqrt\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatAdd\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatAlu\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatCmp\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatCvt\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatDiv\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatMisc\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatMult\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatMultAcc\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatSqrt\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdReduceAdd\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdReduceAlu\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdReduceCmp\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatReduceAdd\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdFloatReduceCmp\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdAes\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdAesMix\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdSha1Hash\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdSha1Hash2\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdSha256Hash\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdSha256Hash2\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdShaSigma2\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdShaSigma3\"] + stats_value[\"system.switch_cpus0.commit.committedInstType_0::SimdPredAlu\"]",
    "system.switch_cpus0.committedInstType_0::total - Float* - Simd*": "stats_value[\"system.switch_cpus0.committedInstType_0::total\"] - stats_value[\"system.switch_cpus0.committedInstType_0::FloatAdd\"] - stats_value[\"system.switch_cpus0.committedInstType_0::FloatCmp\"] - stats_value[\"system.switch_cpus0.committedInstType_0::FloatCvt\"] - stats_value[\"system.switch_cpus0.committedInstType_0::FloatMult\"] - stats_value[\"system.switch_cpus0.committedInstType_0::FloatMultAcc\"] - stats_value[\"system.switch_cpus0.committedInstType_0::FloatDiv\"] - stats_value[\"system.switch_cpus0.committedInstType_0::FloatMisc\"] - stats_value[\"system.switch_cpus0.committedInstType_0::FloatSqrt\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdAdd\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdAddAcc\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdAlu\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdCmp\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdCvt\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdMisc\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdMult\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdMultAcc\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdShift\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdShiftAcc\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdDiv\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdSqrt\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatAdd\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatAlu\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatCmp\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatCvt\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatDiv\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatMisc\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatMult\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatMultAcc\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatSqrt\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdReduceAdd\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdReduceAlu\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdReduceCmp\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatReduceAdd\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatReduceCmp\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdAes\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdAesMix\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdSha1Hash\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdSha1Hash2\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdSha256Hash\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdSha256Hash2\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdShaSigma2\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdShaSigma3\"] - stats_value[\"system.switch_cpus0.committedInstType_0::SimdPredAlu\"]",
    "system.switch_cpus0.committedInstType_0::Float* + Simd*": "stats_value[\"system.switch_cpus0.committedInstType_0::FloatAdd\"] + stats_value[\"system.switch_cpus0.committedInstType_0::FloatCmp\"] + stats_value[\"system.switch_cpus0.committedInstType_0::FloatCvt\"] + stats_value[\"system.switch_cpus0.committedInstType_0::FloatMult\"] + stats_value[\"system.switch_cpus0.committedInstType_0::FloatMultAcc\"] + stats_value[\"system.switch_cpus0.committedInstType_0::FloatDiv\"] + stats_value[\"system.switch_cpus0.committedInstType_0::FloatMisc\"] + stats_value[\"system.switch_cpus0.committedInstType_0::FloatSqrt\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdAdd\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdAddAcc\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdAlu\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdCmp\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdCvt\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdMisc\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdMult\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdMultAcc\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdShift\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdShiftAcc\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdDiv\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdSqrt\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatAdd\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatAlu\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatCmp\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatCvt\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatDiv\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatMisc\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatMult\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatMultAcc\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatSqrt\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdReduceAdd\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdReduceAlu\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdReduceCmp\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatReduceAdd\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdFloatReduceCmp\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdAes\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdAesMix\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdSha1Hash\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdSha1Hash2\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdSha256Hash\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdSha256Hash2\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdShaSigma2\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdShaSigma3\"] + stats_value[\"system.switch_cpus0.committedInstType_0::SimdPredAlu\"]",
    "system.switch_cpus0.committedInstType_0::MemRead + FloatMemRead": "stats_value[\"system.switch_cpus0.committedInstType_0::MemRead\"] + stats_value[\"system.switch_cpus0.committedInstType_0::FloatMemRead\"]",
    "system.switch_cpus0.committedInstType_0::MemWrite + FloatMemWrite": "stats_value[\"system.switch_cpus0.committedInstType_0::MemWrite\"] + stats_value[\"system.switch_cpus0.committedInstType_0::FloatMemWrite\"]",
    "system.switch_cpus0.rename.fpLookups + vecLookups": "stats_value[\"system.switch_cpus0.rename.vecLookups\"]",
    "system.switch_cpus0.fpInstQueueReads + vecInstQueueReads": "stats_value[\"system.switch_cpus0.fpInstQueueReads\"] + stats_value[\"system.switch_cpus0.vecInstQueueReads\"]",
    "system.switch_cpus0.fpInstQueueWrites + vecInstQueueWrites": "stats_value[\"system.switch_cpus0.fpInstQueueWrites\"] + stats_value[\"system.switch_cpus0.vecInstQueueWrites\"]",
    "system.switch_cpus0.fpInstQueueWakeupAccesses + vecInstQueueWakeupAccesses": "stats_value[\"system.switch_cpus0.fpInstQueueWakeupAccesses\"] + stats_value[\"system.switch_cpus0.vecInstQueueWakeupAccesses\"]",
    "system.switch_cpus0.fpRegfileReads + vecRegfileReads": "stats_value[\"system.switch_cpus0.vecRegfileReads\"]",
    "system.switch_cpus0.fpRegfileWrites + vecRegfileWrites": "stats_value[\"system.switch_cpus0.vecRegfileWrites\"]",
    "system.switch_cpus0.fpAluAccesses + system.switch_cpus0.vecAluAccesses": "stats_value[\"system.switch_cpus0.fpAluAccesses\"] + stats_value[\"system.switch_cpus0.vecAluAccesses\"]",
    "system.cpu0.dcache.WriteReq.accesses::total + SwapReq.accesses::total": "stats_value[\"system.cpu0.dcache.WriteReq.accesses::total\"]",
    "system.cpu0.dcache.WriteReq.misses::total + SwapReq.misses::total": "stats_value[\"system.cpu0.dcache.WriteReq.misses::total\"]",
    "system.l3.Read*Req.accesses::total": "stats_value[\"system.l3.ReadCleanReq.accesses::total\"] + stats_value[\"system.l3.ReadExReq.accesses::total\"]",
    "system.l3.Write*Req.accesses::total": "stats_value[\"system.l3.WritebackDirty.accesses::total\"]",
    "system.l3.Read*Req.misses::total": "stats_value[\"system.l3.ReadExReq.misses::total\"] + stats_value[\"system.l3.ReadCleanReq.misses::total\"]",
    "system.l3.Write*Req.misses::total": "stats_value[\"system.l3.WritebackDirty.misss::total\"]",
    "system.cpu0.l2cache.Read*Req.accesses::total": "stats_value[\"system.cpu0.l2cache.ReadCleanReq.accesses::total\"] + stats_value[\"system.cpu0.l2cache.ReadExReq.accesses::total\"] + stats_value[\"system.cpu0.l2cache.ReadSharedReq.accesses::total\"]",
    "system.cpu0.l2cache.Write*Req.accesses::total": "stats_value[\"system.cpu0.l2cache.WritebackClean.accesses::total\"] + stats_value[\"system.cpu0.l2cache.WritebackDirty.accesses::total\"]",
    "system.cpu0.l2cache.Read*Req.misses::total": "stats_value[\"system.cpu0.l2cache.ReadCleanReq.misses::total\"] + stats_value[\"system.cpu0.l2cache.ReadExReq.misses::total\"] + stats_value[\"system.cpu0.l2cache.ReadSharedReq.misses::total\"]",
    "system.cpu0.l2cache.Write*Req.misses::total": "stats_value[\"system.cpu0.l2cache.WritebackClean.misses::total\"] + stats_value[\"system.cpu0.l2cache.WritebackDirty.misses::total\"]",
    "system.membus.pktCount::total": "NOC_numAccess",
    "system.switch_cpus0.fetch2.fpInstructions + vecInstructions": "stats_value[\"system.switch_cpus0.fetch2.fpInstructions\"] + stats_value[\"system.switch_cpus0.fetch2.vecInstructions\"]"
    }

NOC_numAccess_dict = { # benchmark : [baseline, paramedox]
    "bwaves"    : [70732056, 244732056],
    "gcc"       : [12218648, 234218648],
    "mcf"       : [42723924, 258723924],
    "xalancbmk" : [346777464, 615777464],
    "deepsjeng" : [1100020, 170100020],
    "leela"     : [201916, 181201916],
    "exchange2" : [9132, 171009132],
    "xz"        : [2697996, 71746666],
    "cactuBSSN" : [65010796, 297010796],
    "lbm"       : [242679410, 409679410],
    "wrf"       : [68870804, 256870804],
    "cam4"      : [56852756, 249852756],
    "pop2"      : [76739748, 203739748],
    "imagick"   : [7555164, 109555164],
    "nab"       : [3988556, 176988556],
    "fotonik3d" : [168552, 204168552],
    "roms"      : [14634096, 82771031],
    "x264"      : [4814256, 165814256],
    "perlbench" : [3334024, 240334024],
    "omnetpp"   : [52259560, 294259560]
}

args = parser.parse_args()
num_cores = args.numCores
core_suffix = [""]
if num_cores > 1:
    core_suffix = [str(i) for i in range(num_cores)]

# Get appropriate value for NOC number of accesses
NOC_numAccess = ""
for benchmark, values in NOC_numAccess_dict.items():
    if benchmark in args.statFile:
        if "X2" in args.statFile or "A510" in args.statFile:
            NOC_numAccess = values[1]
        else:
            NOC_numAccess = values[0]
assert NOC_numAccess != "", "Could not determine the noc number of accesses"

# Get stats from file
stats_value = dict()
with open(args.statFile,'r') as f:
    # Find stats from file
    for line in f:
        for stat_key in grep_stats_key:
            if "switch_cpus0" in stat_key or "cpu0" in stat_key:
                for i in core_suffix:
                    if "switch_cpus0" in stat_key:
                        core_stat_key = stat_key.replace("switch_cpus0","switch_cpus" + i)
                    elif "cpu0" in stat_key:
                        core_stat_key = stat_key.replace("cpu0","cpu" + i)
                    if core_stat_key in line:
                        stats_value[core_stat_key] = eval(line.split()[1])
            elif stat_key in line:
                stats_value[stat_key] = eval(line.split()[1])
    # Put in default value for those not found in file
    grepped_keys = stats_value.keys()
    for stat_key in grep_stats_key:
        if "switch_cpus0" in stat_key or "cpu0" in stat_key:
            for i in core_suffix:
                if "switch_cpus0" in stat_key:
                    core_stat_key = stat_key.replace("switch_cpus0","switch_cpus" + i)
                elif "cpu0" in stat_key:
                    core_stat_key = stat_key.replace("cpu0","cpu" + i)
                if core_stat_key not in grepped_keys:
                    print(core_stat_key + " not found, using " + str(0) + " as default", file=sys.stderr)
                    stats_value[core_stat_key] = 0
        elif stat_key not in grepped_keys:
            print(stat_key + " not found, using " + str(0) + " as default", file=sys.stderr)
            stats_value[stat_key] = 0

# Calculate stats not in file
with open(args.templateFile,'r') as f:
    for line in f:
        outline = line
        found_in_calc = False
        # Calculate statistics if needed
        for stat_key in calc_stats_key:
            if "switch_cpus0" in stat_key or "cpu0" in stat_key:
                for i in core_suffix:
                    if "switch_cpus0" in stat_key:
                        core_stat_key = stat_key.replace("switch_cpus0","switch_cpus" + i)
                    elif "cpu0" in stat_key:
                        core_stat_key = stat_key.replace("cpu0","cpu" + i)
                    if core_stat_key in line:
                        if "switch_cpus0" in stat_key:
                            calc_val = eval(calc_stats_key[stat_key].replace("switch_cpus0","switch_cpus"+i))
                        elif "cpu0" in stat_key:
                            calc_val = eval(calc_stats_key[stat_key].replace("cpu0","cpu"+i))
                        outline = line.replace(core_stat_key,str(calc_val))
                        found_in_calc = True
            elif stat_key in line:
                outline = line.replace(stat_key,str(eval(calc_stats_key[stat_key])))
                found_in_calc = True
        # Otherwise get replaced with stats from file directly
        if not found_in_calc:
            for stat_key in grep_stats_key:
                if "switch_cpus0" in stat_key or "cpu0" in stat_key:
                    for i in core_suffix:
                        if "switch_cpus0" in stat_key:
                            core_stat_key = stat_key.replace("switch_cpus0","switch_cpus" + i)
                        elif "cpu0" in stat_key:
                            core_stat_key = stat_key.replace("cpu0","cpu" + i)
                        if core_stat_key in line:
                            outline = line.replace(core_stat_key,str(stats_value[core_stat_key]))
                elif stat_key in line:
                    outline = line.replace(stat_key,str(stats_value[stat_key]))
        print(outline.replace("\n",""))
    
