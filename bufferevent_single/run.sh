#!/bin/bash

#make clean && make all

server='server'
client='client'

server_ip='172.18.13.14'
server_port=12345

if test $1 = $server
then
#    rm record_core_*.txt
    ./server --thread_num=$2 --num_server_thread=$3 --num_backend_thread=$4
else
    ./client $2 $server_ip $server_port $3
fi