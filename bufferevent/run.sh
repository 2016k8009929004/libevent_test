#!/bin/bash

make clean && make all

server='server'
client='client'

server_ip='172.18.13.14'
server_port=12345

if test $1 = $server
then
    rm record_core_*.txt
    ./libevent_test $2
else
    ./libevent_test $2 $server_ip $server_port $3
fi