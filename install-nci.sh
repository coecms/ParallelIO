#!/bin/bash
#  Copyright 2016 ARC Centre of Excellence for Climate Systems Science
#
#  \author  Scott Wales <scott.wales@unimelb.edu.au>
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

set -eu

INSTALL_PREFIX=$HOME/scratch/pio
[ -d $INSTALL_PREFIX ] || (echo "Install prefix not a directory - ${INSTALL_PREFIX}" && exit)

VERSION=1.10.0

# Open MPI has a stable interface at each even minor release, e.g. 1.10.1 is
# compatible with 1.10.2
MPIS=( \
    openmpi/1.6.5 \
    openmpi/1.8.8 \
    openmpi/1.10.2 \
    intel-mpi/5.1.2.150 \
)

for mpi in ${MPIS[*]}; do
    module purge
    module load cmake
    module load intel-fc/16.0.2.181
    module load intel-cc/16.0.2.181
    module load netcdf/4.3.3.1p
    module load hdf5/1.8.14p
    # Load MPI and get the library type
    module load "${mpi}"
    set +u
    if [ -n "$OPENMPI_VERSION" ]; then
        MPI_VERSION="ompi${OPENMPI_VERSION%.*}"
    else
        MPI_VERSION="impi"
    fi
    set -u
    COMPILER_VERSION="intel"

    rm -r build
    mkdir -p build
    pushd build
        DESTDIR=${INSTALL_PREFIX}/${VERSION}-${COMPILER_VERSION}-${MPI_VERSION} 
        [ ! -d "$DESTDIR" ] || (echo "Destination already present - ${DESTDIR}" && exit)

        cmake .. -DCMAKE_C_COMPILER=mpicc -DCMAKE_Fortran_COMPILER=mpif90 \
            -DCMAKE_INSTALL_PREFIX=$DESTDIR \
            -DPIO_FILESYSTEM_HINTS=lustre \
            -DCMAKE_Fortran_FLAGS='-xHost -O2 -g -traceback -warn all' \
            -DCMAKE_INCLUDE_PATH=$(echo $CPATH:$LD_LIBRARY_PATH:$(echo $LD_LIBRARY_PATH | sed 's|/lib\>|/lib/Intel|g') | tr ':' ';') \
            -DCMAKE_LIBRARY_PATH=$(echo $LD_LIBRARY_PATH:$(echo $LD_LIBRARY_PATH | sed 's|/lib\>|/lib/Intel|g') | tr ':' ';')
        make
        ctest
        make install
    popd
done
