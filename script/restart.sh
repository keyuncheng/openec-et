#!/bin/bash
oec_packetsize=$1

bash ./stop.sh
bash ./start.sh ${oec_packetsize}
