# Input/mikmod/CMakeLists.txt

xmms_plugin(
    TARGET    mikmod
    TYPE      input
    SOURCES
        drv_xmms.c
        plugin.c
        mikmod-plugin.h
    LINK_LIBS
        ${MIKMOD_LIB}
        Threads::Threads
        libxmms
)

target_include_directories(mikmod PRIVATE ${MIKMOD_INCLUDE})
