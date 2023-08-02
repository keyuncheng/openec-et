# Elastic Transformation

Elastic Transformation is a framework to transform an erasure code into
another erasure code with smaller repair bandwidth, subject to a configurable
sub-packetization. This repository contains the implementation of elastic
transformation as described in our paper.

## Overview

We implement elastic transformation atop HDFS with OpenEC. OpenEC is a
middleware system realizing erasure coding schemes in the form of direct
acyclic graphs (named as ECDAGs). The coding operations are based on ISA-L.

We implement elastic transformation, including transformation arrays, base
codes and transformed codes in the form of ECDAGs. We integrated our
implementation to **OpenEC v1.0.0**, and deploy atop **HDFS on Hadoop 3.0.0**.

This repository contains **the patch** of source code for OpenEC-v1.0.0. To
deploy elastic transformation, please follow the instructions below to install
the patch to OpenEC.

Useful Links:
- OpenEC: [project website](http://adslab.cse.cuhk.edu.hk/software/openec/),
  [paper](https://www.usenix.org/conference/fast19/presentation/li) (USENIX
  FAST'19)
- Hadoop 3.0.0: [link](https://hadoop.apache.org/docs/r3.0.0/)
- ISA-L: [link](https://github.com/intel/isa-l)


## Paper

Kaicheng Tang, Keyun Cheng, Helen H. W. Chan, Xiaolu Li, Patrick P. C. Lee,
Yuchong Hu, Jie Li, and Ting-Yi Wu.

"Balancing Repair Bandwidth and Sub-Packetization in Erasure-Coded Storage via Elastic Transformation."

Proceedings of IEEE International Conference on Computer Communications
(INFOCOM 2023), New York, USA, May 2023.

(AR: 252/1312 = 19.2%)

\[[pdf](https://keyuncheng.github.io/files/publications/infocom23et.pdf)\] \[[software](http://adslab.cse.cuhk.edu.hk/software/openec-et)\]



## Folder Structure

The patch for OpenEC is in ```openec-et-patch/```. The implementation of
erasure codes are in ```openec-et/openec-et-patch/src/ec/```.

```
- ETUnit.cc/hh (Transformation Array)

// base codes
- RSCONV.cc/hh (RS codes)
- WASLRC.cc/hh (Azure's LRCs)
- HHXORPlus.cc/hh (Hitchhiker; we implement Hitchhiker-XOR+ with optimized repair BW)
- HTEC.cc/hh (HashTag EC)

// elastic transformed codes
- ETRSConv.cc/hh (transformed RS codes)
- ETAzureLRC.cc/hh (transformed Azure's LRC)
- ETHHXORPlus.cc/hh (transformed Hitchhiker)
- ETHTEC.cc/hh (transformed HashTag EC)
```

The sample configuration file of elastic transformation is under
```openec-et-patch/conf/sysSetting.xml```. For more details of the
configuration file, please refer to OpenEC documentation.

In the sample configuration file, we include configurations of various base
codes and elastic transformed codes (for k = 10) with different
sub-packetization as below:


| Code | ClassName | Parameters *(n,k)* | Sub-packetization |
| ------ | ------ | ------ | ------ |
| RS code | RSCONV | (14,10) | 1 |
| RS-ET | ETRSConv | (14,10) | 2, 3, 4, 8, 16, 32, 64, 128, 256 |
| Hitchhiker-XOR+ (HH) | HHXORPlus | (14,10) | 2 |
| HH-ET | ETHHXORPlus | (14,10) | 4, 6, 8 |
| HashTag (HTEC) | HTEC | (14,10) | 2 |
| HTEC-ET | ETHTEC | (14,10) | 4, 6, 8 |
| Azure-LRC (LRC) | WASLRC | (n,k,l) =（16,10,2) | 1 |
| LRC-ET | ETAzureLRC | (n,k,l) =（16,10,2) | 2, 3, 4 |


## Deployment

To illustrate the deployment, we provide an example of *(14, 10)* **elastic
transformed RS code (RS-ET)** with sub-packetization = 4.

To deploy elastic transformation, please follow the steps below:

* [Cluster setup](#cluster-setup)

* [Download HDFS-3.0.0 and OpenEC-v1.0.0](#download-hdfs-300-openec-v100)

* [Add patch to OpenEC-v1.0.0](#add-patch-to-openec-v100)

* [Deploy HDFS-3.0.0 with patched
  OpenEC](#deploy-hdfs-300-with-patched-openec)

* [Start HDFS-3.0.0 with OpenEC](#start-hdfs-300-with-openec)

* [Run elastic transformed code](#start-hdfs-300-with-openec)


### Cluster Setup

Cluster configuration

We setup a (local) storage cluster of 15 nodes (1 for HDFS NameNode, 14 for
DataNode). 

| HDFS | OpenEC | Number | IP |
| ------ | ------ | ------ | ------ |
| NameNode | Controller | 1 | 192.168.10.21 |
| DataNode | Agent and Client | 14 | 192.168.10.22,192.168.10.23,192.168.10.24,192.168.10.25,192.168.10.26,192.168.10.27,192.168.10.28,192.168.10.29,192.168.10.30,192.168.10.31,192.168.10.32,192.168.10.33,192.168.10.47,192.168.10.48 | 

On each machine, we create an account with default username as ```et```.
Please change the IP addresses in the configuration files of HDFS and OpenEC
for your cluster accordingly.


### Download HDFS-3.0.0, OpenEC-v1.0.0

On NameNode:

* Download the source code of HDFS-3.0.0 in ```/home/et/hadoop-3.0.0-src```

* Download the source code of OpenEC-v1.0.0 in ```/home/et/openec-v1.0.0```

* Download the patch in ```/home/et/openec-et```


### Add patch to OpenEC-v1.0.0

Copy the source code of OpenEC-v1.0.0 into ```openec-et/``` first, then copy
the patch into ```openec-et/```

```
$ cp -r /home/et/openec-v1.0.0/* /home/et/openec-et
$ cp -r /home/et/openec-et/openec-et-patch/* /home/et/openec-et
```


### Deploy HDFS-3.0.0 with patched OpenEC

Please follow the OpenEC documentation for deploying HDFS-3.0.0 and OpenEC in
the cluster.

Notes:

* the sample configuration files for HDFS-3.0.0 are in
```openec-et-patch/hdfs3-integration/conf```.

We set the following system configurations:
- HDFS block size: 64MiB
- OpenEC packet size: 1MiB
- OpenEC controller threads: 4
- OpenEC agent threads: 20

In ```hdfs-site.xml``` of HDFS:

| Parameter | Description | Example |
| ------ | ------ | ------|
| dfs.block.size | The size of a block in bytes. | 67108864 for block size with 64 MiB. |
| oec.pktsize | The size of a packet in bytes. Note that for a code with sub-packetization level w, the size of a packet is w times the size of a sub-packet. | 1048576 for 1MiB. The sub-packet size for a w = 4 RS-ET code is 256KiB. |


In ```conf/sysSetting.xml``` of OpenEC:

| Parameter | Description | Example |
| ------ | ------ | ------ |
| packet.size | The size of a packet in bytes. | 1048576 for 1MiB. |
| oec.controller.thread.num | Number of controller threads. | 4 |
| oec.agent.thread.num | number of agent threads | 20 |

The other configurations follow the default in OpenEC documentation.


### Start HDFS-3.0.0 with OpenEC

* Start HDFS-3.0.0

```
$ hdfs namenode -format
$ start-dfs.sh
```

* Start OpenEC

```
$ cd openec-et
$ python script/start.py
```

For more details, please follow the documentation of OpenEC.


### Run Elastic Transformed Codes

Below is an example of single block failure repair for an **elastic
transformed *(14,10)* RS code with sub-packetization = 4**.

#### Write Blocks

On one of the HDFS DataNode:

- Use OpenEC Client to issue an (online) file write. Assume the filename is
  ```input_640``` and the file size is: 640 MiB = *k = 10* HDFS blocks. The
  file is encoded into *n* HDFS blocks and randomly distributed to *n* storage
  nodes.

```
cd ~/openec-et
./OECClient write input_640 /test_code ETRSConv_14_10_4 online 640
```

We can check the distribution of blocks with:

```
hdfs fsck / -files -blocks -locations
```

For each block *i* (out of *n*), the filename format in HDFS is
```/test_code_oecobj_<i>```; the physical HDFS block name format is
```blk_<blkid>```.

e.g., the first block (or block 0) is stored in HDFS as
```/test_code_oecobj_0```. We can also get the IP address of DataNode that
stores block 0.

A sample log message copied from ```hdfs fsck``` are shown as below. The IP
address of the DataNode that stores block 0 is ```192.168.10.22```; the
physical block name stored in HDFS is named ```blk_1073741836```.

```
/test_code_oecobj_0 67108864 bytes, replicated: replication=1, 1 block(s):  OK
0. BP-1220575476-192.168.10.21-1672996056729:blk_1073741836_1011 len=67108864 Live_repl=1  [DatanodeInfoWithStorage[192.168.10.22:9866,DS-89dc6e26-7219-
412d-a7fa-e33dbbb14cfe,DISK]]
```


#### Manually Fail a Block

Now we manually remove a block from one of the storage node. For example, we
manually fail block 0 (named ```/test_code_oecobj_0``` in HDFS). **On the
DataNode that stores block 0**, we first enter the folder that physically
stores the block in HDFS:

```
cd ${HADOOP_HOME}/dfs/data/current/BP-*/current/finalized/subdir0/subdir0/
```

Only one block ```blk_<blkid>``` is stored with it's metadata file
```blk_<blkid>.meta```. We copy the block to OpenEC project directory, and
then manually remove the block.

```
cp blk_<blkid> ~/openec-et
rm blk_<blkid>
```

After the operation, block 0 is considered lost, as it no longer exists in the
HDFS directory.

HDFS will automatically detect and report the lost block shortly after the
block manual removal. We can verify with ```hdfs fsck``` again.

```
hdfs fsck / -list-corruptfileblocks
```


#### Repair a Failed Block

After the failure is detected, **on the same DataNode**, we repair the lost
block 0 (named ```/test_code_0```, **without "\_oecobj\_"**) with the OpenEC
Client's read request, and store it as ```test_code_0```.

```
cd ~/openec-et
./OECClient read /test_code_0 test_code_0
```

We can verify that the repaired block is the same as the manually failed block
stored in ```~/openec-et/```.

```
cd ~/openec-et
cmp test_code_0 blk_<blkid>
```
