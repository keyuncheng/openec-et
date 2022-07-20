#!/bin/bash

hdfs_data_dir="/home/openec/hadoop-3.0.0-src/hadoop-dist/target/hadoop-3.0.0/dfs/data"
num_nodes=16

# stop all OpenEC agents
for i in $(seq 1 ${num_nodes}); do ssh openec-dn$i "killall OECAgent; sleep 3; redis-cli flushdb; ps aux | grep OECAgent"; done

# stop the OpenEC coordinator
killall OECCoordinator
redis-cli flushdb

# stop hdfs
stop-dfs.sh

# clean hdfs data
for i in $(seq 1 ${num_nodes}); do ssh openec-dn$i "rm -r ${hdfs_data_dir}/current/*"; done

# clean hdfs metadata
#ssh cloud-node13 hdfs namenode -format -force
hdfs namenode -format -force

rm -f entryStore
rm -f poolStore
