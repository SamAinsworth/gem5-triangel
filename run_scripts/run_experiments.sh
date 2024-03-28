
cd ..
HOME=$(pwd)
STATS=$(date -I)
cd Checkpoints

for BENCH in *
do
	( 
	cd $HOME/Checkpoints/$BENCH
	
	COUNT=$(ls -d m5out/*/ | wc -l)
	for I in $(seq 1 $COUNT)
	do
		$HOME/build/X86/gem5.opt $HOME/configs/deprecated/example/fs.py --disk-image=$HOME/x86-ubuntu --kernel=$HOME/vmlinux-5.4.49 -n 1 --mem-size=4GB --cpu-type=X86O3CPU --triangel -r $I --maxinsts=5000000 --pl2sl3cache  --standard-switch 50000000 --warmup=50000000 --mem-type=LPDDR5_5500_1x16_BG_BL32  >> stdout$STATS.txt 2>&1
		cat m5out/stats.txt >> m5out/statscatTriangelBase$STATS.txt
		$HOME/build/X86/gem5.opt $HOME/configs/deprecated/example/fs.py --disk-image=$HOME/x86-ubuntu --kernel=$HOME/vmlinux-5.4.49 -n 1 --mem-size=4GB --cpu-type=X86O3CPU --triangel --triangelbloom -r $I --maxinsts=5000000 --pl2sl3cache  --standard-switch 50000000 --warmup=50000000 --mem-type=LPDDR5_5500_1x16_BG_BL32  >> stdout$STATS.txt 2>&1
		cat m5out/stats.txt >> m5out/statscatTriangelBloom$STATS.txt 
		$HOME/build/X86/gem5.opt $HOME/configs/deprecated/example/fs.py --disk-image=$HOME/x86-ubuntu --kernel=$HOME/vmlinux-5.4.49 -n 1 --mem-size=4GB --cpu-type=X86O3CPU --triage -r $I --maxinsts=5000000 --pl2sl3cache  --standard-switch 50000000 --warmup=50000000 --mem-type=LPDDR5_5500_1x16_BG_BL32  >> stdout$STATS.txt 2>&1
		cat m5out/stats.txt >> m5out/statscatTriageBase$STATS.txt
		$HOME/build/X86/gem5.opt $HOME/configs/deprecated/example/fs.py --disk-image=$HOME/x86-ubuntu --kernel=$HOME/vmlinux-5.4.49 -n 1 --mem-size=4GB --cpu-type=X86O3CPU --triagedeg4 -r $I --maxinsts=5000000 --pl2sl3cache  --standard-switch 50000000 --warmup=50000000 --mem-type=LPDDR5_5500_1x16_BG_BL32  >> stdout$STATS.txt 2>&1
		cat m5out/stats.txt >> m5out/statscatTriageDeg4$STATS.txt	
		$HOME/build/X86/gem5.opt $HOME/configs/deprecated/example/fs.py --disk-image=$HOME/x86-ubuntu --kernel=$HOME/vmlinux-5.4.49 -n 1 --mem-size=4GB --cpu-type=X86O3CPU -r $I --maxinsts=5000000 --pl2sl3cache  --standard-switch 50000000 --warmup=50000000 --mem-type=LPDDR5_5500_1x16_BG_BL32  >> stdout$STATS.txt 2>&1
		cat m5out/stats.txt >> m5out/statscatNoPF$STATS.txt	
	done 
	)&
done
	echo "Waiting for runs to finish. Check local stdout files in each folder for progress..."
	wait
	echo "Finished!"
cd $HOME/run_scripts
