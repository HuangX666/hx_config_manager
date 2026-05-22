# FindMXML.cmake
# Finds system-installed Mini-XML (mxml) when FetchContent is not used.
#
# Provides:
#   MXML::MXML         – imported target
#   MXML_FOUND
#   MXML_INCLUDE_DIRS
#   MXML_LIBRARIES

find_path(MXML_INCLUDE_DIR
    NAMES mxml.h
    PATHS
        /usr/include
        /usr/local/include
        /opt/homebrew/include
        ${MXML_ROOT}/include
)

find_library(MXML_LIBRARY
    NAMES mxml libmxml mxml4
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/homebrew/lib
        ${MXML_ROOT}/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MXML
    DEFAULT_MSG
    MXML_LIBRARY
    MXML_INCLUDE_DIR
)

if(MXML_FOUND AND NOT TARGET MXML::MXML)
    add_library(MXML::MXML UNKNOWN IMPORTED)
    set_target_properties(MXML::MXML PROPERTIES
        IMPORTED_LOCATION             "${MXML_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${MXML_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(MXML_INCLUDE_DIR MXML_LIBRARY)
