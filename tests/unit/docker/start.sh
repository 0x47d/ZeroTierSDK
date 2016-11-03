#!/bin/bash

# Runs test image and monitor image as daemons
test_name="docker_demo"
echo 'Starting containers for: ' "$test_name"
touch "$test_name".name
test_container=$(docker run -d -it -v $PWD/_results:/opt/results --privileged --device=/dev/net/tun "$test_name":latest)
monitor_container=$(docker run -d -it -v $PWD/_results:/opt/results --privileged --device=/dev/net/tun "$test_name"_monitor:latest)

sleep 90
./check.sh _results/OK.docker_demo.txt
