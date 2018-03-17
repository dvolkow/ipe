#! /bin/bash
set -x

IP_PARENT='10.10.13.55/24'
IP_CHILD='2.2.2.2/24'
PARENT="enp3s0"
CHILD="vlan1"
VID=100

ip addr add ${IP_PARENT} dev ${PARENT}

ip link add link ${PARENT} name ${CHILD} 'type' vlan id ${VID}
ip addr add ${IP_CHILD} dev ${CHILD}
ip link 'set' ${CHILD} 'up'
