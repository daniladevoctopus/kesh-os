
add_executable(errortest errortest.c errortest.rc)
set_module_type(errortest win32gui UNICODE)
add_importlibs(errortest user32 comctl32 winmm msvcrt kernel32)
add_cd_file(TARGET errortest DESTINATION reactos/system32 FOR all)
