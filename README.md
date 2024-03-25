Artefact Evaluation for the Triangel Temporal Prefetcher
==================================================

This repository contains artefacts and workflows 
to reproduce experiments from the ISCA 2024 paper
by S. Ainsworth and L. Mukhanov

" Triangel: A High-Performance, Accurate, Timely, On-Chip Temporal Prefetcher"

Including a modified gem5 simulator with implementations of Triangel and Triage (MICRO 2019) and scripts for compiling and running SPEC CPU2006.

Hardware pre-requisities
========================
* An x86-64 system preferably with sudo access (to install dependencies).

Software pre-requisites
=======================

* Linux operating system (We used Ubuntu 22.04)
* A SPEC CPU2006 iso, placed in the root directory of the repository (We used v1.0).

Installation and Building
========================

You can install this repository as follows:

```
git clone https://github.com/SamAinsworth/gem5-triangel
```

All scripts from here onwards are assumed to be run from the run_scripts directory, from the root of the repository:

```
cd gem5-triangel
cd run_scripts
```

To install software package dependencies, run

```
./dependencies.sh
```

Then, in the scripts folder, to compile the Triangel gem5 simulator, run
```
./build.sh
```

If you are running this as part of the ISCA artefact evaluation, we will provide the KVM checkpoints and Ubuntu image we evaluated on -- see the AE page for more details. Otherwise, you will be able to generate your own (with slightly different results due to differences in sampling) by following the details in the Generating Your Own Checkpoints section below.


Running experimental workflows
==============================

TODO

If any unexpected behaviour is observed, please report it to the author.




Validation of results
==============================

TODO


Generating Your Own Checkpoints
==============================

To compile SPEC CPU2006, first place your SPEC .iso file (other images can be used by modifying the build_spec06.sh script first) in the root directory of the repository (next to the file 'PLACE_SPEC_ISO_HERE').

Name it "cpu2006.iso" or change the script as appropriate.

Then, from the scripts directory, run

```
./build_spec06.sh
```

Once this has successfully completed, it will build and set up run directories for all of the benchmarks.

To build an image for gem5 to use these workloads (inspired by https://github.com/gem5/gem5-resources/tree/stable/src/x86-ubuntu)

Go to the gem5-triangel directory and run

```
cd util/m5
scons build/x86/out/m5
cd ../../disk-image
./build.sh
```

Move the resulting x86-ubuntu image into the root directory of the repository.

Next, it is time to generate the KVM checkpoints.

Run 

```
 sudo sh -c 'echo 1 >/proc/sys/kernel/perf_event_paranoid'
```

To allow gem5 to run in KVM mode (and repeat every time you generate KVM checkpoints on a freshly booted system, else gem5 will throw an error).

Now, for each workload in the spec-bootcfgs folder, boot up gem5 and generate checkpoints, e.g.

```
cd Checkpoints
mkdir Xalan
cd Xalan
../../build/X86/gem5.opt ../../configs/deprecated/example/fs.py -n 1 --mem-size=4GB --disk-image=../../x86-ubuntu --kernel=/home/sam/gem5-triangel/vmlinux-5.4.49 --cpu-type=X86KvmCPU --script=../../spec-bootcfgs/xalan.rcS
```

Once this has booted, telnet in (assuming gem5 is running on port 3456 as default) and run the provided script:

```
telnet localhost 3456
```

and inside the telnet, run

```
m5 readfile > /tmp/script;chmod 755 /tmp/script;/tmp/script
```

This will generate your first checkpoint. While it is running, check the simticks gem5 reports as the point at which the checkpoint is generated (START), and the point at which simulation terminates (END). Divide the difference by 20 (or N, where N is the number of checkpoints you wish to generate), to get the spacing (in picoseconds) between your checkpoints.

To generate the remaining checkpoints, run

```
../../build/X86/gem5.opt ../../configs/deprecated/example/fs.py -n 1 --mem-size=4GB --disk-image=../../x86-ubuntu --kernel=../../vmlinux-5.4.49 --cpu-type=X86KvmCPU --script=../../configs/boot/xalan.rcS -r 1 --take-checkpoints=X,Y --max-checkpoints=20 
```

Where X is (START+(END-START)/20) and Y is (END-START)/20 (or N rather than 20, as appropriate).

Once this is done, run

```
sed -i 's/system\.switch_cpus/system\.cpu/g' m5out/*/m5.cpt
```

To correct the core names inside the checkpoints (otherwise gem5 will throw an error when it tries to use them).

Now, you should be able to run simulations on each checkpoint, for example

```
../../build/X86/gem5.opt ../../configs/deprecated/example/fs.py -n 1 --mem-size=4GB --cpu-type=X86O3CPU -r 4 --maxinsts=5000000 --pl2sl3cache --triangel --standard-switch 50000000 --warmup=50000000 --mem-type=LPDDR5_5500_1x16_8B_BL32
```

In terms of troubleshooting, if any individual simSeconds result from a run of 5000000 instructions, such as above, reports it simulated more than a small fraction of a second, it is because the workload is not running, and instead the OS is idle. We found this sometimes happened particularly in earlier checkpoints. If so, delete these particular checkpoints and run with the rest (otherwise they dominate total simulated seconds). If no checkpoint works correctly, an error will have occurred in generating checkpoints; likely, your workload is not running (or has finished before your checkpoints were taken, in the case the workload is not followed by an m5exit as in the scripts above).

You can repeat the above for the other workloads to get the full set.

Customisation
=======
The prefetcher itself is implemented inside src/mem/cache/prefetch/ -- see Triangel.cc and Triage.cc. See Prefetcher.py in the same folder for the various options available -- and in configs/common/CacheConfig.py to see how they are connected. configs/common/Options.py shows the options available on the command line.





