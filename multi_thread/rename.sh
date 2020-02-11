#!/bin/bash

python generate.py && \
mv perform.txt perf_$1.txt && \
mv accept_cb.txt pthread_$1.txt && \
mv request_process_cb.txt callback_$1.txt
