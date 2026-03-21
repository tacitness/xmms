# Output/pulse/CMakeLists.txt

xmms_plugin(
    TARGET    Pulse
    TYPE      output
    SOURCES
        pulse.c pulse.h
        audio.c
        configure.c
        about.c
    LINK_LIBS
        PkgConfig::PULSE
        Threads::Threads
)
