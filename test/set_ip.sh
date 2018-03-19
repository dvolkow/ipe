#! /bin/bash
set -x

IFNAME=$1
IP=$2

ip addr add ${IP} dev ${IFNAME}

