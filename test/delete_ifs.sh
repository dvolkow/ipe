#! /bin/bash
set -x

PARENT="enp3s0"
CHILD="vlan1"

ip link 'set' ${CHILD} 'down'
ip link delete ${CHILD}
