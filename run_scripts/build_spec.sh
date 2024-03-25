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
runspec --config=x86_64_base.cfg --action=build astar gcc mcf omnetpp soplex sphinx3 xalancbmk -I
runspec --config=x86_64_base.cfg --action=run --size=ref astar gcc mcf omnetpp soplex sphinx3 xalancbmk -n 1 -I
cd $BASE/run_scripts
