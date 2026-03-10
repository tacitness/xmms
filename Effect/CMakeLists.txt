# Effect/CMakeLists.txt

if(XMMS_ENABLE_ECHO)
    add_subdirectory(echo_plugin)
endif()

if(XMMS_ENABLE_STEREO)
    add_subdirectory(stereo_plugin)
endif()

if(XMMS_ENABLE_VOICE)
    add_subdirectory(voice)
endif()
