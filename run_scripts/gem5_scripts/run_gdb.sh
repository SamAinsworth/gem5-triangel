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

gdb --args $BASE/build/ARM/gem5.opt \
    --outdir $OUTPUT/m5out/${BENCH} \
    --redirect-stdout --redirect-stderr \
    $BASE/configs/example/se.py \
    --output=app.out --errout=app.err \
    --cpu-type=X2 \
    --pl2sl3cache \
    -n 1  --mem-size=2GB -c "$BINA" -o="$ARGS" -i "$IN" \
    --maxinsts=100000000 -r 1  --checkpoint-dir=$OUTPUT/m5out/${BENCH}

exit 0
