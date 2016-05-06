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

module purge
module load cmake
module load intel-fc/16.0.2.181
module load intel-cc/16.0.2.181
module load openmpi/1.10.2
module load netcdf/4.3.3.1

set -x
rm -r build
mkdir -p build
pushd build
cmake .. -DCMAKE_C_COMPILER=mpicc -DCMAKE_Fortran_COMPILER=mpif90 \
    -DCMAKE_INCLUDE_PATH=$(echo $CPATH:$LD_LIBRARY_PATH:$(echo $LD_LIBRARY_PATH | sed 's|/lib\>|/lib/Intel|g') | tr ':' ';') \
    -DCMAKE_LIBRARY_PATH=$(echo $LD_LIBRARY_PATH:$(echo $LD_LIBRARY_PATH | sed 's|/lib\>|/lib/Intel|g') | tr ':' ';')
make

