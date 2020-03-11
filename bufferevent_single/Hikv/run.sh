#!/bin/bash

num_kv=200000000
OUTPUT=hikv_result

for ((i=1;i<=16;i++))
do
num_opt=`expr $num_kv / $i`
./hikv --num_warm=$num_opt --num_get=$num_opt --num_server_thread=$i --num_backend_thread=$i > $OUTPUT/$i.txt
done