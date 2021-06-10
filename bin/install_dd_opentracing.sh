#!/bin/sh

set -e # exit if a non-conditional command fails

# Gets the latest release version number from Github.
get_latest_release() {
  wget -qO- "https://api.github.com/repos/$1/releases/latest" |
    grep '"tag_name":' |
    sed -E 's/.*"([^"]+)".*/\1/';
}

DD_OPENTRACING_CPP_VERSION="$(get_latest_release DataDog/dd-opentracing-cpp)"

# Download and install dd-opentracing-cpp library.
wget https://github.com/DataDog/dd-opentracing-cpp/archive/${DD_OPENTRACING_CPP_VERSION}.tar.gz -O dd-opentracing-cpp.tar.gz
mkdir -p dd-opentracing-cpp/.build
tar zxvf dd-opentracing-cpp.tar.gz -C ./dd-opentracing-cpp/ --strip-components=1
cd dd-opentracing-cpp/.build

# Download and install the correct version of opentracing-cpp, & other deps.
../scripts/install_dependencies.sh

cmake ..
make -j $(nproc)
make install

# Install dependency headers and libraries, for use by users of dd_opentracing.
cp ../deps/lib/*.a /usr/local/lib
cp --recursive ../deps/include/opentracing /usr/local/include
