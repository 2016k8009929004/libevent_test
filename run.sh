#!/bin/bash

make clean && make all

server=’server‘
client=‘client’

server_ip=‘172.18.13.15’
server_port=12345

if test $1 = $server
then
    ./libevent_test $2
else
    i=0
    while($i < $2)
    do
        mkdir $i
        cd $i
        ./create_randfile.sh
        cd ..
        let "i++"
    done
    ./libevent_test $2 $server_ip $server_port