#!/bin/bash

#make clean && make all

server='server'
client='client'

server_ip='172.18.13.14'
server_port=12345

num_put=1048576
num_get=1048576

if test $1 = $server
then
#    rm record_core_*.txt
    ./server --num_thread=$2 --num_put=$num_put --num_get=$num_get --num_server_thread=$3 --num_backend_thread=$4
else
    ./client $2 $server_ip $server_port
fi