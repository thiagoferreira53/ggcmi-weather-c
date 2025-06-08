FROM debian:10

RUN ln -sf /bin/bash /bin/sh && \
apt-get update && \
apt-get install curl git make cmake gcc g++ valgrind ca-certificates vim-tiny pkg-config -y --no-install-recommends && \
apt-get install openmpi-bin openmpi-common libopenmpi-dev libjansson-dev -y --no-install-recommends && \
apt-get install -y autoconf automake libtool texinfo


RUN apt-get install  libcurl4 libcurl4-openssl-dev libz-dev libgdal-dev -y --no-install-recommends && \
mkdir -p /custom/tarballs && cd /custom/tarballs && \
curl -O https://support.hdfgroup.org/ftp/HDF5/releases/hdf5-1.12/hdf5-1.12.0/src/hdf5-1.12.0.tar.gz && \
# curl -O https://support.hdfgroup.org/ftp/HDF5/releases/hdf5-1.12/hdf5-1.12.0/src/CMake-hdf5-1.12.0.tar.gz && \
curl -L -o udunits-2.2.17.tar.gz https://github.com/Unidata/UDUNITS-2/archive/refs/tags/v2.2.17.tar.gz && \
curl -O https://codeload.github.com/Unidata/netcdf-c/tar.gz/v4.9.2 && \
cd .. && tar xzvf tarballs/udunits-2.2.17.tar.gz && cd UDUNITS-2-2.2.17 && \
autoreconf -i && PKG_CONFIG_PATH=/usr/local/lib/pkgconfig ./configure --prefix=/usr/local && \
make -j$(nproc); make install && \
cd .. && tar xzvf tarballs/hdf5-1.12.0.tar.gz && cd hdf5-1.12.0 && \
CC=mpicc CXX=mpicxx  ./configure --prefix=/usr/local --enable-parallel && \
make && make install && \
cd .. && tar xzvf tarballs/v4.9.2 && cd netcdf-c-4.9.2 && \
CC=mpicc CXX=mpicxx ./configure --prefix=/usr/local && \
make && make install && \
echo "export LD_LIBRARY_PATH=\"${LD_LIBRARY_PATH}:/usr/local/lib\"" >> ~/.bashrc
