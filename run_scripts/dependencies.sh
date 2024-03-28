sudo apt-get install g++ python3 scons m4 swig
sudo apt-get install zlib1g-dev

#below for KVM support - https://www.gem5.org/documentation/general_docs/using_kvm/
sudo apt-get install qemu-kvm libvirt-daemon-system libvirt-clients bridge-utils
sudo adduser `id -un` libvirt
sudo adduser `id -un` kvm

cd ..
wget http://dist.gem5.org/dist/v21-2/kernels/x86/static/vmlinux-5.4.49
cd run_scripts
