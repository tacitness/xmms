# General/song_change/CMakeLists.txt

xmms_plugin(
    TARGET    song_change
    TYPE      general
    SOURCES
        song_change.c
    LINK_LIBS
        Threads::Threads
)
