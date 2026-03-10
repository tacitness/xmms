# General/CMakeLists.txt

if(XMMS_ENABLE_JOYSTICK)
    add_subdirectory(joystick)
endif()

if(XMMS_ENABLE_SONG_CHANGE)
    add_subdirectory(song_change)
endif()

if(XMMS_ENABLE_IR)
    add_subdirectory(ir)
endif()
