# Input/CMakeLists.txt

if(XMMS_ENABLE_MPG123)
    add_subdirectory(mpg123)
endif()

if(XMMS_ENABLE_WAV)
    add_subdirectory(wav)
endif()

if(XMMS_ENABLE_VORBIS)
    add_subdirectory(vorbis)
endif()

if(XMMS_ENABLE_CDAUDIO)
    add_subdirectory(cdaudio)
endif()

if(XMMS_ENABLE_MIKMOD)
    add_subdirectory(mikmod)
endif()

if(XMMS_ENABLE_TONEGEN)
    add_subdirectory(tonegen)
endif()
