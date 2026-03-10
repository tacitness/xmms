# General/joystick/CMakeLists.txt

include(CheckIncludeFile)
check_include_file("linux/joystick.h" HAVE_LINUX_JOYSTICK)

if(HAVE_LINUX_JOYSTICK)
    xmms_plugin(
        TARGET    joy
        TYPE      general
        SOURCES
            joy.c joy.h
            configure.c
            about.c
        LINK_LIBS
            Threads::Threads
    )
    target_compile_definitions(joy PRIVATE HAVE_LINUX_JOYSTICK=1)
else()
    message(STATUS "  joystick: linux/joystick.h not found, skipping")
    set(XMMS_ENABLE_JOYSTICK OFF PARENT_SCOPE)
endif()
