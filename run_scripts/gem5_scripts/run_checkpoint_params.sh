# Check args first
if [ "$#" -ne 4 ]; then
    echo "Final script, incorrect arguments"
    exit 1
fi

BINA=$1
ARGS=$2
IN=$3
BENCH=$4

export OUTPUT=$BASE/simresults

# Create results folder if it does not exist
mkdir -p $OUTPUT

$BASE/build/ARM/gem5.opt \
    --outdir $OUTPUT/m5out/${BENCH} \
    --redirect-stdout --redirect-stderr \
    $BASE/configs/deprecated/example/se.py \
    --output=app_chkpt.out --errout=app_chkpt.err \
    -n 1  --mem-size=2GB  -c "$BINA" -o="$ARGS" -i "$IN" \
    --maxinsts=10000000000 --checkpoint-at-end --checkpoint-dir=$OUTPUT/m5out/${BENCH}
cp $OUTPUT/m5out/${BENCH}/stats.txt $OUTPUT/m5out/${BENCH}/statschkpt.txt 
exit 0
