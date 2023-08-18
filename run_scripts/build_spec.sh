cd ..
BASE=$(pwd)
mkdir specmnt
mkdir SPEC
sudo mount -o loop *.iso specmnt
cd specmnt
./install.sh -d ../SPEC
cd ..
sudo umount specmnt
rm -r specmnt
cp spec_confs/x86_64_base.cfg SPEC/config
cd SPEC
. ./shrc   
runspec --config=x86_64_base.cfg --action=build astar bwaves bzip2 cactusADM calculix gamess gcc GemsFDTD gobmk gromacs h264ref hmmer lbm leslie3d libquantum mcf milc namd omnetpp povray sjeng soplex tonto xalancbmk zeusmp -I
runspec --config=x86_64_base.cfg --action=run --size=ref astar bwaves bzip2 cactusADM calculix gamess gcc GemsFDTD gobmk -n 1 -I
cd $BASE/run_scripts
