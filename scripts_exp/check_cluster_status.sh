#!/bin/bash

for i in {1..14}; do echo "data node $i"; ssh dn$i 'ps aux | grep OECAgent | grep -v grep'; done

echo "coordinator"
ps aux | grep OECCoordinator | grep -v grep


