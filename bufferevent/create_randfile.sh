#!/bin/bash

dd if=/dev/urandom bs=1G count=1 | base64 > client-input.dat