#!/bin/bash

dd if=/dev/urandom bs=1G count=8 | base64 > client-input.dat