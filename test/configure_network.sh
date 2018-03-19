#! /bin/bash
set -x

COUNT=2
DEF_PREFIX="vlan"

IP_PARENT='10.10.13.55/24'
PARENT="enp3s0"

VIDS=('100' '101' '42')
PARENTS=($PARENT $PARENT $DEF_PREFIX"1")
IPS=('2.2.2.2/24' '2.2.2.3/24' '2.2.2.4/24')

function set_IP {
        ip addr add ${IP_PARENT} dev ${PARENT}
}

function add_links {
        for i in `seq 0 $COUNT`;
        do
                ip link add link ${PARENTS[$i]} name ${DEF_PREFIX}${i} 'type' vlan id ${VIDS[$i]}
                ip addr add ${IPS[$i]} dev ${DEF_PREFIX}${i}
                ip link 'set' ${DEF_PREFIX}${i} 'up'

        done
}


add_links
