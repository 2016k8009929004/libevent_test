#!/bin/bash

dd if=/dev/urandom bs=1KB count=10 | base64 > client-input.dat