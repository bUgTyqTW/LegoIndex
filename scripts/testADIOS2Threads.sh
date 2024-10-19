#!/bin/bash

threads="1,2,4,8,16,32,64,128"

# Convert the comma-separated string to an array
IFS=',' read -ra test_thread_array <<< "$threads"

for i in "${test_thread_array[@]}"; do
    echo "Running with $i threads"

    echo 3 | sudo tee /proc/sys/vm/drop_caches

    ./build-with-blosc/test_adios_thread \
        -f "/nvme/gc/openpmd_010000.bp/" \
        -k "/data/10000/particles/electrons/position/" \
        -n $i
done