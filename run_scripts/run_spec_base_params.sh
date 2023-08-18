# Check args first
if [ "$#" -ne 1 ]; then
    echo "Run script, incorrect arguments"
    exit 1
fi

STATS=$1

cd ..
set -u
export BASE=$(pwd)
export SPEC=$(pwd)/SPEC/benchspec/CPU2006/

N=$(grep ^cpu\\scores /proc/cpuinfo | uniq |  awk '{print $4}')
M=$(grep MemTotal /proc/meminfo | awk '{print $2}')
G=$(expr $M / 2097152)
#P=$((G<N ? G : N))
P=6
i=0

for BENCH in xalancbmk mcf gcc # omnetpp astar xalancbmk #astar  #soplex #xalancbmk cactusADM zeusmp astar bwaves bzip2  calculix gamess gcc GemsFDTD gobmk gromacs h264ref hmmer lbm leslie3d libquantum mcf milc namd omnetpp povray sjeng soplex tonto
do
  ((i=i%P)); ((i++==0)) && wait
  (
  cd $SPEC
  IN=$(grep $BENCH $BASE/spec_confs/input.txt | awk -F':' '{print $2}'| xargs)
  BIN=$(grep $BENCH $BASE/spec_confs/binaries.txt | awk -F':' '{print $2}' | xargs)
  BINA=./$(echo $BIN)"_base.aarch64"
  echo $BINA
  ARGS=$(grep $BENCH $BASE/spec_confs/args.txt | awk -F':' '{print $2}'| xargs)
  cd *$BENCH/run/run_base_ref_aarch64.000*
  $BASE/run_scripts/gem5_scripts/run_checkpoint_params.sh "$BINA" "$ARGS" "$IN" "$BENCH"
  #$BASE/run_scripts/gem5_scripts/run_debug.sh "$BINA" "$ARGS" "$IN" "$BENCH"
  $BASE/run_scripts/gem5_scripts/run_test.sh "$BINA" "$ARGS" "$IN" "$BENCH" $STATS
  )  &
done
cd $BASE/run_scripts
