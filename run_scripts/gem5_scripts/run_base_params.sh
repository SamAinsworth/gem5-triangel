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
    --outdir $BASE/m5out/cpt_${BENCH}_${NUM_O3} \
    --redirect-stdout --redirect-stderr \
    $BASE/configs/example/se.py \
    --output=app.out --errout=app.err \
    -n 1  --mem-size=2GB  -c "$BINA" -o="$ARGS" -i "$IN" \
    --maxinsts=1000000000 --r 1 --checkpoint-dir=$BASE/m5out/cpt_${BENCH}

exit 0
