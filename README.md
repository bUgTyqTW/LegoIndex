# LEGOINDEX: A Scalable and Flexible Indexing Framework for Efficient Analysis of Extreme-Scale Particle Data

## Dependencies
- Linux (Ubuntu 22.04)
 
- RocksDB dependencies: Follow the [installation guide](https://github.com/facebook/rocksdb/blob/main/INSTALL.md)
 
- Python 3: `sudo apt-get install python3-dev`

## Deployment
1. Initialize submodules:


```bash
git submodule update --init --recursive
```
 
2. Compile RocksDB:


```bash
cd third_party/rocksdb/ && mkdir build && cd build && cmake .. -DWITH_GFLAGS=0 && make -j64
```
**Note:** Due to a version conflict with googletest, RocksDB must be compiled separately and linked into LegoIndex through the CMakeLists.txt (For now, automatically link to third_party/rocksdb/build/). If you encounter compilation errors, please disable the "warnings as errors" option in RocksDB's CMakeLists.txt.
 
3. Install Python dependencies:


```bash
sudo apt install python3-pip && pip3 install numpy mpi4py
```
 
4. If the particle dataset is compressed with Blosc, set up the Blosc environment:


```bash
export sw_dir=your_directory && source scripts/setup_blosc-1.21.1.sh
```
 
5. Build and compile **LegoIndex** :

```bash
cd ${proj} && mkdir build && cd build && cmake .. -DBUILD_TESTING=OFF && make -j64
```
Or compile **LegoIndex**  with Blosc:

```bash
mkdir build-with-blosc && cd build-with-blosc && cmake .. \
    -DBUILD_TESTING=OFF -DADIOS2_USE_Blosc2=ON -DADIOS2_USE_Python=ON \
    -DBLOSC_INCLUDE_DIR=${BLOSC_INCLUDE_DIR} -DBLOSC_LIBRARY=${BLOSC_LIBRARY} \
    && make -j64
```
## Index Construction
Use the following command gives an example to construct **LegoIndex**:


```bash
./test_build -f "/data/gc/middle_16/particle_9000.bp/" -i bp09000_1k -s "hydrogen" -t minmax --iteration 9000 -m 16 -b 100 -x minmax -l 1000 -d rocksdb
```

### Parameters: 
 
- `-f` : Path to the particle dataset
 
- `-i` : Path to save the index
 
- `-m` : Number of workers
 
- `-b` : Data reader chunk size
 
- `-t` : Index type (e.g. `minmax` for linear-based, `rtree` for tree-based)
 
- `-d` : Index storage backend (e.g. `file` or `rocksdb`)
 
- `-x` : Multi-level index type (e.g. `minmax` for linear-based, `rtree` for tree-based)
 
- `-l` : Multi-level slice size (e.g. 10000, means 10k particles in one slice)
 
- `-p` : Constructed attributes of index (e.g., position, momentum), default both.
 
- `--iteration` : Specific iteration to construct the index for
 
- `--species` : Particle species


## Querying

The modified **openPMD-viewer**  is available on the `openPMD-viewer` branch. After cloning, rebuild the softlink to the LegoIndex library:

```bash
ln -s /data/gc/GEOSIndex/build-with-blosc/libgeosindex.so geosindex.so
```
## Testing 

Some simple test cases are available in the `tests` folder of both branches, covering basic functionality.

## Evaluation

To comprehensively evaluate **LegoIndex** performance, we use the workload generator provided under `benchmark/bench.sh` and `benchmark/bench.py` to generate random queries, in the `openPMD-viewer` branch.


We Run large scale evaluation tests whiling monitoring the memory usage in `benchmark/benchTest.sh` and `benchmark/benchTest.py`

### Example Command in benchmark/benchTest.sh


```bash
# Loop select_n times
for j in "${test_type_array[@]}"; do
    for ((i=0; i<${select_n} * ${percentage_num} * 5; i++)); do
        # Clear memory for a fair comparison
        echo 3 | sudo tee /proc/sys/vm/drop_caches

        # Run the benchmark
        nohup python3 -u benchmark/batchTest.py \
            --bpfile $bpfile \
            --index $index \
            --iteration $iteration \
            --species $species \
            --query_seq $i \
            --test_type $j \
            --query_path $benchmark_query_path \
            --limit_memory_usage "$limit_memory_usage" \
            --block_meta_path "$block_meta_path" \
            > ${output_dir}/type_${j}_$(printf '%05d' $i).log 2>&1 &
        
        # Monitor memory usage in the background
        pid=$!
        monitor_process $pid "${output_dir}/type_${j}_$(printf '%05d' $i).memlog"
    done
done
```

### Example Query in benchmark/benchTest.py


```python
ts = OpenPMDTimeSeries(
    path_to_dir=bp_file_path,
    backend='openpmd-api',
    geos_index=geos_index,
    fastbit_index=fastbit_index,
    geos_index_type=geos_index_type,
    geos_index_storage_backend=geos_index_storage_backend,
    geos_index_save_path=geos_index_save_path,
    geos_index_secondary_type=geos_index_secondary_type,
)

start = time.time()

result = ts.get_particle(
    var_list=target_array,
    iteration=iteration,
    species=species,
    select=select_envelope,
    geos_index_use_secondary=geos_index_use_secondary,
    geos_index_direct_block_read=geos_index_direct_block_read,
    geos_index_read_groups=geos_index_read_groups,
    skip_offset=skip_offset,
    limit_memory_usage=limit_memory_usage,
    block_meta_path=block_meta_path,
)

end = time.time()
if len(result) > 0:
    print(f"Total Time: {end - start}, data size: {result[0].size}")
else:
    print(f"Total Time: {end - start}, data size: 0")
```
