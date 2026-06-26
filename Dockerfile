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
        jansson-devel texinfo autoconf automake libtool \
        libcurl libcurl-devel zlib-devel gdal-devel \
        expat-devel libxml2-devel diffutils xz bzip2 flex bison \
        numactl-devel hwloc-devel libevent-devel ucx-devel && \
    dnf clean all && rm -rf /var/cache/dnf /var/cache/yum

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

# Build OpenMPI 5.0.6 from source for RHEL9 compatibility
RUN mkdir -p /tmp/build && cd /tmp/build && \
    curl -L -o openmpi-5.0.6.tar.gz https://download.open-mpi.org/release/open-mpi/v5.0/openmpi-5.0.6.tar.gz && \
    tar -xzf openmpi-5.0.6.tar.gz && cd openmpi-5.0.6 && \
    ./configure --prefix=/usr/local/openmpi-5.0.6 \
                --enable-mpi-fortran \
                --enable-shared \
                --enable-static \
                --with-pmix=internal \
                --with-prrte=internal \
                CC=/usr/local/gcc-14.2.0/bin/gcc \
                CXX=/usr/local/gcc-14.2.0/bin/g++ \
                FC=/usr/local/gcc-14.2.0/bin/gfortran && \
    make -j$(nproc) && make install && \
    cd / && rm -rf /tmp/build

# Set OpenMPI environment variables
ENV PATH=/usr/local/openmpi-5.0.6/bin:$PATH
ENV LD_LIBRARY_PATH=/usr/local/openmpi-5.0.6/lib:$LD_LIBRARY_PATH
ENV MANPATH=/usr/local/openmpi-5.0.6/share/man:$MANPATH

# Build UDUNITS, HDF5, and NetCDF (NetCDF 4.9.3 to match HPC)
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
    # Build NetCDF-C 4.9.3
    tar xzvf tarballs/netcdf-c-4.9.3.tar.gz && cd netcdf-c-4.9.3 && \
    CC=mpicc CXX=mpicxx ./configure --prefix=/usr/local && \
    make -j$(nproc) && make install && \
    echo "export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:/usr/local/lib" >> ~/.bashrc && \
    cd / && rm -rf /tmp/tarballs /tmp/udunits-2.2.28 /tmp/hdf5-1.12.0 /tmp/netcdf-c-4.9.3

RUN mkdir -p /usr/local/lib/pkgconfig && \
    echo 'prefix=/usr/local' > /usr/local/lib/pkgconfig/udunits2.pc && \
    echo 'exec_prefix=${prefix}' >> /usr/local/lib/pkgconfig/udunits2.pc && \
    echo 'libdir=${exec_prefix}/lib' >> /usr/local/lib/pkgconfig/udunits2.pc && \
    echo 'includedir=${prefix}/include' >> /usr/local/lib/pkgconfig/udunits2.pc && \
    echo '' >> /usr/local/lib/pkgconfig/udunits2.pc && \
    echo 'Name: UDUNITS-2' >> /usr/local/lib/pkgconfig/udunits2.pc && \
    echo 'Description: API for units of physical quantities' >> /usr/local/lib/pkgconfig/udunits2.pc && \
    echo 'Version: 2.2.28' >> /usr/local/lib/pkgconfig/udunits2.pc && \
    echo 'Requires:' >> /usr/local/lib/pkgconfig/udunits2.pc && \
    echo 'Conflicts:' >> /usr/local/lib/pkgconfig/udunits2.pc && \
    echo 'Libs: -L${libdir} -ludunits2' >> /usr/local/lib/pkgconfig/udunits2.pc && \
    echo 'Libs.private: -lexpat -lm' >> /usr/local/lib/pkgconfig/udunits2.pc && \
    echo 'Cflags: -I${includedir}' >> /usr/local/lib/pkgconfig/udunits2.pc

# Set PKG_CONFIG_PATH
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH