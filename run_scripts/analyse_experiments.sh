compare1=TriageBase
compare2=TriageDeg4
compare3=TriangelBase
compare4=TriangelBloom


cd ..
HOME=$(pwd)
STATS=$(date -I)
cd Checkpoints

for BENCH in *
do
	cd $HOME/Checkpoints/$BENCH
	echo $BENCH

	echo "Name, NoPF , $compare1 , $compare2 , $compare3, $compare4"


	declare -A time
	declare -A speedup
	declare -A samples
	  
	for run in NoPF $compare1 $compare2 $compare3 $compare4
	do
	 time[$run]=$(grep simSeconds $1*/m5out/*$run*.txt | awk '{print $2}' | awk '{s+=($S1>1.0?0:$S1)} END {printf "%.5f\n", s}')
	 #Conditional above catches the bug with accidentally running Kernel code with tiny IPC dominating results.
	 speedup[$run]=$(echo "scale=5; (${time[NoPF]})/(${time[$run]})" | bc)
	 samples[$run]=$(grep simSeconds $1*/m5out/*$run*.txt | wc -l)
	done  


	 echo "Time, " ${time[NoPF]} ", " ${time[$compare1]} ", " ${time[$compare2]} ", "  ${time[$compare3]} ", "  ${time[$compare4]} 
	 echo "Speedup, " ${speedup[NoPF]} "," ${speedup[$compare1]} ", "  ${speedup[$compare2]} ", "  ${speedup[$compare3]} ", "  ${speedup[$compare4]} 
	 echo "Samples, " ${samples[NoPF]} ", " ${samples[$compare1]} ", "  ${samples[$compare2]} ", "  ${samples[$compare3]} ", "  ${samples[$compare4]} 
	  

	 
	declare -A dramreads
	declare -A dramwrites
	declare -A dramratio


	for run in NoPF $compare1 $compare2 $compare3 $compare4
	do
	 dramreads[$run]=$(grep bytesRead::tot $1*/m5out/*$run*.txt | awk '{print $2}' | awk '{s+=$S1} END {printf "%.0f\n", s}')
	 dramwrites[$run]=$(grep bytesWritten::tot $1*/m5out/*$run*.txt | awk '{print $2}' | awk '{s+=$S1} END {printf "%.0f\n", s}')
	 dramratio[$run]=$(echo "scale=5; (${dramreads[$run]}+${dramwrites[$run]})/(${dramreads[NoPF]} + ${dramwrites[NoPF]})" | bc)
	done
	 
	 echo "DRAM Reads, " ${dramreads[NoPF]} ", " ${dramreads[$compare1]} ", " ${dramreads[$compare2]} ", "  ${dramreads[$compare3]} ", "  ${dramreads[$compare4]} 
	 echo "DRAM Writes, " ${dramwrites[NoPF]} "," ${dramwrites[$compare1]} ", "  ${dramwrites[$compare2]} ", "  ${dramwrites[$compare3]} ", "  ${dramwrites[$compare4]} 
	 echo "DRAM Ratio, " ${dramratio[NoPF]} ", " ${dramratio[$compare1]} ", "  ${dramratio[$compare2]} ", "  ${dramratio[$compare3]} ", "  ${dramratio[$compare4]} 
	 
	 declare -A dramenergy
	declare -A l3energy
	declare -A totalenergy
	 
	declare -A l3s
	declare -A metas
	declare -A l3rat

	for run in NoPF $compare1 $compare2 $compare3 $compare4
	do
	 l3s[$run]=$(grep l3.overallAccesses::total $1*/m5out/*$run*.txt | awk '{print $2}' | awk '{s+=$S1} END {printf "%.0f\n", s}')
	 metas[$run]=$(grep l2cache.prefetcher.metadataAccess $1*/m5out/*$run*.txt | awk '{print $2}' | awk '{s+=$S1} END {printf "%.0f\n", s}')
	 l3rat[$run]=$(echo "scale=5; (${l3s[$run]}+${metas[$run]})/${l3s[NoPF]}" | bc)
	 
	 l3energy[$run]=$(echo "scale=5; (64*${l3s[$run]}+64*${metas[$run]})/(64*${l3s[NoPF]}+64*${metas[NoPF]}+25*${dramreads[NoPF]}+25*${dramwrites[NoPF]})" | bc)
	 dramenergy[$run]=$(echo "scale=5; (25*${dramreads[$run]}+25*${dramwrites[$run]})/(64*${l3s[NoPF]}+64*${metas[NoPF]}+25*${dramreads[NoPF]}+25*${dramwrites[NoPF]})" | bc)
	 totalenergy[$run]=$(echo "scale=5; ${l3energy[$run]} + ${dramenergy[$run]}" | bc)
	done
	 
	 echo "L3 Accesses, " ${l3s[NoPF]} ", " ${l3s[$compare1]} ", " ${l3s[$compare2]} ", "  ${l3s[$compare3]} ", "  ${l3s[$compare4]} 
	 echo "Metas, " ${metas[NoPF]} "," ${metas[$compare1]} ", "  ${metas[$compare2]} ", "  ${metas[$compare3]} ", "  ${metas[$compare4]} 
	 echo "L3 ratio, " ${l3rat[NoPF]} ", " ${l3rat[$compare1]} ", "  ${l3rat[$compare2]} ", "  ${l3rat[$compare3]} ", "  ${l3rat[$compare4]} 
	 
	 echo "L3 Energy, " ${l3energy[NoPF]} ", " ${l3energy[$compare1]} ", " ${l3energy[$compare2]} ", "  ${l3energy[$compare3]} ", "  ${l3energy[$compare4]} 
	 echo "DRAM Energy, " ${dramenergy[NoPF]} "," ${dramenergy[$compare1]} ", "  ${dramenergy[$compare2]} ", "  ${dramenergy[$compare3]} ", "  ${dramenergy[$compare4]} 
	 echo "Total Energy, " ${totalenergy[NoPF]} ", " ${totalenergy[$compare1]} ", "  ${totalenergy[$compare2]} ", "  ${totalenergy[$compare3]} ", "  ${totalenergy[$compare4]} 
	 
	 
	 
	 declare -A pfused
	declare -A pfunused
	declare -A accuracy

	for run in $compare1 $compare2 $compare3 $compare4
	do
	 pfused[$run]=$(grep l2cache.prefetcher.pfUseful $1*/m5out/*$run*.txt | awk '{print $2}' | awk '{s+=$S1} END {printf "%.0f\n", s}')
	 pfunused[$run]=$(grep l2cache.prefetcher.pfUnused $1*/m5out/*$run*.txt | awk '{print $2}' | awk '{s+=$S1} END {printf "%.0f\n", s}')
	 accuracy[$run]=$(echo "scale=5; (${pfused[$run]})/(${pfused[$run]}+${pfunused[$run]})" | bc)
	done
	 
	 echo "PF Used, " ", " ${pfused[$compare1]} ", " ${pfused[$compare2]} ", "  ${pfused[$compare3]} ", "  ${pfused[$compare4]} 
	 echo "PF Unused, "  "," ${pfunused[$compare1]} ", "  ${pfunused[$compare2]} ", "  ${pfunused[$compare3]} ", "  ${pfunused[$compare4]} 
	 echo "PF Accuracy, "  ", " ${accuracy[$compare1]} ", "  ${accuracy[$compare2]} ", "  ${accuracy[$compare3]} ", "  ${accuracy[$compare4]} 
	 
	declare -A coverage
	declare -A pfmissed

	for run in NoPF $compare1 $compare2 $compare3 $compare4
	do
	 pfmissed[$run]=$(grep l2cache.demandMisses::switch_cpus_1.data $1*/m5out/*$run*.txt | awk '{print $2}' | awk '{s+=$S1} END {printf "%.0f\n", s}')
	 coverage[$run]=$(echo "scale=5; 1-((${pfmissed[$run]})/(${pfmissed[NoPF]}))" | bc)
	done
	 
	 echo "PF Missed, " ${pfmissed[NoPF]} ", " ${pfmissed[$compare1]} ", " ${pfmissed[$compare2]} ", "  ${pfmissed[$compare3]} ", "  ${pfmissed[$compare4]} 
	 echo "PF Coverage, "  ", " ${coverage[$compare1]} ", "  ${coverage[$compare2]} ", "  ${coverage[$compare3]} ", "  ${coverage[$compare4]} 
	 
	  
	declare -A lookupcorrect
	declare -A lookupwrong
	declare -A lookupacc

	for run in $compare1 $compare2
	do
	lookupCorrect[$run]=$(grep l2cache.prefetcher.lookupCorrect $1*/m5out/*$run*.txt | awk '{print $2}' | awk '{s+=$S1} END {printf "%.0f\n", s}')
	lookupWrong[$run]=$(grep l2cache.prefetcher.lookupWrong $1*/m5out/*$run*.txt | awk '{print $2}' | awk '{s+=$S1} END {printf "%.0f\n", s}')
	lookupacc[$run]=$(echo "scale=5; (${lookupCorrect[$run]})/(${lookupCorrect[$run]}+${lookupWrong[$run]})" | bc)
	done

	echo "Lookup Accuracy, "  ", " ${lookupacc[$compare1]} ", "  ${lookupacc[$compare2]}

done
cd $HOME/run_scripts
