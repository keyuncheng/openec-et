#!/bin/bash

stop-dfs.sh

# namenode
rm -rf $HADOOP_HOME/logs/*

# datanodes
for i in {1..14}; do ssh dn$i "rm -rf $HADOOP_HOME/logs/*"
for i in {1..14}; do ssh dn$i "rm -rf $HADOOP_HOME/dfs/*"

hdfs namenode -format -force && start-dfs.sh
