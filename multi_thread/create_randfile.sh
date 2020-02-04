#!/bin/bash

dd if=/dev/urandom bs=1K count=4 | base64 > client-input.dat