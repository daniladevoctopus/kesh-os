@echo off
set "PATH=C:\RosBE\bin;C:\RosBE\i386\bin;%PATH%"
ninja -C output-MinGW-i386 %*
