#!/bin/bash

make clean && make all

server='server'
client='client'

server_ip='172.18.13.15'
server_port=12345

if test $1 = $server
then
    if [ -f "record.txt" ];then
       rm record.txt
    fi
    if [ -f "accept_cb.txt" ];then
       rm accept_cb.txt
    fi
    ./libevent_test $2
else
    ./libevent_test $2 $server_ip $server_port $3
fi