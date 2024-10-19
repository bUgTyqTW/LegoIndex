cmake_minimum_required(VERSION 3.19)
project(GEOSIndex)

set(CMAKE_CXX_STANDARD 17)

# Set the build type to Debug
set(CMAKE_BUILD_TYPE Debug)

#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")

add_subdirectory(third_party/pybind11)
add_subdirectory(third_party/adios2)
#add_subdirectory(third_party/rocksdb)
add_subdirectory(third_party/protobuf)
add_subdirectory(third_party/GEOS)

find_package(Python REQUIRED COMPONENTS Development)
# Include Python header directories
include_directories(${Python_INCLUDE_DIRS})

find_package(MPI REQUIRED)
include_directories(${MPI_INCLUDE_PATH})

# Find the GEOS library
include_directories(third_party/GEOS/include)
include_directories(third_party/pybind11/include)
include_directories(third_party/protobuf/src)
include_directories(third_party/adios2)
include_directories(third_party/rocksdb/include)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/third_party/GEOS/capi)

include_directories(include)
include_directories(proto)
file(GLOB APP_SOURCES src/*.cpp proto/*.cc)

set(GEOS_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/third_party/GEOS/lib/libgeos.so)
#set(ADIOS2_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/third_party/adios2/build/lib/libadios2_cxx11.so)

# Due to the gtest lib of rocksdb is conflict with that of adios2, so compile rocksdb separately
set(ROCKSDB_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rocksdb/build/librocksdb.so)

# Add your own source files
#add_executable(GEOSIndex main.cpp ${APP_SOURCES})  # Generate a shared library (.so file)
#add_executable(test_bloomfilter tests/testBloomFilter.cpp ${APP_SOURCES})
# Link the GEOS library
#target_link_libraries(GEOSIndex ${GEOS_LIBRARIES} ${PYTHON_LIBRARIES} protobuf::libprotobuf adios2 ${MPI_LIBRARIES} ${ROCKSDB_LIBRARY})

add_executable(test_adios_read tests/testADIOS2Read.cpp)
target_link_libraries(test_adios_read adios2 ${MPI_LIBRARIES})

add_executable(test_adios_read_block tests/testADIOS2ReadBlock.cpp)
target_link_libraries(test_adios_read_block adios2 ${MPI_LIBRARIES})

add_executable(test_adios_thread tests/testADIOS2Threads.cpp)
target_link_libraries(test_adios_thread adios2 ${MPI_LIBRARIES})

add_executable(test_adios_read_statics tests/testADIOS2Readstatics.cpp)
target_link_libraries(test_adios_read_statics adios2 ${MPI_LIBRARIES})

add_executable(test_adios_read_statics_encodev tests/testADIOS2Readstatics_encodev.cpp)
target_link_libraries(test_adios_read_statics_encodev adios2 ${MPI_LIBRARIES})


add_executable(test_build tests/testBuild.cpp ${APP_SOURCES})
target_link_libraries(test_build ${GEOS_LIBRARIES} ${PYTHON_LIBRARIES} protobuf::libprotobuf adios2 ${MPI_LIBRARIES} ${ROCKSDB_LIBRARY})

add_executable(test_query tests/testQuery.cpp ${APP_SOURCES})
target_link_libraries(test_query ${GEOS_LIBRARIES} ${PYTHON_LIBRARIES} protobuf::libprotobuf adios2 ${MPI_LIBRARIES} ${ROCKSDB_LIBRARY})

# add_library(geosindex SHARED main.cpp ${APP_SOURCES})
# target_link_libraries(geosindex ${GEOS_LIBRARIES} ${PYTHON_LIBRARIES} protobuf::libprotobuf adios2 ${MPI_LIBRARIES} ${ROCKSDB_LIBRARY})
# set_target_properties(geosindex PROPERTIES VERSION 1.1)

add_executable(test_change_block_size_by_variable tests/testChangeBlockSizeByVariable.cpp)
target_link_libraries(test_change_block_size_by_variable adios2 ${MPI_LIBRARIES})

add_executable(test_change_block_size_for_particle_shrink tests/testChangeBlockSizeForParticle_Shrink.cpp)
target_link_libraries(test_change_block_size_for_particle_shrink adios2 ${MPI_LIBRARIES})

add_executable(test_change_block_size_for_particle_extend tests/testChangeBlockSizeForParticle_Extend.cpp)
target_link_libraries(test_change_block_size_for_particle_extend adios2 ${MPI_LIBRARIES})

add_executable(test_read_and_write_attr tests/testReadAndWriteAttr.cpp)
target_link_libraries(test_read_and_write_attr adios2 ${MPI_LIBRARIES})

add_library(geosindex MODULE python/geosindex.cpp ${APP_SOURCES})
target_link_libraries(geosindex ${GEOS_LIBRARIES} ${PYTHON_LIBRARIES} protobuf::libprotobuf adios2 ${MPI_LIBRARIES} ${ROCKSDB_LIBRARY})
set_target_properties(geosindex PROPERTIES VERSION 1.2 OUTPUT_NAME "geosindex")