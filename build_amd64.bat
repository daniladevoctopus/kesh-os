@echo off
set "PATH=C:\TDM-GCC-64\bin;C:\RosBE\bin;%PATH%"
ninja -C output-MinGW-amd64 %*
