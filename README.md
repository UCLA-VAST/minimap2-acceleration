# Minimap2-acceleration

![][image-1] ![][image-2]

## <a name="started"></a>Getting Started

### Build testbed and generate test data

```bash
# Fetch code.
git clone https://github.com/UCLA-VAST/minimap2-acceleration.git

# Build testbed.
(cd testbed/ && make);

# Generate test data for PacBio genomic reads,
#   just as if invoke the Minimap2 command line tool.
testbed/minimap2 -ax map-pb ref.fa tgt.fa \
    --chain-dump-in in-1k.txt \
    --chain-dump-out out-1k.txt \
    --chain-dump-limit=1000 > /dev/null

# There are two files generated:
#   in-1k.txt: the input of the chaining function
#     for 1,000 reads.
#   out-1k.txt: the output of the corresponding
#     chaining tasks.
# You can use them to run benchmarks of different
#   kernels, and compare results to ensure
#   correctness.
```

### Build FPGA kernel and run benchmarks

```bash
# Set up your Xilinx SDx environment.
source /opt/tools/xilinx/SDx/2018.3/settings64.sh
ulimit -s unlimited

# Build HLS kernel for software emulation.
(cd kernel/hls/ && make csim-target);

# Simulate the kernel benchmark in software.
XCL_EMULATION_MODE=sw_emu kernel/hls/bin/2018.3/vcu1525/kernel \
	kernel/hls/bit/2018.3/vcu1525/kernel-csim.xclbin \
	in-1k.txt kernel-1k.txt

# Build HLS kernel for hardware emulation and run.
(cd kernel/hls/ && make cosim-target);
XCL_EMULATION_MODE=hw_emu kernel/hls/bin/2018.3/vcu1525/kernel \
	kernel/hls/bit/2018.3/vcu1525/kernel-cosim.xclbin \
	in-1k.txt kernel-1k.txt

# Generate bitstream and run on board.
(cd kernel/hls/ && make bitstream);
kernel/hls/bin/2018.3/vcu1525/kernel \
	kernel/hls/bit/2018.3/vcu1525/kernel-hw.xclbin \
	in-1k.txt kernel-1k.txt

# The result is slightly different from out-1k.txt
#   since we applied frequency optimizations,
#   but they are equivalent in functionality.
# If you want to reproduce the exact result,
#   comment out OPTIMIZE_DSP in kernel/hls/src/device_kernel.cpp
#   and modify the code accordingly.
```

### Build GPU kernel and run benchmarks

```bash
# Build CUDA kernel.
(cd kernel/cuda/ && make);

# Execute the kernel benchmark.
kernel/cuda/kernel in-1k.txt kernel-1k.txt

# Compare the results.
cmp out-1k.txt kernel-1k.txt
```

### Build CPU SIMD kernel and run benchmarks

```bash
# Source Intel Parallel Studio XE environment.
source /opt/tools/intel/parallel-studio/parallel_studio_xe_2019/psxevars.sh

# Build SIMD kernel.
(cd kernel/simd/ && make);

# Execute the kernel benchmark
#   with automatic thread control.
kernel/simd/kernel in-1k.txt kernel-1k.txt

# Advanced use: execute the kernel benchmark
#   with 14 threads, scatter affinity
#   and CPU binding for NUMA optimization.
KMP_AFFINITY=granularity=fine,scatter \
	OMP_NUM_THREADS=14 \
	numactl --cpubind=1 \
	kernel/simd/kernel \
		in-1k.txt kernel-1k.txt

# Compare the results.
cmp out-1k.txt kernel-1k.txt
```

## Table of Contents

- [Getting Started][1]
- [General Information][2]
	- [Introduction][3]
	- [Background][4]
	- [Methods][5]
- [Users' Guide][6]
	- [Obtain and Build Code][7]
	- [Generate Test Data][8]
	- [Run FPGA Benchmark][9]
	- [Run GPU Benchmark][10]
	- [Run CPU Benchmark][11]
	- [Evaluate Overlapping Results][12]
- [Developers' Guide][13]
	- [Directory Layout][14]
	- [Testbed][15]
	- [GPU Kernel][16]
	- [CPU Kernel][17]
- [Limitations and Notes][18]
- [Acknowledgement][19]

## <a name="general"></a> General Information

### <a name="intro"></a> Introduction

In genome sequencing, it is a crucial but time-consuming task to [detect potential overlaps][20] between any pair of the input reads, especially those that are ultra-long. The state-of-the-art overlapping tool [Minimap2][21] outperforms other popular tools in speed and accuracy. It has a single computing hot-spot, [chaining][22], that takes 70% of the time and needs to be accelerated.

We modify the chaining algorithm to reorder the operation sequence that transforms the algorithm into its hardware-friendly equivalence. We customize a fine-grained task dispatching scheme which could keep parallel PEs busy while satisfying the on-chip memory restriction. Moreover, we map the algorithm to a fully pipelined streaming architecture on FPGA using HLS, which achieves significant performance improvement. The same methodology applies to GPU and CPU SIMD implementation, and we also achieve decent speedups.

In this open source repository, we release (1) our HLS chaining algorithm implementation on FPGA, (2) CUDA chaining algorithm kernel on GPU and (3) optimized CPU code for chaining. They share the same benchmarking input/output interface and the same testbed so the comparison can be fair, and the correctness can be tested. The code is released under MIT license for further academic research and integration.

If you want to check out details or use our acceleration in your work, please see our paper and cite:

> L. Guo, J. Lau, Z. Ruan, P. Wei, and J. Cong, “[Hardware Acceleration of Long Read Pairwise Overlapping in Genome Sequencing: A Race Between FPGA and GPU][23],” in 2019 IEEE 27th International Symposium On Field-Programmable Custom Computing Machines (FCCM), April 2019.

### <a name="backg"></a> Background

Assembling long reads from third-generation sequencing into the original sequences without a reference whole genome typically uses three stages: Overlap, Layout, and Consensus (OLC). The overlap step checks where a pair of reads have parts in common. The layout step uses continuous reads with overlaps to build the result sequences. The consensus step picks the most likely nucleotide sequence.

![][image-3]

The first step of OLC assembly, overlap detection, is the performance bottleneck of the assembly process. Though similar to the read-to-read or read-to-reference alignment problem commonly used in the short-read assembly, overlap detection is a different problem. It could benefit from specialized algorithms that perform efficiently and robustly on high error rate long reads.

Steps of overlap detection include seeding and chaining:

* Seeding: (1) for a given set of reads, extract features from each read and (2) identify reads that share seeds significantly;
* Chaining: (3) find serials of  seed matches that share consistent distances on the two reads:

![][image-4]

We find chaining takes about 70% of the total execution time when Minimap2 is working as a long-read overlapper. Besides, other profiling results show chaining also takes a significant amount of time in other cases, e.g., 30% of the total time in reference-based assembly. This motivates our acceleration of the chaining algorithm.

![Time breakdown of Minimap2 working as an overlapper by \`gprof\`, \`mm\_chain\_dp\` is the function that performs chaining.][image-5]

### <a name="method"></a> Methods

## <a name="userg"></a> Users’ Guide

### <a name="obtain"></a> Obtain and Build Code

You can download the whole repository with:

```bash
git clone https://github.com/UCLA-VAST/minimap2-acceleration.git
```

In the following text, we assume you work in the cloned directory. For example, after you run the command above, you can change your working path with:

```bash
cd minimap2-acceleration
```

#### Build Testbed

You need to have a C compiler, GNU make and zlib development files installed to build the testbed software. You can build the testbed with the following command:

```bash
(cd testbed/ && make);
```

#### Build FPGA Kernel

You need to have SDAccel, Vivado HLS and Vivado installed. We use SDAccel as the interface for the kernel to communicate with the host. Before you get started, you may need to source the environment file provided by the tools, for example:

```bash
source /opt/tools/xilinx/SDx/2018.3/settings64.sh
ulimit -s unlimited
```

For software emulation, you can build the kernel with the following command:

```bash
(cd kernel/hls/ && make csim-target);
```

For hardware emulation:

```bash
(cd kernel/hls/ && make cosim-target);
```

And for bitstream generation:

```bash
(cd kernel/hls/ && make bitstream);
```

#### Build GPU Kernel

You need to have a C compiler and CUDA 10 installed. You can build the GPU kernel benchmark with the following command:

```bash
(cd kernel/cuda/ && make);
```

#### Build CPU Kernel

You need to have Intel Parallel Studio XE installed. The code is tested with Intel Parallel Studio XE 2019. You need to first setup the compilation environment with:

```bash
source /opt/tools/intel/parallel-studio/parallel_studio_xe_2019/psxevars.sh
```

The path may differs in your environment. Please consult with your system administrator for the location.

You can build the CPU SIMD kernel with the following command:

```bash
(cd kernel/simd/ && make);
```

### <a name="generate"></a> Generate Test Data

We tested our implementation with the public Caenorhabditis Elegans 40x Sequence Coverage dataset obtained from a PacBio sequencer. You may want to first obtain the dataset from [here][24]. You can use any of the download tools it recommended on the page to obtain the `.fastq` files, and combine them with the `cat` command. In the following text, we assume you have the combined genome read file at `~/c_elegans40x.fastq`.

To generate the test data for later benchmarking, you can run:

```bash
./testbed/minimap2 -ax map-pb \
    ~/c_elegans40x.fastq ~/c_elegans40x.fastq \
    --chain-dump-in in-30k.txt \
    --chain-dump-out out-30k.txt \
    --chain-dump-limit=30000 > /dev/null
```

This command generates `in-30k.txt`, the input file of the chaining function for 30,000 reads that we use later in the benchmark sections. Moreover, `out-30k.txt`, the expected output file for the corresponding chaining tasks. You can compare it with the output from kernel executions.

### <a name="fpga"></a> Run FPGA Benchmark

With the test data generated in [Generate Test Data][25] section, you can simulate the built HLS kernel on CPU with:
```bash

# Simulate the kernel benchmark in software.
XCL_EMULATION_MODE=sw_emu kernel/hls/bin/2018.3/vcu1525/kernel \
	kernel/hls/bit/2018.3/vcu1525/kernel-csim.xclbin \
	in-30k.txt kernel-30k.txt
```

This command reads the input anchors from file `in-30k.txt` to host memory, simulate transferring of the data to FPGA onboard memory through PCIe, simulate the kernel for computation, transfer it back to host memory and write computed scores and predecessors into file `kernel-30k.txt`.

You may need to generate smaller data for hardware emulation because it runs simulation underneath. Using a large data set will result in long simulation times. You can do the emulation with:

```bash
XCL_EMULATION_MODE=hw_emu kernel/hls/bin/2018.3/vcu1525/kernel \
	kernel/hls/bit/2018.3/vcu1525/kernel-cosim.xclbin \
	in-small.txt kernel-small.txt
```

For onboard execution, you can run:

```bash
kernel/hls/bin/2018.3/vcu1525/kernel \
	kernel/hls/bit/2018.3/vcu1525/kernel-hw.xclbin \
	in-30k.txt kernel-30k.txt
```

Note that the result from FPGA kernel is slightly different from the file testbed generated. We applied frequency optimizations, which makes the result different numerically but equivalent in functionality. If you expect a exact result, you can modify the code and comment out `OPTIMIZE_DSP` in `kernel/hls/src/device_kernel.cpp` file.

### <a name="gpu"></a> Run GPU Benchmark

With the test data generated in [Generate Test Data][26] section, you can execute the built GPU kernel with:

```bash
kernel/cuda/kernel in-30k.txt kernel-30k.txt
```

This command reads the input anchors from file `in-30k.txt` to host memory, transfer the data to GPU global memory through PCIe, run the GPU kernel for computation, transfer it back to host memory and write computed scores and predecessors into file `kernel-30k.txt`.

This command prints three metrics on standard output. The first time is from host memory to GPU global memory through PCIe. The second is the transferring time plus GPU kernel total execution time. The third is all time end-to-end, including GPU/host communication and GPU execution.

For example, with NVIDIA Tesla P100 GPU, the output is:

```
****** kernel took 0.834192 seconds to transfer in data
 ***** kernel took 2.032814 seconds to transfer in and execute
 ***** kernel took 2.688967 seconds for end-to-end
```

NOTE: If you are using a GPU other than NVIDIA Tesla P100 GPU, you may want to tune the GPU specific parameters. Please see `kernel/cuda/include/common.h` and [GPU Kernel][27] section for details.

To check the correctness, you can run:

```bash
cmp out-30k.txt kernel-30k.txt
```

If it outputs nothing as expected, this means the output of the acceleration kernel is correct.

### <a name="cpu"></a> Run CPU Benchmark

With the test data generated in [Generate Test Data][28] section, you can execute the built CPU SIMD kernel with:

```bash
kernel/simd/kernel in-30k.txt kernel-30k.txt
```

This command reads the input anchors from file `in-30k.txt`, run the CPU SIMD kernel for computation, and write computed scores and predecessors into file `kernel-30k.txt`.

This command prints one metric on standard output, which is the total kernel execution time on CPU. For example, on a 14 threaded Intel Xeon CPU E5-2680, the output is:

```bash
 ***** kernel took 4.358382 seconds to finish
```

You may want to have control over how many threads to execute on CPU. You can specify it with an environment variable `OMP_NUM_THREADS`. Besides, if you want to control over the thread affinity with CPU cores, you can specify `KMP_AFFINITY`. You can see [Intel® C++ Compiler 19.0 Developer Guide and Reference][29] for details. For example, the following command runs the kernel on 14 threads, and use scatter affinity for threads:

```bash
KMP_AFFINITY=granularity=fine,scatter \
	OMP_NUM_THREADS=14 \
	kernel/simd/kernel \
		in-30k.txt kernel-30k.txt
```

To experiment with NUMA affinity, you can simply bind all execution to a single CPU core with `numactl --cpubind=1`.

To check the correctness, you can run:

```bash
cmp out-30k.txt kernel-30k.txt
```

If it outputs nothing as expected, this means the output of the acceleration kernel is correct.

### <a name="eval"></a> Evaluate Overlapping Results

## <a name="develg"></a> Developers' Guide

### <a name="layout"></a> Directory Layout

* **README.md**: this file.
* **testbed**: a modified version of Minimap2 that supports test data generation.
	* **testbed/main.c**: the main entry function, and definition of the added command line options.
	* **testbed/chain.c**: the source file of the modifications in the chaining algorithm, and also the code logic for dumping input/output files.
* **kernel/hls**: an HLS implementation of Minimap2 chaining algorithm for Xilinx FPGA.
* **kernel/cuda**: a CUDA implementation of Minimap2 chaining algorithm for NVIDIA Tesla P100 GPU. Also tested on K40c and V100 GPU with different parameters.
	* **kernel/cuda/device/device\_kernel.cu**: the GPU kernel for chaining algorithm.
	* **kernel/cuda/device/device\_kernel\_wrapper.cu**: the wrapper function for transferring data, executing GPU kernel, and the measurement of execution time.
	* **kernel/cuda/include/common.h**: the parameters for GPU execution, including the CUDA stream count, the block size, the thread unrolling factor and the tiling size.
* **kernel/simd**: a SIMD implementation using pragmas of Intel C Compiler.
	* **kernel/simd/src/host\_kernel.cpp**: the CPU SIMD kernel implementation for the chaining algorithm.

### <a name="testbed"></a> Testbed

#### Command Line Tool

The testbed is a modified version of [Minimap2][30] software and inherits most of the command line options from Minimap2. Therefore, you can check out the [manual reference pages][31] of Minimap2 to see what is available in the testbed program. You can simply use it as if you invoke the Minimap2 command line tool.

The modified software parse three additional command line options:

* `--chain-dump-in`: the output file to store input of the chaining algorithm. In function invocation of `mm_chain_dp` function, we output its arguments to the specified file. The format of this file is documented later.
* `--chain-dump-out`: the output file to store the output of the chaining algorithm. After the function `mm_chain_dp` computed the desired results with unoptimized code, we dump the results into this file. The format is documented later. By comparing accelerators’ result with this file, we can know if we obtained the correct answer.
* `--chain-dump-limit`: this option specifies input and output of how many reads is dumped into the files.  For example, if you specify it as 1000, the tool dumps anchors and chaining output for 1000 reads in the reference file (first argument) to all reads in the target file (second argument).

We modified the chaining algorithm in the testbed program to be equivalent to our implemented accelerations. Without using the additional command options, you can execute it to simulate the end-to-end output if you integrate our kernels into the original software.

#### Dump Files

The file dumped by `--chain-dump-in` and `--chain-dump-out` options are the main interfaces with the benchmark software for our kernels.

##### Input File

The dumped input file is an EOF terminated plain text file. It consists of multiple continuous blocks until the end of the file. 

For each block, the first line is five numbers, which is `n, avg_qspan, max_dist_x, max_dist_y, bw` used in the chaining function. `n` is an integer for the count of anchors, `avg_qspan` is a floating point number for the average length of anchors used in the score computation, and the three remaining parameters are integer thresholds used in the Minimap2 chaining algorithm.

Following is `n` lines, each line is consists of four integers separated with a tab character. The numbers are `x, y, w, tag` as defined in the paper, indicating an exact match between read strings a and b of length w: a[`x`-`w`+1] .. a[`x`] = b[`y`-`w`+1] .. b[`y`], and anchors with different tag value are from different read pairs. At the ending of one task, there will be a line of `EOR`.

A sample dumped input file:

```
21949	29.074217	5000	5000	500
2	52	41	52
2	61	35	61
...
EOR
...
```

##### Output File

The dumped output file is also an EOF terminated plain text file. It also has multiple continuous blocks until the end of the file. 

The first line is an integer `n` which is the count of anchors. Following is `n` lines, each line is two integer values `f` and `p`. `f` is the score for the best chain ending at the corresponding anchor computed in the chaining algorithm, while `p` is the best predecessor anchor for this chain (-1 means the chain has only one anchor which is the current one). At the ending of one task, there will be a line of `EOR`.

A sample dumped output file:

```
21949
41	-1
50	0
...
EOR
...
```

### <a name="gpu-kernel-devel"></a>GPU Kernel

#### Command Line Tool

The command line interface for the benchmark is simple. You can invoke the command with two files as options. The first option is the dumped input file from the testbed program. The second option is the path to the file you want the kernel to dump results. Please note that do not pass the dumped output file as the second option. You need that file to compare for correctness with the output of the kernel. For example:

```
./kernel/cuda/kernel in-30k.txt kernel-30k.txt
```

#### Communication Layout

In our GPU kernel implementation, we use a similar memory data layout to our FPGA design. Please note that in software integration, this scheme is not required as GPU can schedule tasks itself. While in this benchmark, for reducing the task switching overhead and have a fair comparison with FPGA, the workload are batched and tiled into chunks as done in FPGA kernel design. For batching, we use `STREAM_NUM * BLOCK_NUM * THREAD_FACTOR` as the task count. We iterate and concatenate reads to the end of the first idle task queue. Then, we split tasks into chunks of `TILE_SIZE` and invoke the kernel to process tasks chuck by chuck. Each chunk adds its last 64 elements to the front of the next chuck so that the scores can be correctly computed.

#### GPU Specific Parameters

There are several GPU specific parameters in the code. Before you port the benchmark into another device, you may need to modify them for better performance.

In `kernel/cuda/include/common.h`, you can modify the parameters related to device properties:

* `STREAM_NUM`: a parameter to decide how many CUDA streams are mapped to task-level parallelism. In most cases, it should be 1 because using only thread block is sufficient.
* `BLOCK_NUM`: a parameter to decide how many CUDA thread blocks are generated to utilize task-level parallelism. This value should be equal to, or multiple of the count of streaming multiprocessor (SM) times the maximum block count in one SM. For example, for NVIDIA Tesla P100, it can be 1792. For V100, it can be 2560. For K40c, it can be 240.
* `THREAD_FACTOR`: a parameter to decide if task-level parallelism is mapped into CUDA threads in one thread block. For example, if this value is 2, there are 128 threads (4 warps) in one thread block handling two different tasks. For GPUs before Kepler architecture, for example, NVIDIA Tesla K40c, there are only 16 thread blocks in one SM. In order to achieve full occupancy, this value should be 2 to have 64 warps in one SM. For recent GPUs, this value should be 1.
* `TILE_SIZE`: a parameter to determine how many anchors per task are processed in one batch. We recommend 1024.

Sample configuration for K40c:

```c
#define STREAM_NUM (1)
#define BLOCK_NUM (240)
#define THREAD_FACTOR (2)
#define TILE_SIZE (1024)
```

In `kernel/cuda/device/device_kernel.cu`, you can decide if you use shared memory to buffer global memory access. If you want so, you can uncomment the line:

```c
//#define USE_LOCAL_BUFFER
```

However, this may reduce occupancy. You can experiment to see if the size of shared memory in your device is sufficient.

### <a name="cpu-kernel-devel"></a>CPU Kernel

#### Command Line Tool

The command line interface for the benchmark is simple. You can invoke the command with two files as options. The first option is the dumped input file from the testbed program. The second option is the path to the file you want the kernel to dump results. Please note that do not pass the dumped output file as the second option. You need that file to compare for correctness with the output of the kernel. For example:

```
./kernel/simd/kernel in-30k.txt kernel-30k.txt
```

#### OpenMP Parallelism and Thread Affinity

To control over parallelization, you can pass environment variables. For example:

* `OMP_NUM_THREADS`: the count of spawned OpenMP threads. `OMP_NUM_THREADS=14` instructs OpenMP to create 14 threads as the thread pool for computation of tasks.
* `KMP_AFFINITY`: a parameter to restrict execution of threads to a subset of physical cores so that the cache can be reused after context switching. See [Intel C++ Compiler 19.0 Developer Guide and Reference][32] for details.

Besides, you may want to modify the parallelization scheduling strategy used by OpenMP. You can modify the `schedule(guided)` parameter in `kernel/simd/src/host_kernel.cpp`:

```
#pragma omp parallel for schedule(guided)
```

To restrict the execution of the program to CPU cores in specified NUMA nodes, you can use [`numactl(8)`][33] tool.

If you want to implement thread affinity in the original Minimap2 software, you can modify `kthread.c` file with changes similar to:

```c
diff --git a/kthread.c b/kthread.c
index ffdf940..7a9373c 100644
--- a/kthread.c
+++ b/kthread.c
@@ -1,5 +1,8 @@
+#define _GNU_SOURCE
 #include <pthread.h>
 #include <stdlib.h>
+#include <errno.h>
+#include <stdlib.h>
 #include <limits.h>
 #include <stdint.h>
 #include "kthread.h"
@@ -63,6 +66,13 @@ void kt_for(int n_threads, void (*func)(void*,long,int), void *data, long n)
                for (i = 0; i < n_threads; ++i)
                        t.w[i].t = &t, t.w[i].i = i;
                for (i = 0; i < n_threads; ++i) pthread_create(&tid[i], 0, ktf_worker, &t.w[i]);
+        		for (i = 0; i < n_threads; ++i) {
+            		cpu_set_t cpuset;
+            		CPU_ZERO(&cpuset);
+            		CPU_SET(i, &cpuset);
+            		int s = pthread_setaffinity_np(tid[i], sizeof(cpu_set_t), &cpuset);
+            		if (s != 0) return;
+        		}
                for (i = 0; i < n_threads; ++i) pthread_join(tid[i], 0);
                free(tid); free(t.w);
        } else {
```

If you want to implement NUMA optimizations in the original Minimap2 software, you need to allocate all data passed to the chaining algorithm in the corresponding NUMA memory node. To do so, you can use [`numa(3)`][34] to allocate, or [Silo][35] for cross-platform support.

#### Memory Alignment

If you want to integrate the CPU kernel into the original software, please remember to align all data. You can check out `kernel/simd/src/host_kernel.cpp` to see how to allocate aligned memory with `aligned_alloc` function.

Besides, remember to keep all alignment annotations of local variables, and `__assume_aligned` macro. Otherwise, the compiler may assume the data to be unaligned and produce suboptimal assembly code.

#### Vectorization

For portability, we transform loops into parallelizable form and implement all vectorization instructions with `#pragma simd` pragma. If you want to port the acceleration to AVX512 or future instruction set, you may change `-xAVX2` to `-xAVX512` in the `Makefile`. If you want the program to be portable to machines with no AVX2 support, you can remove `-axAVX2` in the `Makefile`.

To check if your CPU has AVX2 support, you can run the following command:

```c
cat /proc/cpuinfo | grep avx2 | head -1
```

If it outputs something that starts with `flags`, the CPU can run AVX2 programs.

## <a name="limit"></a> Limitations and Notes

* Our accelerated kernels do not support spliced long reads. For example, acceleration of `minimap2 -ax splice ref.fa tgt.fa` is not yet supported.
* Our accelerations are not yet integrated into the software. The code in this repository contains kernels for the chaining function itself, which can be driven by testbed generated chaining data. File input and output takes significant time in benchmarking, and we don’t count the time as part of the kernel execution. To achieve end-to-end acceleration, integration is required.
* We use an assumption that the quality of output does not degrade when we choose the lookup depth of the dynamic programming algorithm to be 64, based on the claim in Li, H. (2018).
* We inherit most of [limitations of Minimap2][36].
* For integration, we recommend implementing the whole `mm_chain_dp` function in all accelerations solutions to reduce output communication. The reason we choose the score value as the output point is that we can better evaluate the correctness in fine grain. We also recommend integrating the seeding part.

## <a name="ack"></a> Acknowledgement

We directly used and modified the code from Heng Li’s Minimap2 to generate test data for chaining algorithm. We also implement all kernels of the chaining algorithm based on Heng Li’s paper:

> Li, H. (2018). Minimap2: pairwise alignment for nucleotide sequences. *Bioinformatics*, **34**:3094-3100. [doi:10.1093/bioinformatics/bty191][37]

This research is supported by CRISP, one of six centers in JUMP, a Semiconductor Research Corporation (SRC) program and the contributions from the member companies under the Center for Domain-Specific Computing (CDSC) Industrial Partnership Program.

---

![][image-6] 

[1]:	#started
[2]:	#general
[3]:	#intro
[4]:	#backg
[5]:	#method
[6]:	#userg
[7]:	#obtain
[8]:	#generate
[9]:	#fpga
[10]:	#gpu
[11]:	#cpu
[12]:	#eval
[13]:	#develg
[14]:	#layout
[15]:	#testbed
[16]:	#gpu-kernel-devel
[17]:	#cpu-kernel-devel
[18]:	#limit
[19]:	#ack
[20]:	http://www.cs.jhu.edu/~langmea/resources/lecture_notes/assembly_olc.pdf
[21]:	https://github.com/lh3/minimap2
[22]:	https://doi.org/10.1093/bioinformatics/bty191
[23]:	http://vast.cs.ucla.edu/sites/default/files/publications/minimap2-acc-approved.pdf
[24]:	http://datasets.pacb.com.s3.amazonaws.com/2014/c_elegans/list.html
[25]:	#generate
[26]:	#generate
[27]:	#gpu-kernel-devel
[28]:	#generate
[29]:	https://software.intel.com/en-us/cpp-compiler-developer-guide-and-reference-thread-affinity-interface-linux-and-windows
[30]:	https://github.com/lh3/minimap2
[31]:	https://lh3.github.io/minimap2/minimap2.html
[32]:	https://software.intel.com/en-us/cpp-compiler-developer-guide-and-reference-thread-affinity-interface-linux-and-windows
[33]:	https://linux.die.net/man/8/numactl
[34]:	http://man7.org/linux/man-pages/man3/numa.3.html
[35]:	https://github.com/stanford-mast/Silo
[36]:	https://github.com/lh3/minimap2#limit
[37]:	https://doi.org/10.1093/bioinformatics/bty191

[image-1]:	https://img.shields.io/badge/Version-Experimental-green.svg
[image-2]:	https://img.shields.io/bower/l/bootstrap.svg
[image-3]:	https://user-images.githubusercontent.com/843780/58130406-c7fa6280-7bd0-11e9-8880-9617bcbd3171.png
[image-4]:	https://user-images.githubusercontent.com/843780/58130612-4b1bb880-7bd1-11e9-86f4-81fe4f1be4d1.png
[image-5]:	https://user-images.githubusercontent.com/843780/58130132-2ffc7900-7bd0-11e9-92f5-d5fe437c5785.png
[image-6]:	http://vast.cs.ucla.edu/sites/default/themes/CADlab_cadlab/images/logo.png