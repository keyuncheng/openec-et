#!/bin/bash

stop-dfs.sh

# namenode
rm -rf $HADOOP_HOME/logs/*

# datanodes
for i in {1..14}; do ssh dn$i "rm -rf $HADOOP_HOME/logs/*"; done
for i in {1..14}; do ssh dn$i "rm -rf $HADOOP_HOME/dfs/*"; done

# kill java
for i in {1..14}; do ssh dn$i "sudo killall java"; done