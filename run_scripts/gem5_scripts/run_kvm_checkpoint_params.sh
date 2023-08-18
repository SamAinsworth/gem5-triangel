# Check args first
if [ "$#" -ne 4 ]; then
    echo "Final script, incorrect arguments"
    exit 1
fi

BINA=$1
ARGS=$2
IN=$3
BENCH=$4

export OUTPUT=$BASE/simresultskvm

# Create results folder if it does not exist
mkdir -p $OUTPUT

$BASE/build/X86/gem5.opt \
    --outdir $OUTPUT/m5out/${BENCH} \
    --redirect-stdout --redirect-stderr \
    $BASE/configs/deprecated/example/se.py --cpu-type=X86KvmCPU \
    --output=app_kvm_chkpt.out --errout=app_kvm_chkpt.err \
    -n 1  --mem-size=4GB  -c "$BINA" -o="$ARGS" -i "$IN" \
    --maxinsts=1000000000 --checkpoint-at-end --checkpoint-dir=$OUTPUT/m5out/${BENCH}
cp $OUTPUT/m5out/${BENCH}/stats.txt $OUTPUT/m5out/${BENCH}/statschkpt.txt 
cp $OUTPUT/m5out/${BENCH}/simerr $OUTPUT/m5out/${BENCH}/simerrkvm 
cp $OUTPUT/m5out/${BENCH}/simout $OUTPUT/m5out/${BENCH}/simoutkvm
exit 0
