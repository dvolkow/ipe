#! /bin/bash
set -x

IP_PARENT='10.10.13.55/24'
IP_CHILD1='2.2.2.2/24'
IP_CHILD2='2.2.2.3/24'
PARENT="enp3s0"
CHILD1="vlan1"
CHILD2="vlan2"
VID1=100
VID2=101

ip addr add ${IP_PARENT} dev ${PARENT}

ip link add link ${PARENT} name ${CHILD1} 'type' vlan id ${VID1}
ip link add link ${PARENT} name ${CHILD2} 'type' vlan id ${VID2}
ip addr add ${IP_CHILD1} dev ${CHILD1}
ip addr add ${IP_CHILD2} dev ${CHILD2}
ip link 'set' ${CHILD1} 'up'
ip link 'set' ${CHILD2} 'up'
