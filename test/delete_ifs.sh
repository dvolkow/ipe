#! /bin/bash
set -x

COUNT=2
UPPER="vlan3"

ip link delete ${UPPER}

for i in `seq 0 $COUNT`;
do 
        ip link delete "vlan"${i}
done
