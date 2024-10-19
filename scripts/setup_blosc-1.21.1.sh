#!/bin/bash

# source scripts/setup_blosc-1.21.1.sh 
# cmake .. -DBUILD_TESTING=OFF -DADIOS2_USE_Blosc2=ON -DADIOS2_USE_Python=ON -DBLOSC_INCLUDE_DIR=${BLOSC_INCLUDE_DIR} -DBLOSC_LIBRARY=${BLOSC_LIBRARY}

# pre-set: SW_DIR="/${PSCRATCH}/sw/"
if [ -z ${SW_DIR} ]
then
  echo "set default SW_DIR: ${PWD}/sw/"
  SW_DIR=${PWD}/sw/
fi
# check if SW_DIR exists, if not make it
if [ ! -d ${SW_DIR} ]
then
  mkdir -p ${SW_DIR}
fi

if [ -d ${SW_DIR}/c-blosc-1.21.1 ]
then
  echo "c-blosc-1.21.1 is already installed in ${SW_DIR}"
else
  build_dir=$(mktemp -d)
  # blosc
  blosc_src_dir=$HOME/src/blosc
  if [ -d ${blosc_src_dir} ]
  then
    cd ${blosc_src_dir}
    git fetch --prune
    git checkout tags/v1.21.1
    git pull
    cd -
  else
    git clone https://github.com/Blosc/c-blosc.git ${blosc_src_dir}
    cd ${blosc_src_dir}
    git checkout tags/v1.21.1
    cd -
  fi
  cmake -S git ${blosc_src_dir} -B ${build_dir}/blosc-build -DCMAKE_INSTALL_PREFIX=${SW_DIR}/c-blosc-1.21.1
  cmake --build ${build_dir}/blosc-build --target install --parallel 30
  rm -rf ${build_dir}
fi

export BLOSC_DIR=${SW_DIR}/c-blosc-1.21.1
export BLOSC_INCLUDE_DIR=${BLOSC_DIR}/include
export BLOSC_LIBRARY=${BLOSC_DIR}/lib/libblosc.so


# Echo variables for verification
echo "BLOSC_DIR: $BLOSC_DIR"
echo "BLOSC_INCLUDE_DIR: $BLOSC_INCLUDE_DIR"
echo "BLOSC_LIBRARY: $BLOSC_LIBRARY"