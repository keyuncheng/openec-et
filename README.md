# Elastic Transformation

Elastic Transformation is a framework to transform an erasure code into another erasure code with smaller repair bandwidth, subject to a configurable sub-packetization. This repository contains the implementation of elastic transformation as described in our paper.

## Overview

We implement elastic transformation atop HDFS with OpenEC. OpenEC is a middleware system realizing erasure coding schemes in the form of direct acyclic graphs (named as ECDAGs). The coding operations are based on ISA-L.

We implement elastic transformation, including transformation arrays, base codes and transformed codes in the form of ECDAGs. We integrated our implementation to **OpenEC v1.0**, and deploy atop **HDFS on Hadoop 3.0.0**.

Useful Links:
- OpenEC: [project website](http://adslab.cse.cuhk.edu.hk/software/openec/), [paper](https://www.usenix.org/conference/fast19/presentation/li) (USENIX FAST'19)
- Hadoop 3.0.0: [link](https://hadoop.apache.org/docs/r3.0.0/)
- ISA-L: [link](https://github.com/intel/isa-l)


## Implementation

The source codes of elastic transformation are under ```src/ec```:
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

## Deployment

### Deploy OpenEC atop HDFS 3

We deploy OpenEC atop HDFS on Hadoop 3.0.0. To run an *(n,k)* code, at least *n + 1* machines are required.
Please follow the instructions in the OpenEC document (```doc/doc.pdf```, OpenEC with HDFS 3) for the deployment.

### System Configurations

We use [Wonder Shaper](https://github.com/magnific0/wondershaper) to configure the network bandwidth **on each machine**.

We use the default system configurations as below:
- Network bandwidth: 1Gb/s
- HDFS block size: 64MiB
- OpenEC packet size: 1MiB
- OpenEC controller threads: 4
- OpenEC agent threads: 20

On **each machine**:

In hdfs-site.xml:
```
dfs.block.size: 67108864
oec.pktsize: 1048576
```

In sysSetting.xml:
```
packet.size: 1048576
oec.controller.thread.num: 4 // number of controller threads
oec.agent.thread.num: 20 // number of agent threads
```

The other configurations follow the defaults in ```doc/doc.pdf```.

## Run Elastic Transformed Codes

Below is an example of single block failure repair for an **elastic transformed *(9,6)* RS code with sub-packetization = 3**. We also provide sample coding schemes for other codes with *(n,k) = (9,6)* in ```conf/sysSetting.xml```.

### Write Blocks

On one storage node:

- Enter the OpenEC project directory (e.g. ```~/openec-et```).

- Use the OpenEC Client to issue an (online) file write (filename: ```input_384```, size: 384 MiB = *k* HDFS blocks). The file is encoded into *n* blocks and randomly distributed to *n* storage nodes.

```
cd ~/openec-et
./OECClient write input_384 /test_code ETRSConv_9_6_3 online 384
```

We can check the information of the blocks with:

```
hdfs fsck / -files -blocks -locations
```

For each block *i* (out of *n*), the filename format in HDFS is ```/test_code_oecobj_<i>```; the physical HDFS block name format is ```blk<blkid>```.

### Manually Fail a Block

Now we manually remove block *i*. **On the DataNode that stores block *i***, we enter the folder that physically stores the block in HDFS:

```
cd ${HADOOP_HOME}/dfs/data/current/BP-*/current/finalized/subdir0/subdir0/
```

Only one block ```blk<blkid>``` is stored with it's metadata ```blk<blkid>.meta```. We copy the block to OpenEC project directory, and then manually remove the block.

```
cp blk<blkid> ~/openec-et
rm blk<blkid>
```

### Repair a Failed Block

HDFS will detect and report the lost block shortly after the block manual removal. We can verify with ```hdfs fsck``` again.

```
hdfs fsck / -list-corruptfileblocks
```

After the failure is detected, we repair the **failed block *i*** (named ```/test_code_<i>```, **without "\_oecobj\_"**) with the OpenEC Client's read request.

```
cd ~/openec-et
./OECClient read /test_code_<i> test_code_<i>
```

We can verify the repaired block is the same as the manually failed block.

```
cmp test_code_<i> blk<blkid>
```