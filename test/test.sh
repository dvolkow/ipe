#! /bin/bash
set -e
set -x

MODNAME="ipe.ko"
VRF_NAME="vrf_test"
IF_NAME="vlan1"
PARENT="enp3s0"
VID="42"

function get_ifindex() {
        LIST=`ip a | grep ${PARENT} | grep -o -E '[0-9]+'`
        echo `echo $LIST | cut -d" " -f1`
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

function delete_vrf() {
        ip netns del ${VRF_NAME}
}

function insmod_ipe() {
        insmod ../kernel/${MODNAME}
}

function rmmod_ipe() {
        rmmod ${MODNAME}
}

if [[ $1 == "make" ]]; then
        make_all
        exit
fi

if [[ $1 == "create_vrf" ]]; then
        create_vrf
        exit
fi

if [[ $1 == "ins" ]]; then
        insmod_ipe
        exit
fi

if [[ $1 == "run" ]]; then
        IFINDEX=`get_ifindex`
        NEW_VID="142"
        echo $IFINDEX
#        ../ipe dev ${IFINDEX} netns ${VRF_NAME} id ${NEW_VID}
        ../ipe dev ${IFINDEX} id ${NEW_VID}
        exit
fi

if [[ $1 == "rmmod" ]]; then
        rmmod_ipe
        exit
fi

if [[ $1 == "delete_vrf" ]]; then
        delete_vrf
        exit
fi
