import math
import os
import sys

readFile = ["read_cb_1.txt", "read_cb_2.txt", "read_cb_3.txt", "read_cb_4.txt", "read_cb_5.txt"]

first = 0

for file in readFile:
    with open(file, "r") as f:
        total = 0
        count = 0
        for line in f:
           res = line.split()
           if len(res) == 2 and res[1].isdigit():
               total += int(res[1])
               count += 1
        f.close()
        if first == 0:
            avg = round(float(total)/float(count), 4)
            first = 1
        else:
            avg = (avg + round(float(total)/float(count), 4)) / 2

print('read_cb average: %.4f' % avg)

acceptFile = ["accept_cb_1.txt", "accept_cb_2.txt", "accept_cb_3.txt", "accept_cb_4.txt", "accept_cb_5.txt"]

first = 0

for file in acceptFile:
    with open(file, "r") as f:
        tot_accept = 0
        tot_pthread_create = 0
        count = 0
        for line in f:
           res = line.split()
           if len(res) == 4 and res[1].isdigit() and res[3].isdigit():
               tot_accept += int(res[1])
               tot_pthread_create += int(res[3])
               count += 1
        f.close()
        if first == 0:
            avg_accept = round(float(tot_accept)/float(count), 4)
            avg_pthread_create = round(float(tot_pthread_create)/float(count), 4)
            first = 1
        else:
            avg_accept = (avg_accept + round(float(tot_accept)/float(count), 4)) / 2
            avg_pthread_create = (avg_pthread_create + round(float(tot_pthread_create)/float(count), 4)) / 2

print('accept average: %.4f, pthread_create average: %.4f' % (avg_accept, avg_pthread_create))
