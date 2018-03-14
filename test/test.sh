#! /bin/bash
set -e

VRF_NAME="vrf_test"
IF_NAME="vlan1"
PARENT="enp3s0"
VID="42"

function get_ifindex() {
        LIST=`ip a | grep ${PARENT} | grep -o -E '[0-9]+'`
        echo $LIST | cut -d" " -f2
}

function make_all() {
        cd ../kernel
        make disclean
        make 
        make clean 

        cd ../user 
        make 

        cd ../
}

function create_vrf() {

        ip netns add ${VRF_NAME}

        ip link add link ${PARENT} name ${IF_NAME} type vlan id ${VID}

        ip link set ${IF_NAME} netns ${VRF_NAME}
}

make_all
create_vrf

IFINDEX=get_ifindex
NEW_VID="142"

./ipe dev ${IFINDEX} netns ${VRF_NAME} id ${NEW_VID}
