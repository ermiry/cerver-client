#!/bin/bash

./test/bin/collections --quiet || { exit 1; }

./test/bin/json || { exit 1; }

./test/bin/packets || { exit 1; }

./test/bin/threads || { exit 1; }

./test/bin/utils || { exit 1; }
