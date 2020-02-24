import math
import os
import sys

fileName = ["read_cb_1.txt", "read_cb_2.txt", "read_cb_3.txt", "read_cb_4.txt", "read_cb_5.txt"]

first = 0

for file in fileName:
    with open(file, "r") as f:
        total = 0
        count = 0
        for line in f:
           res = line.split()
           if len(res) == 2 and res[1].isdigit():
               time = res[1]
               total = total + int(time)
               count += 1
        f.close()
        if first == 0:
            avg = round(float(total)/float(count), 4)
            first = 1
        else:
            avg = (avg + round(float(total)/float(count), 4)) / 2

print("read_cb average:",avg)
