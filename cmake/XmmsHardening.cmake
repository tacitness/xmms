# XmmsHardening.cmake
# ---------------------------------------------------------------------------
# Security hardening + sanitizer flags for XMMS.
#
# XMMS parses untrusted input (Winamp skins, m3u/pls playlists, MP3/Ogg/FLAC
# streams), so exploit-mitigation flags are not optional for release builds.
#
# Controlled by two cache options (declared in the top-level CMakeLists.txt):
#   XMMS_ENABLE_HARDENING   ON  — stack protector, FORTIFY, RELRO, PIE, etc.
#   XMMS_ENABLE_SANITIZERS  OFF — ASan + UBSan (mutually relaxes FORTIFY)
#
# Flags are probed with check_c_compiler_flag / check_linker_flag so the build
# stays portable across gcc/clang and glibc/musl (Alpine) toolchains.
# ---------------------------------------------------------------------------
include(CheckCCompilerFlag)
include(CheckLinkerFlag)

# Apply a C compiler flag globally only if the compiler accepts it.
function(xmms_add_c_flag_if_supported flag)
    string(MAKE_C_IDENTIFIER "XMMS_HAVE_CFLAG${flag}" _var)
    check_c_compiler_flag("${flag}" ${_var})
    if(${_var})
        add_compile_options($<$<COMPILE_LANGUAGE:C>:${flag}>)
    endif()
endfunction()

# Apply a linker flag globally only if the linker accepts it.
function(xmms_add_link_flag_if_supported flag)
    string(MAKE_C_IDENTIFIER "XMMS_HAVE_LDFLAG${flag}" _var)
    check_linker_flag(C "${flag}" ${_var})
    if(${_var})
        add_link_options("${flag}")
    endif()
endfunction()

function(xmms_apply_hardening)
    if(NOT XMMS_ENABLE_HARDENING)
        message(STATUS "Hardening flags: DISABLED (XMMS_ENABLE_HARDENING=OFF)")
        return()
    endif()

    # --- Stack protection ---------------------------------------------------
    xmms_add_c_flag_if_supported(-fstack-protector-strong)
    xmms_add_c_flag_if_supported(-fstack-clash-protection)

    # --- Control-flow integrity (x86: Intel CET; arm64: BTI/PAC) ------------
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|i.86|AMD64")
        xmms_add_c_flag_if_supported(-fcf-protection=full)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        xmms_add_c_flag_if_supported(-mbranch-protection=standard)
    endif()

    # --- _FORTIFY_SOURCE ----------------------------------------------------
    # Requires optimization, so only enable in optimized build types.
    # FORTIFY conflicts with ASan instrumentation — skip when sanitizing.
    if(NOT XMMS_ENABLE_SANITIZERS AND NOT XMMS_BUILD_FUZZERS AND
       CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
        # Level 3 needs gcc>=12 or clang>=9; fall back to 2 elsewhere.
        set(_fortify 2)
        if((CMAKE_C_COMPILER_ID STREQUAL "GNU" AND
            CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 12) OR
           (CMAKE_C_COMPILER_ID MATCHES "Clang" AND
            CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 9))
            set(_fortify 3)
        endif()
        # Undef first so we never clash with a toolchain default (-Werror safe).
        add_compile_options($<$<COMPILE_LANGUAGE:C>:-U_FORTIFY_SOURCE>)
        add_compile_definitions(_FORTIFY_SOURCE=${_fortify})
        message(STATUS "Hardening: _FORTIFY_SOURCE=${_fortify}")
    endif()

    # --- Position-independent executables ------------------------------------
    set(CMAKE_POSITION_INDEPENDENT_CODE ON PARENT_SCOPE)
    xmms_add_link_flag_if_supported(-pie)

    # --- Linker hardening ---------------------------------------------------
    xmms_add_link_flag_if_supported(LINKER:-z,relro)
    xmms_add_link_flag_if_supported(LINKER:-z,now)
    xmms_add_link_flag_if_supported(LINKER:-z,noexecstack)
    xmms_add_link_flag_if_supported(LINKER:--as-needed)

    message(STATUS "Hardening flags: ENABLED")
endfunction()

function(xmms_apply_sanitizers)
    if(NOT XMMS_ENABLE_SANITIZERS)
        return()
    endif()
    set(_san "-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined")
    separate_arguments(_san_list UNIX_COMMAND "${_san}")
    add_compile_options(${_san_list})
    add_link_options(-fsanitize=address,undefined)
    message(STATUS "Sanitizers: ENABLED (ASan + UBSan)")
endfunction()
