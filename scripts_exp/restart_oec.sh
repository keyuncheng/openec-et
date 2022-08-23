#!/bin/bash

home_dir=$(echo ~)
proj_dir=${home_dir}/openec-et

# stop OEC
python stop.py

# coordinator
rm -f ../coor_output
rm -f ../entryStore
rm -f ../poolStore

# agent
for i in {1..14}; do ssh dn$i "cd ${proj_dir}; rm -f agent_output"

# restart HDFS
bash restart_hdfs.sh

sleep 2

python start.py