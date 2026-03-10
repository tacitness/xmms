# Output/esd/CMakeLists.txt — ESD (esound) output plugin
# ESD is deprecated/absent on most modern systems.
# Guard: only built when XMMS_ENABLE_ESD=ON and libesd is found.

xmms_plugin(
    TARGET    esdout
    TYPE      output
    SOURCES
        esd.c esdout.h
        mixer.c
        about.c
        configure.c
        audio.c
        init.c
    LINK_LIBS
        ${ESD_LIB}
)

target_include_directories(esdout PRIVATE ${ESD_INCLUDE})
