@echo off
set "PATH=C:\TDM-GCC-64\bin;C:\RosBE\bin;%PATH%"
cmake -G Ninja -B output-MinGW-amd64 -DARCH=amd64 -DCMAKE_TOOLCHAIN_FILE=toolchain-gcc.cmake -DCMAKE_BUILD_TYPE=Debug %*
