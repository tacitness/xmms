# Input/vorbis/CMakeLists.txt

xmms_plugin(
    TARGET    vorbis
    TYPE      input
    SOURCES
        vorbis.c vorbis.h
        configure.c
        http.c http.h
        fileinfo.c
        vcedit.c vcedit.h
    LINK_LIBS
        PkgConfig::VORBISFILE
        PkgConfig::VORBIS
        PkgConfig::OGG
        Threads::Threads
        libxmms
)
