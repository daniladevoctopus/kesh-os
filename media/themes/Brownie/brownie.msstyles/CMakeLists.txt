
if(DLL_EXPORT_VERSION GREATER_EQUAL 0x600)
    add_definitions(-DUSE_PNG_IMAGES)
endif()

list(APPEND TEXTFILES
    textfiles/ExtraLargeNormal.INI
    textfiles/LargeFontsNormal.INI
    textfiles/NormalNormal.INI)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/textfiles)
foreach(_file ${TEXTFILES})
    get_filename_component(_file_name ${_file} NAME_WE)
    get_filename_component(_subdir ${_file} DIRECTORY)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${_subdir})
    set(_converted_file ${CMAKE_CURRENT_BINARY_DIR}/${_subdir}/${_file_name}_utf16.INI)
    utf16le_convert(${CMAKE_CURRENT_SOURCE_DIR}/${_file} ${_converted_file})
    list(APPEND _converted_files ${_converted_file})
endforeach()

set_source_files_properties(brownie.rc PROPERTIES OBJECT_DEPENDS "${_converted_files}")
add_library(brownie.msstyles MODULE brownie.rc)
set_module_type(brownie.msstyles module)
add_dependencies(brownie.msstyles xdk)
set_target_properties(brownie.msstyles PROPERTIES SUFFIX "")
add_cd_file(TARGET brownie.msstyles DESTINATION reactos/Resources/Themes/Brownie FOR all)
