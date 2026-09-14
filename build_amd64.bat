@echo off
set "PATH=C:\TDM-GCC-64\bin;C:\RosBE\bin;%PATH%"
set "BISON_PKGDATADIR=C:\RosBE\share\bison"
set "M4=m4.exe"
ninja -j4 -C output-MinGW-amd64 %*
