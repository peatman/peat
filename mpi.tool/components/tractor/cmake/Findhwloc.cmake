find_package(PkgConfig QUIET)

if(PkgConfig_FOUND)
    pkg_check_modules(PC_hwloc QUIET hwloc)
endif()

find_path(hwloc_INCLUDE_DIR
    NAMES hwloc.h
    PATHS ${PC_hwloc_INCLUDE_DIRS}
    PATH_SUFFIXES hwloc
)

find_library(hwloc_LIBRARY
    NAMES hwloc
    PATHS ${PC_hwloc_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(hwloc
    FOUND_VAR hwloc_FOUND
    REQUIRED_VARS hwloc_LIBRARY hwloc_INCLUDE_DIR
)

if(hwloc_FOUND)
    set(hwloc_LIBRARIES ${hwloc_LIBRARY})
    set(hwloc_INCLUDE_DIRS ${hwloc_INCLUDE_DIR})
    
    if(NOT TARGET hwloc::hwloc)
        add_library(hwloc::hwloc SHARED IMPORTED)
        set_target_properties(hwloc::hwloc PROPERTIES
            IMPORTED_LOCATION ${hwloc_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${hwloc_INCLUDE_DIR}
        )
    endif()
endif()

mark_as_advanced(hwloc_INCLUDE_DIR hwloc_LIBRARY)

