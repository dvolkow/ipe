#! /bin/bash
set -x


COUNT=2
UPPER="vlan2"

sudo ip link delete ${UPPER}

for i in `seq 0 $COUNT`;
do 
        sudo ip link delete "vlan"${i}
done
