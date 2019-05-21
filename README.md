# Minimap2-acceleration

![][image-1] ![][image-2]

## <a name="started"></a>Getting Started

### Build testbed and generate test data

```bash
# Build testbed
(cd testbed && make);

# Generate test data for PacBio genomic reads,
#   just as if invoke the Minimap2 command line tool.
./testbed/minimap2 -ax map-pb ref.fa tgt.fa \
    --chain-dump-in in-1k.txt \
    --chain-dump-out out-1k.txt \
    --chain-dump-limit=1000 > /dev/null

# There are two files generated:
#   in-1k.txt: the input of chaining function for 1,000 reads.
#   out-1k.txt: the output of the corresponding chaining tasks.
# You can use them to run benchmarks of different kernels,
#   and compare results to ensure correctness.
```

### Build FPGA kernel and run benchmarks

### Build GPU kernel and run benchmarks

### Build CPU SIMD kernel and run benchmarks

## Table of Contents

- [Getting Started][1]
- [General Information][2]
	- [Introduction][3]
	- [Background][4]
	- [Methods][5]
- [Users' Guide][6]
	- [Obtain and Build Code][7]
	- [Generate Test Data][8]
	- [Run FPGA benchmark][9]
	- [Run GPU benchmark][10]
	- [Run CPU benchmark][11]
	- [Evaluate Overlapping Results][12]
- [Developers' Guide][13]
	- [Directory Layout][14]
	- [Testbed][15]
- [Limitations and Notes][16]
- [Acknowledgement][17]

## <a name="general"></a> General Information

### <a name="intro"></a> Introduction

In genome sequencing, it is a crucial but time-consuming task to [detect potential overlaps][18] between any pair of the input reads, especially those that are ultra-long. The state-of-the-art overlapping tool [Minimap2][19] outperforms other popular tools in speed and accuracy. It has a single computing hot-spot, [chaining][20], that takes 70% of the time and needs to be accelerated.

We modify the chaining algorithm to reorder the operation sequence that transforms the algorithm into its hardware-friendly equivalence. We customize a fine-grained task dispatching scheme which could keep parallel PEs busy while satisfying the on-chip memory restriction. Moreover, we map the algorithm to a fully pipelined streaming architecture on FPGA using HLS, which achieves significant performance improvement. The same methodology applies to GPU and CPU SIMD implementation, and we also achieve decent speedups.

In this open source repository, we release (1) our HLS chaining algorithm implementation on FPGA, (2) CUDA chaining algorithm kernel on GPU and (3) optimized CPU code for chaining. They share the same benchmarking input/output interface and the same testbed so the comparison can be fair, and the correctness can be tested. The code is released under MIT license for further academic research and integration.

If you want to check out details or use our acceleration in your work, please see our paper and cite:

> L. Guo, J. Lau, Z. Ruan, P. Wei, and J. Cong, “[Hardware Acceleration of Long Read Pairwise Overlapping in Genome Sequencing: A Race Between FPGA and GPU][21],” in 2019 IEEE 27th International Symposium On Field-Programmable Custom Computing Machines (FCCM), April 2019.

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

You need to have a C compiler, GNU make and zlib development files installed to build the testbed software:

```bash
(cd testbed && make);
```


#### Build FPGA Kernel

#### Build GPU Kernel

#### Build CPU Kernel

### <a name="generate"></a> Generate Test Data

We tested our implementation with the public Caenorhabditis Elegans 40x Sequence Coverage dataset obtained from a PacBio sequencer. You may want to first obtain the dataset from [here][22]. You can use any of the download tools it recommended on the page to obtain the `.fastq` files, and combine them with the `cat` command. In the following text, we assume you have the combined genome read file at `~/c_elegans40x.fastq`.

To generate the test data for later benchmarking, you can run:

```bash
./testbed/minimap2 -ax map-pb \
    ~/c_elegans40x.fastq ~/c_elegans40x.fastq \
    --chain-dump-in in-30k.txt \
    --chain-dump-out out-30k.txt \
    --chain-dump-limit=30000 > /dev/null
```

This command will generate `in-30k.txt`, the input file of the chaining function for 30,000 reads that we will use later in the benchmark sections. Moreover, `out-30k.txt`, the expected output file for the corresponding chaining tasks. You can compare it with the output from kernel executions.

### <a name="fpga"></a> Run FPGA benchmark

### <a name="gpu"></a> Run GPU benchmark

### <a name="cpu"></a> Run CPU benchmark

### <a name="eval"></a> Evaluate Overlapping Results

## <a name="develg"></a> Developers' Guide

### <a name="layout"></a> Directory Layout

* **README.md**: this file.
* **testbed**: a modified version of Minimap2 that supports test data generation.
	* **testbed/main.c**: the main entry function, and definition of the added command line options.
	* **testbed/chain.c**: the source file of the modifications in the chaining algorithm, and also the code logic for dumping input/output files.
* **kernel/hls**: an HLS implementation of Minimap2 chaining algorithm for Xilinx FPGA.
* **kernel/cuda**: a CUDA implementation of Minimap2 chaining algorithm for NVIDIA Tesla P100 GPU. Also tested on K40c and V100 GPU with different parameters.
* **kernel/simd**: a SIMD implementation using pragmas of Intel C Compiler.

### <a name="testbed"></a> Testbed

#### Command Line Tool

The testbed is a modified version of [Minimap2][23] software and inherits most of the command line options from Minimap2. Therefore, you can check out the [manual reference pages][24] of Minimap2 to see what is available in the testbed program. You can simply use it as if you invoke the Minimap2 command line tool.

The modified software parse three additional command line options:

* `--chain-dump-in`: the output file to store input of the chaining algorithm. In function invocation of `mm_chain_dp` function, we output its arguments to the specified file. The format of this file is documented later.
* `--chain-dump-out`: the output file to store the output of the chaining algorithm. After the function `mm_chain_dp` computed the desired results with unoptimized code, we dump the results into this file. The format is documented later. By comparing accelerators’ result with this file, we can know if we obtained the correct answer.
* `--chain-dump-limit`: this option specify input and output of how many reads is dumped into the files.  For example, if you specify it as 1000, the tool dumps anchors and chaining output for 1000 reads in the reference file (first argument) to all reads in the target file (second argument).

We modified the chaining algorithm in the testbed program to be equivalent to our implemented accelerations. Without using the additional command options, you can execute it to simulate the end-to-end output if you integrate our kernels into the original software.

#### Dump Files

The file dumped by `--chain-dump-in` and `--chain-dump-out` options are the main interfaces with the benchmark software for our kernels.

##### Input File

The dumped input file is an EOF terminated plain text file. It consists of multiple continuous blocks until the end of the file. 

For each block, the first line is five numbers, which is `n, avg_qspan, max_dist_x, max_dist_y, bw` used in the chaining function. `n` is an integer for the count of anchors, `avg_qspan` is a floating point number for the average length of anchors used in the score computation, and the three remaining parameters are integer thresholds used in the Minimap2 chaining algorithm.

Following is `n` lines, each line is consists of four integers separated with a tab character. The numbers are `x, y, w, tag` as defined in the paper, indicating an exact match between read strings a and b of length w: a[`x`-`w`+1] .. a[`x`] = b[`y`-`w`+1] .. b[`y`], and anchors with different tag value are from different read pairs.

A sample dumped input file:

```
21949	29.074217	5000	5000	500
2	52	41	52
2	61	35	61
...
```

##### Output File

The dumped output file is also an EOF terminated plain text file. It also has multiple continuous blocks until the end of the file. 

The first line is an integer `n` which is the count of anchors. Following is `n` lines, each line is two integer values `f` and `p`. `f` is the score for the best chain ending at the corresponding anchor computed in the chaining algorithm, while `p` is the best predecessor anchor for this chain (-1 means the chain has only one anchor which is the current one).

A sample dumped output file:

```
21949
41	-1
50	0
...
```

## <a name="limit"></a> Limitations and Notes

* Our accelerated kernels do not support spliced long reads. For example, acceleration of `Minimap2 -ax splice ref.fa tgt.fa ` is not yet supported.
* Our accelerations are not yet integrated into the software. The code in this repository contains kernels for the chaining function itself, which can be driven by testbed generated chaining data.
* We use an assumption that the quality of output does not degrade when we choose the lookup depth of the dynamic programming algorithm to be 64, based on the claim in Li, H. (2018).
* We inherit most of [limitations of Minimap2][25].
* For integration, we recommend implementing the whole `mm_chain_dp` function in all accelerations solutions to reduce output communication. The reason we choose the score value as the output point is that we can better evaluate the correctness in fine grain. We also recommend integrating the seeding part.

## <a name="ack"></a> Acknowledgement

We directly used and modified the code from Heng Li’s Minimap2 to generate test data for chaining algorithm. We also implement all kernels of the chaining algorithm based on Heng Li’s paper.

This research is supported by CRISP, one of six centers in JUMP, a Semiconductor Research Corporation (SRC) program and the contributions from the member companies under the Center for Domain-Specific Computing (CDSC) Industrial Partnership Program.

> Li, H. (2018). Minimap2: pairwise alignment for nucleotide sequences. *Bioinformatics*, **34**:3094-3100. [doi:10.1093/bioinformatics/bty191][26]

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
[16]:	#limit
[17]:	#ack
[18]:	http://www.cs.jhu.edu/~langmea/resources/lecture_notes/assembly_olc.pdf
[19]:	https://github.com/lh3/Minimap2
[20]:	https://doi.org/10.1093/bioinformatics/bty191
[21]:	http://vast.cs.ucla.edu/sites/default/files/publications/Minimap2-acc-approved.pdf
[22]:	http://datasets.pacb.com.s3.amazonaws.com/2014/c_elegans/list.html
[23]:	https://github.com/lh3/Minimap2
[24]:	https://lh3.github.io/Minimap2/Minimap2.html
[25]:	https://github.com/lh3/Minimap2#limit
[26]:	https://doi.org/10.1093/bioinformatics/bty191

[image-1]:	https://img.shields.io/badge/Version-Experimental-green.svg
[image-2]:	https://img.shields.io/bower/l/bootstrap.svg
[image-3]:	https://user-images.githubusercontent.com/843780/58130406-c7fa6280-7bd0-11e9-8880-9617bcbd3171.png "OLC Flow" width=400px
[image-4]:	https://user-images.githubusercontent.com/843780/58130612-4b1bb880-7bd1-11e9-86f4-81fe4f1be4d1.png "Overlap Steps" width=400px
[image-5]:	https://user-images.githubusercontent.com/843780/58130132-2ffc7900-7bd0-11e9-92f5-d5fe437c5785.png "Profiling Result" width=400px