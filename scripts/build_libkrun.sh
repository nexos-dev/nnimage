#!/bin/sh
# buildlibkrun.sh - builds libkrun library for platforms that don't have it in a package manager
# Copyright 2026 Jedidiah Thompson
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

panic()
{
    echo "$0: error: $1"
    exit 1
}

checkerr()
{
    if [ $2 -ne 0 ]
    then
        panic "$1"
    fi
}

outputdir=$1
jobs=$2
suppressmsg=$3
[ -z "$outputdir" ] && panic "Output directory not specified"

if [ "$2" != "-s" ]
then
    echo "Please ensure the following dependencies are installed on your system:"
    echo "  - GNU Make"
    echo "  - GCC or Clang"
    echo "  - A working rust toolchain"
    echo "  - Flex and Bison"
    echo "  - xz"
    echo "  - tar"
    echo "  - git"
    echo "  - curl"
    echo "  - sudo"
    echo "  - libelf"
    echo "  - Python 3"
    echo "  - pyelftools"
    echo "  - patchelf"
    echo "  - static libc"
    echo "  - patch"
    echo "If not, please install them and re-run this script."
    sleep 3
fi

# Clone libkrunfw
if [ ! -d "$outputdir/libkrunfw" ]
then
    git clone https://github.com/libkrun/libkrunfw.git "$outputdir/libkrunfw" -b v5.5.0
    checkerr "Failed to clone libkrunfw" $?
fi

# Clone libkrun
if [ ! -d "$outputdir/libkrun" ]
then
    git clone https://github.com/libkrun/libkrun.git "$outputdir/libkrun" -b v1.19.4
    checkerr "Failed to clone libkrun" $?
fi

if [ ! -z "$jobs" ]
then
    jobsarg="-j $jobs"
else
    jobsarg=""
fi

# Now we need to build libkrunfw. If not running on Linux, we must build using krunvm
olddir=$PWD
cd $outputdir/libkrunfw
if [ "$(uname)" != "Linux" ]
then
    ./build_on_krunvm.sh
    checkerr "Failed to build libkrunfw" $?
    make $jobsarg
    checkerr "Failed to build libkrunfw" $?
    echo "Requesting sudo privileges to install libkrunfw..."
    sudo make install
    checkerr "Failed to install libkrunfw" $?
else
    make $jobsarg
    checkerr "Failed to build libkrunfw" $?
    echo "Requesting sudo privileges to install libkrun..."
    sudo make install
    checkerr "Failed to install libkrunfw" $?
fi

# Now build libkrun
cd $outputdir/libkrun
make BLK=1 NET=1 $jobsarg
checkerr "Failed to build libkrun" $?
sudo make install
checkerr "Failed to install libkrun" $?
