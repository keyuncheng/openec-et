#!/bin/bash

# write 20 hdfs stripes
bash test_hdfs_online.sh

echo "start repair"

repair_node="dn1"

relative_blk_path=`ssh ${repair_node} "cd $HADOOP_HOME/dfs/; find -name finalized"`
ssh ${repair_node} "cd $HADOOP_HOME/dfs/; cd ${relative_blk_path}; mv * bak"

echo "removed blocks"

cd $HADOOP_HOME/logs

while true
do
    repairTime=`grep -r repairTime ./`
    len=${#repairTime}
    echo ${len}
    if [ ${len} -gt 0 ]
    then
        echo $repairTime
        break
    else
        echo "wait for results, current result: ${repairTime}"
        sleep 5
    fi
done