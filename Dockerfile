FROM quay.io/centos/centos:stream9

# Enable CRB (CodeReady Builder) repo for development packages
RUN dnf -y install 'dnf-command(config-manager)' && \
    dnf config-manager --set-enabled crb && \
    dnf makecache && \
    dnf clean all && rm -rf /var/cache/dnf /var/cache/yum

# Install base development tools and libraries (added diffutils)
RUN ln -sf /bin/bash /bin/sh && \
    dnf -y update && \
    dnf -y install --allowerasing \
        curl git make cmake gcc gcc-c++ valgrind \
        ca-certificates vim-minimal pkg-config \
        openmpi openmpi-devel \
        jansson-devel texinfo autoconf automake libtool \
        libcurl libcurl-devel zlib-devel gdal-devel \
        expat-devel libxml2-devel diffutils xz bzip2 flex bison && \
    dnf clean all && rm -rf /var/cache/dnf /var/cache/yum

# Set OpenMPI environment variables
ENV PATH=/usr/lib64/openmpi/bin:$PATH
ENV LD_LIBRARY_PATH=/usr/lib64/openmpi/lib:$LD_LIBRARY_PATH
ENV MANPATH=/usr/lib64/openmpi/share/man:$MANPATH

# Build GCC 14.2.0 from source
RUN mkdir -p /tmp/build && cd /tmp/build && \
    curl -L -o gcc-14.2.0.tar.xz https://ftp.gnu.org/gnu/gcc/gcc-14.2.0/gcc-14.2.0.tar.xz && \
    tar -xf gcc-14.2.0.tar.xz && cd gcc-14.2.0 && \
    ./contrib/download_prerequisites && \
    mkdir build && cd build && \
    ../configure --prefix=/usr/local/gcc-14.2.0 \
                 --disable-multilib \
                 --enable-languages=c,c++,fortran \
                 --disable-bootstrap && \
    make -j$(nproc) && make install && \
    ln -sf /usr/local/gcc-14.2.0/bin/* /usr/local/bin/ && \
    cd / && rm -rf /tmp/build

# Update environment so new GCC is used
ENV CC=/usr/local/gcc-14.2.0/bin/gcc
ENV CXX=/usr/local/gcc-14.2.0/bin/g++
ENV FC=/usr/local/gcc-14.2.0/bin/gfortran
ENV PATH=/usr/local/gcc-14.2.0/bin:$PATH
ENV LD_LIBRARY_PATH=/usr/local/gcc-14.2.0/lib64:$LD_LIBRARY_PATH

# Build UDUNITS, HDF5, and NetCDF (NetCDF bumped to 4.9.3)
RUN mkdir -p /tmp/tarballs && cd /tmp/tarballs && \
    curl -L -o hdf5-1.12.0.tar.gz https://support.hdfgroup.org/ftp/HDF5/releases/hdf5-1.12/hdf5-1.12.0/src/hdf5-1.12.0.tar.gz && \
    curl -L -o udunits-2.2.28.tar.gz https://downloads.unidata.ucar.edu/udunits/2.2.28/udunits-2.2.28.tar.gz && \
    curl -L -o netcdf-c-4.9.3.tar.gz https://codeload.github.com/Unidata/netcdf-c/tar.gz/v4.9.3 && \
    cd /tmp && \
    # Build UDUNITS (official release, no autoreconf needed)
    tar xzvf tarballs/udunits-2.2.28.tar.gz && cd udunits-2.2.28 && \
    ./configure --prefix=/usr/local && \
    make -j$(nproc) && make install && \
    cd /tmp && \
    # Build HDF5 with MPI
    tar xzvf tarballs/hdf5-1.12.0.tar.gz && cd hdf5-1.12.0 && \
    CC=mpicc CXX=mpicxx ./configure --prefix=/usr/local --enable-parallel && \
    make -j$(nproc) && make install && \
    cd /tmp && \
    # Build NetCDF-C
    tar xzvf tarballs/netcdf-c-4.9.3.tar.gz && cd netcdf-c-4.9.3 && \
    CC=mpicc CXX=mpicxx ./configure --prefix=/usr/local && \
    make -j$(nproc) && make install && \
    echo "export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:/usr/local/lib" >> ~/.bashrc && \
    cd / && rm -rf /tmp/tarballs /tmp/udunits-2.2.28 /tmp/hdf5-1.12.0 /tmp/netcdf-c-4.9.3

