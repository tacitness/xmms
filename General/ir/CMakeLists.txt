# General/ir/CMakeLists.txt — IR-man general plugin

xmms_plugin(
    TARGET    ir
    TYPE      general
    SOURCES
        ir.c ir.h
        irman.h
        configure.c
        about.c
        irio.c
        irfunc.c
    LINK_LIBS
        Threads::Threads
)
