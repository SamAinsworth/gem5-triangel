cd ..
BASE=$(pwd)
mkdir specmnt
mkdir SPEC17
sudo mount -o loop cpu2017*.iso specmnt
cd specmnt
./install.sh -d ../SPEC17
cd ..
sudo umount specmnt
rm -r specmnt
cp spec_confs/aarch64_17.cfg SPEC17/config
cd SPEC17
. ./shrc   
runcpu --config=aarch64_17.cfg --action=build perlbench_s gcc_s mcf_s omnetpp_s xalancbmk_s x264_s deepsjeng_s leela_s exchange2_s xz_s bwaves_s cactuBSSN_s lbm_s wrf_s cam4_s pop2_s imagick_s nab_s fotonik3d_s roms_s -I
runcpu --config=aarch64_17.cfg --action=run --size=ref perlbench_s gcc_s mcf_s omnetpp_s xalancbmk_s x264_s deepsjeng_s leela_s exchange2_s xz_s bwaves_s cactuBSSN_s lbm_s wrf_s cam4_s pop2_s imagick_s nab_s fotonik3d_s roms_s --noreportable --iterations=1  -I
cd $BASE/scripts
