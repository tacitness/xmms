# libxmms/CMakeLists.txt — shared utility library for XMMS and plugins

set(LIBXMMS_SOURCES
    configfile.c  configfile.h
    xmmsctrl.c    xmmsctrl.h
    dirbrowser.c  dirbrowser.h
    util.c        util.h
    formatter.c   formatter.h
    titlestring.c titlestring.h
    xentry.c      xentry.h
    xconvert.c    xconvert.h
    charset.c     charset.h
)

add_library(libxmms SHARED ${LIBXMMS_SOURCES})

# Preserve the "lib" prefix for the installed .so name (libxmms.so)
set_target_properties(libxmms PROPERTIES
    OUTPUT_NAME  "xmms"
    VERSION      "${PROJECT_VERSION}"
    SOVERSION    "1"
)

target_compile_options(libxmms PRIVATE ${XMMS_C_WARNINGS})
target_compile_definitions(libxmms PRIVATE ${XMMS_DEFINES})

target_include_directories(libxmms
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/xmms>
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_BINARY_DIR}
)

target_link_libraries(libxmms
    PUBLIC
        PkgConfig::GTK
        PkgConfig::GLIB
        Threads::Threads
)

# Public headers installed to ${prefix}/include/xmms/
set(LIBXMMS_PUBLIC_HEADERS
    configfile.h xmmsctrl.h dirbrowser.h util.h
    formatter.h titlestring.h
)

install(TARGETS libxmms
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
install(FILES ${LIBXMMS_PUBLIC_HEADERS}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/xmms"
)
