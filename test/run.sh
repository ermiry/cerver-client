#!/bin/bash

LD_LIBRARY_PATH=bin ./test/bin/collections --quiet || { exit 1; }

LD_LIBRARY_PATH=bin ./test/bin/utils || { exit 1; }
