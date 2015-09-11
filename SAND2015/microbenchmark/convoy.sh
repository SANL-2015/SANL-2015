#!/bin/bash

TYPE=$1
CLIENTS=$2

if [ -z "$TYPE" ]; then
		TYPE=spinlock
fi

if [ -z "$CLIENTS" ]; then
		CLIENTS=100
fi

echo "running '$TYPE' with '$CLIENTS' clients"
./benchmark -1  -n 5000 -d 1000000 -A 1 -s 0 -m -u -g 10 -l 10 -F $TYPE -c $CLIENTS


