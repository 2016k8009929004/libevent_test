#!/bin/bash

dd if=/dev/urandom bs=1M count=50 | base64 > client-input.dat