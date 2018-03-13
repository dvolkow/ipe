#! /bin/bash

function make_all() {
        cd ../kernel
        make clean
        make 

        cd ../user 
        make 
}

make_all
