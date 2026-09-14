@echo off
set "PATH=C:\TDM-GCC-64\bin;C:\RosBE\bin;%PATH%"
ninja -j4 -C output-MinGW-amd64 %*
