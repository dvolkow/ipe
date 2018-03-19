#! /bin/bash
set -x

COUNT=2
UPPER="vlan3"

ip link delete ${UPPER}

for i in `seq 1 $COUNT`;
do 
        ip link delete "vlan"${i}
done
