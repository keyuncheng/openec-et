#!/bin/bash

home_dir=$(echo ~)
proj_dir=${home_dir}/openec-et
scripts_exp_dir=${proj_dir}/scripts_exp
config_dir=${proj_dir}/conf
config_filename=sysSetting.xml
hadoop_home_dir=$(echo $HADOOP_HOME)
hdfs_config_dir=${proj_dir}/hdfs3.3.4-integration/conf

# update OEC configs

# datanodes
for i in {1..14}; do scp ${config_dir}/${config_filename} dn$i:${config_dir}; done

# update HDFS configs to Hadoop

# namenode
cp ${hdfs_config_dir}/* ${hadoop_home_dir}/etc/hadoop
bash update_ip.sh

# datanodes
for i in {1..14}; do scp ${hdfs_config_dir}/* dn$i:${hadoop_home_dir}/etc/hadoop; done

for i in {1..14}; do ssh dn$i "cd ${scripts_exp_dir}; bash update_ip.sh"; done