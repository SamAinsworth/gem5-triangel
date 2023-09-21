# Check args first
if [ "$#" -ne 1 ]; then
    echo "Run script, incorrect arguments"
    exit 1
fi

STATS=$1

echo "" > m5out/statscat$STATS.txt
for I in {2..21}
do
	/home/sam/gem5-sabotriage/build/X86/gem5.opt /home/sam/gem5-sabotriage/configs/deprecated/example/fs.py --disk-image=/home/sam/gem5-sabotriage/x86-ubuntu --kernel=/home/sam/gem5-sabotriage/vmlinux-5.4.49 -n 1 --mem-size=4GB --cpu-type=X86O3CPU --triage -r $I --maxinsts=5000000 --pl2sl3cache  --standard-switch 50000000 --warmup=50000000 --mem-type=LPDDR5_5500_1x16_BG_BL32
	cat m5out/stats.txt >> m5out/statscatTriage$STATS.txt
done

