FROM debian:stable-slim

RUN ln -sf /bin/bash /bin/sh && \
apt-get update && \
apt-get install curl git make cmake gcc g++ valgrind ca-certificates vim-tiny pkg-config -y --no-install-recommends && \
apt-get install openmpi-bin openmpi-common libopenmpi-dev libjansson-dev -y --no-install-recommends

RUN apt-get install  libcurl4 libcurl4-openssl-dev libz-dev libgdal-dev libudunits2-dev libudunits2-data -y --no-install-recommends && \
mkdir -p /custom/tarballs && cd /custom/tarballs && \
curl -O https://support.hdfgroup.org/ftp/HDF5/releases/hdf5-1.12/hdf5-1.12.0/src/hdf5-1.12.0.tar.gz && \
# curl -O https://support.hdfgroup.org/ftp/HDF5/releases/hdf5-1.12/hdf5-1.12.0/src/CMake-hdf5-1.12.0.tar.gz && \
curl -O https://codeload.github.com/Unidata/netcdf-c/tar.gz/v4.7.4 && \
cd .. && tar xzvf tarballs/hdf5-1.12.0.tar.gz && cd hdf5-1.12.0 && \
CC=mpicc CXX=mpicxx  ./configure --prefix=/usr/local --enable-parallel && \
make && make install && \
cd .. && tar xzvf tarballs/v4.7.4 && cd netcdf-c-4.7.4 && \
CC=mpicc CXX=mpicxx ./configure --prefix=/usr/local && \
make && make install && \
echo "export LD_LIBRARY_PATH=\"${LD_LIBRARY_PATH}:/usr/local/lib\"" >> ~/.bashrc
