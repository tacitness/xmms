# Output/alsa/CMakeLists.txt

xmms_plugin(
    TARGET    ALSA
    TYPE      output
    SOURCES
        alsa.c alsa.h
        about.c
        audio.c
        configure.c
        init.c
    LINK_LIBS
        PkgConfig::ALSA
        Threads::Threads
)
