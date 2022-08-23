#!/bin/bash

home_dir=$(echo ~)
proj_dir=${home_dir}/openec-et
script_dir=${proj_dir}/script
scripts_exp_dir=${proj_dir}/scripts_exp

# stop OEC
cd ${script_dir}
python stop.py

# coordinator
cd ${proj_dir}
rm -f coor_output
rm -f entryStore
rm -f poolStore

# agent
for i in {1..14}; do ssh dn$i "cd ${proj_dir}; rm -f agent_output"; done

# restart HDFS
cd ${scripts_exp_dir}
bash restart_hdfs.sh

sleep 2

cd ${script_dir}
bash env.sh && python start.py
