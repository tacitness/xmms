# Output/CMakeLists.txt

include(CheckIncludeFile)

if(XMMS_ENABLE_ALSA)
    add_subdirectory(alsa)
endif()

if(XMMS_ENABLE_PULSE)
    add_subdirectory(pulse)
endif()

if(XMMS_ENABLE_OSS)
    add_subdirectory(OSS)
endif()

if(XMMS_ENABLE_ESD)
    add_subdirectory(esd)
endif()

add_subdirectory(disk_writer)
