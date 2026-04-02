cmake_minimum_required(VERSION 3.20)

project(xmms
    VERSION 1.2.11
    DESCRIPTION "XMMS — X Multimedia System (GTK3/GTK4 resurrection)"
    LANGUAGES C
)

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
option(XMMS_GTK_VERSION     "GTK major version to build against (3 or 4)" 3)
option(XMMS_ENABLE_ALSA     "Build ALSA output plugin"       ON)
option(XMMS_ENABLE_PULSE    "Build PulseAudio output plugin" ON)
option(XMMS_ENABLE_OSS      "Build OSS output plugin"        ON)
option(XMMS_ENABLE_ESD      "Build ESD/esound output plugin" OFF)  # deprecated
option(XMMS_ENABLE_DISKWRITER "Build disk-writer output plugin" ON)
option(XMMS_ENABLE_MPG123   "Build mpg123 input plugin"      ON)
option(XMMS_ENABLE_WAV      "Build WAV input plugin"         ON)
option(XMMS_ENABLE_VORBIS   "Build Ogg/Vorbis input plugin"  ON)
option(XMMS_ENABLE_CDAUDIO  "Build CD-audio input plugin"    ON)
option(XMMS_ENABLE_MIKMOD   "Build MikMod input plugin"      ON)
option(XMMS_ENABLE_TONEGEN  "Build tone-generator input plugin" ON)
option(XMMS_ENABLE_ECHO     "Build echo effect plugin"       ON)
option(XMMS_ENABLE_STEREO   "Build stereo effect plugin"     ON)
option(XMMS_ENABLE_VOICE    "Build voice effect plugin"      ON)
option(XMMS_ENABLE_JOYSTICK "Build joystick general plugin"  ON)
option(XMMS_ENABLE_SONG_CHANGE "Build song-change general plugin" ON)
option(XMMS_ENABLE_IR       "Build IR-man general plugin"    ON)
option(XMMS_ENABLE_WMXMMS   "Build WindowMaker dockapp"      ON)
option(XMMS_ENABLE_BLURSCOPE "Build blur-scope visualization plugin" ON)
option(XMMS_ENABLE_SANALYZER "Build spectrum-analyzer visualization plugin" ON)
option(XMMS_ENABLE_OPENGLSPECTRUM "Build OpenGL spectrum analyzer visualization plugin" ON)
option(XMMS_ONE_PLUGIN_DIR  "Install all plugins into one dir" OFF)
option(XMMS_DISABLE_USER_PLUGIN_DIR "Disable per-user plugin dir" OFF)
option(BUILD_SHARED_LIBS    "Build shared libraries"         ON)

# Always export compile_commands.json for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# C standard
# ---------------------------------------------------------------------------
include(CheckIncludeFile)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Module path
# ---------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(XmmsPlugin)
include(GNUInstallDirs)

# ---------------------------------------------------------------------------
# Compiler warnings
# ---------------------------------------------------------------------------
set(XMMS_C_WARNINGS
    -Wall
    -Wextra
    -Wshadow
    -Wstrict-prototypes
    -Wmissing-prototypes
    -Wno-unused-parameter
    -Wno-sign-compare
)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND XMMS_C_WARNINGS -Og -g)
endif()

# ---------------------------------------------------------------------------
# Install paths
# ---------------------------------------------------------------------------
set(XMMS_DATA_DIR       "${CMAKE_INSTALL_FULL_DATADIR}/xmms")
set(XMMS_LOCALE_DIR     "${CMAKE_INSTALL_FULL_LOCALEDIR}")

if(XMMS_ONE_PLUGIN_DIR)
    set(XMMS_PLUGIN_BASE_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/xmms/Plugins")
else()
    set(XMMS_PLUGIN_BASE_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/xmms")
endif()

# ---------------------------------------------------------------------------
# Compile definitions (propagated to all targets)
# ---------------------------------------------------------------------------
set(XMMS_DEFINES
    # AC_GNU_SOURCE equivalent: expose full POSIX + GNU extensions
    _GNU_SOURCE
    DATA_DIR="${XMMS_DATA_DIR}"
    PLUGIN_DIR="${XMMS_PLUGIN_BASE_DIR}"
    LOCALEDIR="${XMMS_LOCALE_DIR}"
)
if(XMMS_DISABLE_USER_PLUGIN_DIR)
    list(APPEND XMMS_DEFINES DISABLE_USER_PLUGIN_DIR=1)
endif()

# ---------------------------------------------------------------------------
# pkg-config
# ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)

# GTK — version switchable
if(XMMS_GTK_VERSION EQUAL 4)
    pkg_check_modules(GTK REQUIRED IMPORTED_TARGET gtk4 >= 4.0)
    set(XMMS_GTK3 0)
    set(XMMS_GTK4 1)
    list(APPEND XMMS_DEFINES XMMS_GTK4=1)
    message(STATUS "Building against GTK4: ${GTK_VERSION}")
else()
    pkg_check_modules(GTK REQUIRED IMPORTED_TARGET "gtk+-3.0 >= 3.22" gthread-2.0)
    set(XMMS_GTK3 1)
    set(XMMS_GTK4 0)
    list(APPEND XMMS_DEFINES XMMS_GTK3=1)
    message(STATUS "Building against GTK3: ${GTK_VERSION}")
endif()

pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET "glib-2.0 >= 2.32")

# pthreads
find_package(Threads REQUIRED)

# zlib
find_package(ZLIB QUIET)

# X11 / SM
find_package(X11 QUIET)
if(X11_FOUND AND X11_SM_FOUND)
    set(HAVE_SM TRUE)
    set(SM_LIBS "${X11_SM_LIB};${X11_ICE_LIB}")
else()
    set(HAVE_SM FALSE)
    set(SM_LIBS "")
endif()

# XF86VidMode
if(X11_xf86vmode_FOUND)
    set(HAVE_XF86VIDMODE TRUE)
    set(VM_LIBS "Xxf86vm")
else()
    set(HAVE_XF86VIDMODE FALSE)
    set(VM_LIBS "")
endif()

# ALSA
if(XMMS_ENABLE_ALSA)
    pkg_check_modules(ALSA IMPORTED_TARGET "alsa >= 0.9")
    if(ALSA_FOUND)
        list(APPEND XMMS_DEFINES HAVE_ALSA=1)
        message(STATUS "ALSA found: ${ALSA_VERSION}")
    else()
        message(WARNING "ALSA not found — disabling alsa output plugin")
        set(XMMS_ENABLE_ALSA OFF)
    endif()
endif()

# PulseAudio
if(XMMS_ENABLE_PULSE)
    pkg_check_modules(PULSE IMPORTED_TARGET "libpulse-simple >= 0.9")
    if(PULSE_FOUND)
        list(APPEND XMMS_DEFINES HAVE_PULSE=1)
        message(STATUS "PulseAudio found: ${PULSE_VERSION}")
    else()
        message(WARNING "libpulse-simple not found — disabling PulseAudio output plugin")
        set(XMMS_ENABLE_PULSE OFF)
    endif()
endif()

# OSS — header-only check
if(XMMS_ENABLE_OSS)
    check_include_file("sys/soundcard.h" HAVE_SYS_SOUNDCARD_H)
    check_include_file("soundcard.h"     HAVE_SOUNDCARD_H)
    if(HAVE_SYS_SOUNDCARD_H OR HAVE_SOUNDCARD_H)
        list(APPEND XMMS_DEFINES HAVE_OSS=1)
    else()
        message(STATUS "OSS headers not found — disabling oss output plugin")
        set(XMMS_ENABLE_OSS OFF)
    endif()
endif()

# libmpg123
if(XMMS_ENABLE_MPG123)
    pkg_check_modules(MPG123 IMPORTED_TARGET "libmpg123 >= 1.0")
    if(MPG123_FOUND)
        list(APPEND XMMS_DEFINES HAVE_LIBMPG123=1)
    else()
        message(WARNING "libmpg123 not found — disabling mpg123 input plugin")
        set(XMMS_ENABLE_MPG123 OFF)
    endif()
endif()

# Ogg/Vorbis
if(XMMS_ENABLE_VORBIS)
    pkg_check_modules(VORBIS     IMPORTED_TARGET "vorbis >= 1.0")
    pkg_check_modules(VORBISFILE IMPORTED_TARGET "vorbisfile >= 1.0")
    pkg_check_modules(OGG        IMPORTED_TARGET "ogg >= 1.0")
    if(VORBIS_FOUND AND VORBISFILE_FOUND AND OGG_FOUND)
        list(APPEND XMMS_DEFINES HAVE_VORBIS=1 HAVE_OGG=1)
    else()
        message(WARNING "libvorbis/ogg not found — disabling vorbis input plugin")
        set(XMMS_ENABLE_VORBIS OFF)
    endif()
endif()

# MikMod
if(XMMS_ENABLE_MIKMOD)
    find_library(MIKMOD_LIB mikmod)
    find_path(MIKMOD_INCLUDE mikmod.h)
    if(MIKMOD_LIB AND MIKMOD_INCLUDE)
        list(APPEND XMMS_DEFINES HAVE_MIKMOD=1)
    else()
        message(STATUS "libmikmod not found — disabling mikmod input plugin")
        set(XMMS_ENABLE_MIKMOD OFF)
    endif()
endif()

# ESD (deprecated, likely absent)
if(XMMS_ENABLE_ESD)
    find_library(ESD_LIB esd)
    find_path(ESD_INCLUDE esd.h)
    if(ESD_LIB AND ESD_INCLUDE)
        list(APPEND XMMS_DEFINES HAVE_ESD=1)
    else()
        message(STATUS "libesd not found — disabling esd output plugin")
        set(XMMS_ENABLE_ESD OFF)
    endif()
endif()

# OpenGL Spectrum — needs OpenGL + libepoxy (for GtkGLArea)
if(XMMS_ENABLE_OPENGLSPECTRUM)
    find_package(OpenGL)
    pkg_check_modules(Epoxy IMPORTED_TARGET "epoxy >= 1.0")
    if(OPENGL_FOUND AND Epoxy_FOUND)
        message(STATUS "OpenGL Spectrum: OpenGL found, epoxy ${Epoxy_VERSION}")
    else()
        message(STATUS "OpenGL or epoxy not found — disabling opengl_spectrum plugin")
        set(XMMS_ENABLE_OPENGLSPECTRUM OFF)
    endif()
endif()

# Architecture — x86 assembly optimisations in mpg123
include(CheckIncludeFile)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|i.86|AMD64)")
    set(ARCH_X86 TRUE)
    list(APPEND XMMS_DEFINES ARCH_X86=1)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|i.86")
        # Enable x86 ASM decode for mpg123 when using GAS
        set(USE_X86ASM TRUE)
        list(APPEND XMMS_DEFINES USE_X86ASM=1)
    endif()
else()
    set(ARCH_X86 FALSE)
    set(USE_X86ASM FALSE)
endif()

# ---------------------------------------------------------------------------
# config.h generation
# ---------------------------------------------------------------------------
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/xmms_config.h.cmake"
    "${CMAKE_BINARY_DIR}/config.h"
)
include_directories("${CMAKE_BINARY_DIR}")   # so #include "config.h" works

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------
add_subdirectory(libxmms)
add_subdirectory(xmms)
add_subdirectory(Output)
add_subdirectory(Input)
add_subdirectory(Effect)
add_subdirectory(General)
add_subdirectory(Visualization)
if(XMMS_ENABLE_WMXMMS)
    add_subdirectory(wmxmms)
endif()

# ---------------------------------------------------------------------------
# Freedesktop icon set + .desktop entry (issue #38)
# ---------------------------------------------------------------------------
foreach(ICON_SIZE 16 24 32 48 64 128 256)
    install(FILES
        "data/icons/hicolor/${ICON_SIZE}x${ICON_SIZE}/apps/xmms.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${ICON_SIZE}x${ICON_SIZE}/apps"
    )
endforeach()

install(FILES data/xmms.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
)

# Refresh the icon cache after install (best-effort — gtk-update-icon-cache may not be present)
install(CODE "
    find_program(_gtk_update_icon_cache gtk-update-icon-cache)
    if(_gtk_update_icon_cache)
        execute_process(
            COMMAND \${_gtk_update_icon_cache} -f -t
                    \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/icons/hicolor\"
            OUTPUT_QUIET ERROR_QUIET
        )
    endif()
" COMPONENT Runtime)

# ---------------------------------------------------------------------------
# Print summary
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "===== XMMS build summary =====")
message(STATUS "  GTK version  : ${XMMS_GTK_VERSION}")
message(STATUS "  Prefix       : ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Plugin dir   : ${XMMS_PLUGIN_BASE_DIR}")
message(STATUS "  ALSA         : ${XMMS_ENABLE_ALSA}")
message(STATUS "  PulseAudio   : ${XMMS_ENABLE_PULSE}")
message(STATUS "  OSS          : ${XMMS_ENABLE_OSS}")
message(STATUS "  ESD          : ${XMMS_ENABLE_ESD}")
message(STATUS "  mpg123       : ${XMMS_ENABLE_MPG123}")
message(STATUS "  Ogg/Vorbis   : ${XMMS_ENABLE_VORBIS}")
message(STATUS "  MikMod       : ${XMMS_ENABLE_MIKMOD}")
message(STATUS "  CDaudio      : ${XMMS_ENABLE_CDAUDIO}")
message(STATUS "  SM support   : ${HAVE_SM}")
message(STATUS "  XF86VidMode  : ${HAVE_XF86VIDMODE}")
message(STATUS "  x86 ASM      : ${USE_X86ASM}")
message(STATUS "  OpenGL Spec  : ${XMMS_ENABLE_OPENGLSPECTRUM}")
message(STATUS "==============================")
message(STATUS "")
