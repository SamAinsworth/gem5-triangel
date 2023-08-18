# Check args first
if [ "$#" -ne 6 ]; then
    echo "Final script, incorrect arguments"
    exit 1
fi

BINA=$1
ARGS=$2
IN=$3
BENCH=$4
MAXINSTS=$5
M5OUTBASE=$6

export OUTPUTM5OUT=$M5OUTBASE/m5out

# Create results folder if it does not exist
mkdir -p $OUTPUTM5OUT

$BASE/build/ARM/gem5.opt \
    --outdir ${OUTPUTM5OUT}/cpt_${BENCH}_${NUM_O3} \
    --redirect-stdout --redirect-stderr \
    $BASE/configs/example/se.py \
    --output=app.out --errout=app.err \
    -n 1  --mem-size=16GB  -c "$BINA" -o="$ARGS" -i "$IN" \
    --cpu-type=TimingSimpleCPU --fast-forward=${MAXINSTS} \
    --maxinsts=1000000 --checkpoint-at-end --checkpoint-dir=${OUTPUTM5OUT}/cpt_${BENCH}_${NUM_O3}

exit 0
