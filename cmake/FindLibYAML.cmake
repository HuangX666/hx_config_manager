# FindLibYAML.cmake
# Finds system-installed libyaml when FetchContent is not used.
#
# Provides:
#   LibYAML::LibYAML   – imported target
#   LIBYAML_FOUND
#   LIBYAML_INCLUDE_DIRS
#   LIBYAML_LIBRARIES

find_path(LIBYAML_INCLUDE_DIR
    NAMES yaml.h
    PATHS
        /usr/include
        /usr/local/include
        /opt/homebrew/include
        ${LIBYAML_ROOT}/include
)

find_library(LIBYAML_LIBRARY
    NAMES yaml libyaml
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/homebrew/lib
        ${LIBYAML_ROOT}/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibYAML
    DEFAULT_MSG
    LIBYAML_LIBRARY
    LIBYAML_INCLUDE_DIR
)

if(LIBYAML_FOUND AND NOT TARGET LibYAML::LibYAML)
    add_library(LibYAML::LibYAML UNKNOWN IMPORTED)
    set_target_properties(LibYAML::LibYAML PROPERTIES
        IMPORTED_LOCATION             "${LIBYAML_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBYAML_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(LIBYAML_INCLUDE_DIR LIBYAML_LIBRARY)
