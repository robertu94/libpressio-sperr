# LibPressio-SPERR

A LibPressio compressor plugin for SPERR. Packaged seperately because of GPL Licensing

## Installation

Via Spack

```
git clone https://github.com/robertu94/spack_packages robertu94_packages
spack repo add ./robertu94_packages

spack install libpressio-sperr
```

Manually Via CMake

```
# install cmake, sperr, libpressio and dependencies first

cmake -S . -B build -DCMAKE_INSTALL_PREFIX
cmake --build build
cmake --install
```
