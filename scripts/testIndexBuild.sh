#!/bin/bash

# This script is used to run the benchmark tests in batch mode.

# Check if a command-line argument is provided
if [ $# -eq 0 ]; then
    # Set default value
    bpfile="/nvme/gc/diag2/openpmd.bp"
    threads="128,64,32,16,8,4,2,1"
    index_name="diag2"
    block_batch_size="10000,1000,100,10,1"
    iteration="500"
    species="electrons"
    index_type="minmax,rtree"
    result_dir="scripts/log/build"
    storageBackend="file"
    secondaryIndexType="none"
else
    # Use the value provided as a command-line argument
    bpfile=$1
    threads=$2
    index_name=$3
    block_batch_size=$4
    iteration=$5
    species=$6
    index_type=$7
    result_dir=$8
    storageBackend=$9
    secondaryIndexType=${10}
fi

if [ ! -d "$result_dir" ]; then
    mkdir -p "$result_dir"
fi

# Convert the comma-separated string to an array
IFS=',' read -ra block_batch_array <<< "$block_batch_size"
IFS=',' read -ra threads_array <<< "$threads"
IFS=',' read -ra index_type_array <<< "$index_type"

# Function to display memory usage and log to file
function monitor_memory_usage_and_log() {
    local log_file=$1
    current_date_time=$(date +'%Y-%m-%d %H:%M:%S')
    echo "Current Date and time: $current_date_time" >> "$log_file"
    echo "Memory usage:" >> "$log_file"
    free -hm >> "$log_file"
}

# Function to monitor process status and memory usage
function monitor_process() {
    local pid=$1
    local log_file=$2

    # Loop until the process completes
    while kill -0 $pid 2>/dev/null; do
        # Display memory usage periodically and log to file
        monitor_memory_usage_and_log "$log_file"
        sleep 2  # Adjust sleep interval as needed
    done
}

# Loop select_n times
for j in "${index_type_array[@]}"; do
    for k in "${threads_array[@]}"; do
        for l in "${block_batch_array[@]}"; do
            # Loop select_n times
            for ((i=0; i<5; i++)); do
                echo 3 | sudo tee /proc/sys/vm/drop_caches

                echo "nohup ./build-with-blosc/test_build \\
                    -f $bpfile \\
                    -i $index_name \\
                    -m $k \\
                    -b $l \\
                    -t $j \\
                    --iteration $iteration \\
                    --species $species \\
                    --storageBackend $storageBackend \\
                    --secondaryIndexType $secondaryIndexType \\
                    > ${result_dir}/index_build_${j}_threads_${k}_blocks_${l}_${i}.log 2>&1 &"  > scripts/log/buildTest_sh.log

                # Run the command with the given parameters
                nohup ./build-with-blosc/test_build \
                    -f $bpfile \
                    -i $index_name \
                    -m $k \
                    -b $l \
                    -t $j \
                    --iteration $iteration \
                    --species $species \
                    --storageBackend $storageBackend \
                    --secondaryIndexType $secondaryIndexType \
                    > ${result_dir}/index_build_${j}_threads_${k}_blocks_${l}_${i}.log 2>&1 &
                
                # # Get the PID of the last background process
                pid=$!

                # # Monitor the process (e.g., memory usage) in the background
                monitor_process $pid "${result_dir}/index_build_${j}_threads_${k}_blocks_${l}_${i}.memlog"

            done
        done
    done
done