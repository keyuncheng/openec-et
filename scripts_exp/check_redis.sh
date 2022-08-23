#!/bin/bash

for i in {1..14}; do echo "data node $i"; redis-cli -h dn$i KEYS \*; done

echo "coordinator"
redis-cli KEYS \*


