//
// Created by chang on 5/25/23.
//

#include <iostream>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <Python.h>

#include <chrono>
#include <geosindex/rtreebuild.h>
#include <geosindex/rtreequery.h>

#include <geosindex/minmaxquery.h>
#include <geosindex/minmaxbuild.h>


int main() {
    // Start the timer
    auto start = std::chrono::high_resolution_clock::now();

//    geosindex::BuildGEOSIndex buildGEOSIndex("/data/gc/rocksdb-index/WarpX/build/bin/diags/diag1/openpmd.bp",
//                                             {"position", "momentum"},
//                                             "/data/gc/rocksdb-index/WarpX/build/bin/diags/diag1/rocksdb_test",
//                                             "electrons",
//                                             10,
//                                             5,
//                                             10);
//
//
//    buildGEOSIndex.initJobsQueue();
//    // use a separate thread to execuate batchRead function
//    std::thread batchReadThread(&geosindex::BuildGEOSIndex::batchRead, &buildGEOSIndex);
//
//    buildGEOSIndex.buildSecondarySTRtreeParallelWithData();
//    buildGEOSIndex.buildFirstSTRtree3dByBlockSliceList();
//    buildGEOSIndex.writeBatchToDB();

    // buildGEOSIndex.buildSTRtreeParallel();
    // // print the size of the secondaryJobsQueue
    // std::cout << "The size of the secondaryJobsQueue is " << buildGEOSIndex.secondaryJobsQueue.size() << std::endl;


    // buildGEOSIndex.buildSecondarySTRtreeParallel();
    // buildGEOSIndex.buildFirstSTRtree3dByBlockSliceList();



//    buildGEOSIndex.buildFirstSTRtree3d();
//    geosindex::QueryGEOSINDEX query_test("/data/gc/rocksdb-index/WarpX/build/bin/diags/diag1/rocksdb");
//    auto result = query_test.queryByXYZ("/data/400/particles/electrons/position/", -0.06996e-25, -0.06996e-24, -0.06996e-5, 0.06996e-5, 4.996e-05, 7.996e-02);
//    query_test.queryByXYZ("/data/400/particles/electrons/position/", result, -10.06996e-05, 0.06996e-05, -10.06996e-05, 0.06996e-05, -10.06996e-05, 0.06996e-01);

//    batchReadThread.join();


    // End the timer
    auto end = std::chrono::high_resolution_clock::now();

    // Calculate the duration in seconds
    std::chrono::duration<double> duration = end - start;
    double seconds = duration.count();

    // Output the running time
    std::cout << "The code execution took " << seconds << " seconds." << std::endl;


    std::cout << "test" << std::endl;
    return 0;
}


