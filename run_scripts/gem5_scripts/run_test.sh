# Check args first
if [ "$#" -ne 5 ]; then
    echo "Final script, incorrect arguments"
    exit 1
fi

BINA=$1
ARGS=$2
IN=$3
BENCH=$4
STATS=$5


export OUTPUT=$BASE/simresults

#--pl2sl3cache

# Create results folder if it does not exist
mkdir -p $OUTPUT

for I in 1
do
	$BASE/build/ARM/gem5.opt \
	    --outdir $OUTPUT/m5out/${BENCH} \
	    --redirect-stdout --redirect-stderr \
	    $BASE/configs/deprecated/example/se.py \
	    --output=app.out --errout=app.err \
	    --cpu-type=X2 \
	    --pl2sl3cache  \
	    -n 1  --mem-size=2GB -c "$BINA" -o="$ARGS" -i "$IN" \
	    -r $I  --checkpoint-dir=$OUTPUT/m5out/${BENCH} --triangel --maxinsts=10000000
	mv $OUTPUT/m5out/${BENCH}/stats.txt $OUTPUT/m5out/${BENCH}/statsNewTriangel$STATS$I.txt
	$BASE/build/ARM/gem5.opt \
	    --outdir $OUTPUT/m5out/${BENCH} \
	    --redirect-stdout --redirect-stderr \
	    $BASE/configs/deprecated/example/se.py \
	    --output=app.out --errout=app.err \
	    --cpu-type=X2 \
	    --pl2sl3cache  \
	    -n 1  --mem-size=2GB -c "$BINA" -o="$ARGS" -i "$IN" \
	    -r $I  --checkpoint-dir=$OUTPUT/m5out/${BENCH} --triage --maxinsts=10000000
	mv $OUTPUT/m5out/${BENCH}/stats.txt $OUTPUT/m5out/${BENCH}/statsNewTriage$STATS$I.txt
	$BASE/build/ARM/gem5.opt \
	    --outdir $OUTPUT/m5out/${BENCH} \
	    --redirect-stdout --redirect-stderr \
	    $BASE/configs/deprecated/example/se.py \
	    --output=app.out --errout=app.err \
	    --cpu-type=X2 \
	    --pl2sl3cache  \
	    -n 1  --mem-size=2GB -c "$BINA" -o="$ARGS" -i "$IN" \
	    -r $I  --checkpoint-dir=$OUTPUT/m5out/${BENCH} --maxinsts=10000000
	mv $OUTPUT/m5out/${BENCH}/stats.txt $OUTPUT/m5out/${BENCH}/statsNewNo$STATS$I.txt	
done



exit 0
