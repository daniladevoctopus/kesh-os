@echo off
set "PATH=C:\RosBE\bin;C:\RosBE\i386\bin;%PATH%"
ninja -j4 -C output-MinGW-i386 %*
