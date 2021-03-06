#! /bin/bash
set -x

COUNT=2
DEF_PREFIX="vlan"

IP_PARENT='10.10.13.58/24'
PARENT="enp3s0"

VIDS=('100' '101' '42')
PARENTS=($PARENT $PARENT $DEF_PREFIX"1")
IPS=('2.2.2.1/24' '2.2.3.1/24' '2.2.4.1/24')

function set_IP {
        sudo ip addr add ${IP_PARENT} dev ${PARENT}
}

function add_links {
        for i in `seq 0 $COUNT`;
        do
                sudo ip link add link ${PARENTS[$i]} name ${DEF_PREFIX}${i} 'type' vlan id ${VIDS[$i]}
                sudo ip addr add ${IPS[$i]} dev ${DEF_PREFIX}${i}
                sudo ip link 'set' ${DEF_PREFIX}${i} 'up'

        done
}

sudo insmod ../kernel/ipe.ko 

set_IP
add_links

ip a
