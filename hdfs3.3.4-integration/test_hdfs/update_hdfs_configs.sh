#!/bin/bash

home_dir=$(echo ~)
proj_dir=${home_dir}/openec-et
scripts_exp_dir=${proj_dir}/scripts_exp
hadoop_home_dir=$(echo $HADOOP_HOME)
hdfs_config_dir=${proj_dir}/hdfs3.3.4-integration/test_hdfs/conf

# update HDFS configs to Hadoop

cp rack_aware.sh ${hdfs_config_dir}

# namenode
cp ${hdfs_config_dir}/* ${hadoop_home_dir}/etc/hadoop
cd ${scripts_exp_dir}; bash update_ip.sh; cd -
chmod 777 ${hadoop_home_dir}/etc/hadoop/rack_aware.sh

# datanodes
for i in {1..14}; do scp ${hdfs_config_dir}/* dn$i:${hadoop_home_dir}/etc/hadoop; done

for i in {1..14}; do ssh dn$i "cd ${scripts_exp_dir}; bash update_ip.sh"; done
for i in {1..14}; do ssh dn$i "chmod 777 ${hadoop_home_dir}/etc/hadoop/rack_aware.sh"; done