#! /bin/bash

set -x
PARENT=$1
CHILD=$2
VID=$3

ip link add link ${PARENT} name ${CHILD} type vlan id ${VID}
