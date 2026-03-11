# cmake/XmmsPlugin.cmake
# Helper macro for defining XMMS plugin shared libraries (MODULE).
# Usage:
#   xmms_plugin(TARGET <name>
#               SOURCES <src1> [<src2> ...]
#               TYPE    <input|output|effect|general|visualization>
#               [LINK_LIBS <lib1> ...]
#               [OPTIONAL])
#
# Produces a MODULE library installed into ${XMMS_PLUGIN_DIR}/<Type>/
# Named lib<name>.so — matching the Autotools convention.

include(GNUInstallDirs)

macro(xmms_plugin)
    cmake_parse_arguments(XPLUGIN
        "OPTIONAL"
        "TARGET;TYPE"
        "SOURCES;LINK_LIBS"
        ${ARGN}
    )

    if(NOT XPLUGIN_TARGET)
        message(FATAL_ERROR "xmms_plugin: TARGET is required")
    endif()

    if(NOT XPLUGIN_TYPE)
        message(FATAL_ERROR "xmms_plugin: TYPE is required (input|output|effect|general|visualization)")
    endif()

    string(TOLOWER "${XPLUGIN_TYPE}" _plugin_type_lower)
    string(TOUPPER "${XPLUGIN_TYPE}" _plugin_type_upper)

    # Map type → plugin subdirectory name (mirrors Autotools one-plugin-dir=no)
    if(_plugin_type_lower STREQUAL "input")
        set(_plugin_subdir "Input")
    elseif(_plugin_type_lower STREQUAL "output")
        set(_plugin_subdir "Output")
    elseif(_plugin_type_lower STREQUAL "effect")
        set(_plugin_subdir "Effect")
    elseif(_plugin_type_lower STREQUAL "general")
        set(_plugin_subdir "General")
    elseif(_plugin_type_lower STREQUAL "visualization")
        set(_plugin_subdir "Visualization")
    else()
        message(FATAL_ERROR "xmms_plugin: unknown TYPE '${XPLUGIN_TYPE}'")
    endif()

    add_library(${XPLUGIN_TARGET} MODULE ${XPLUGIN_SOURCES})

    # Standard compile options inherited from root
    target_compile_options(${XPLUGIN_TARGET} PRIVATE
        ${XMMS_C_WARNINGS}
    )

    target_compile_definitions(${XPLUGIN_TARGET} PRIVATE
        ${XMMS_DEFINES}
    )

    target_include_directories(${XPLUGIN_TARGET} PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/libxmms
        ${GTK_INCLUDE_DIRS}
        ${GLIB_INCLUDE_DIRS}
    )

    target_link_libraries(${XPLUGIN_TARGET} PRIVATE
        ${XPLUGIN_LINK_LIBS}
        ${GTK_LIBRARIES}
    )

    # Plugins must be position-independent, no SONAME, no lib prefix in install.
    # Do NOT set C_VISIBILITY_PRESET to hidden: pluginenum.c uses dlsym() to
    # find get_iplugin_info/get_oplugin_info/etc., so those entry points must
    # appear in the dynamic symbol table with default visibility.
    set_target_properties(${XPLUGIN_TARGET} PROPERTIES
        PREFIX        ""
        NO_SONAME     TRUE
    )

    install(TARGETS ${XPLUGIN_TARGET}
        LIBRARY DESTINATION "${XMMS_PLUGIN_BASE_DIR}/${_plugin_subdir}"
    )

    message(STATUS "  [plugin] ${_plugin_subdir}/${XPLUGIN_TARGET}")
endmacro()
