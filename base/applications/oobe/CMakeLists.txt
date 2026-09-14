
add_executable(oobe oobe.c oobe.rc)
set_module_type(oobe win32gui UNICODE)
add_importlibs(oobe user32 gdi32 msimg32 comctl32 iphlpapi ws2_32 advapi32 netapi32 msvcrt kernel32)
add_cd_file(TARGET oobe DESTINATION reactos/system32 FOR all)
