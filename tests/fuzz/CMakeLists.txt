# tests/fuzz/CMakeLists.txt — libFuzzer harnesses for untrusted-input parsers.
#
# Enabled with -DXMMS_BUILD_FUZZERS=ON (requires clang). The top-level
# CMakeLists adds -fsanitize=fuzzer-no-link,address,undefined to all targets
# when fuzzing is on; here we link each harness with -fsanitize=fuzzer.

if(NOT XMMS_BUILD_FUZZERS)
    return()
endif()

set(XMMS_FUZZERS
    fuzz_configfile     # INI / .pls playlist parser  (xmms_cfg_open_file)
    fuzz_titlestring    # title-format %-escape parser (xmms_get_titlestring)
)

foreach(fuzzer ${XMMS_FUZZERS})
    add_executable(${fuzzer} ${fuzzer}.c)
    target_link_libraries(${fuzzer}
        PRIVATE
            libxmms
            PkgConfig::GTK
            PkgConfig::GLIB
    )
    target_include_directories(${fuzzer} PRIVATE "${CMAKE_SOURCE_DIR}/libxmms")
    target_compile_definitions(${fuzzer} PRIVATE _GNU_SOURCE)
    target_compile_options(${fuzzer} PRIVATE -fsanitize=fuzzer)
    target_link_options(${fuzzer} PRIVATE -fsanitize=fuzzer)

    # Quick CI smoke: run the fuzzer briefly against its seed corpus. A crash
    # makes ctest fail. Long campaigns run out-of-band (OSS-Fuzz / nightly).
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/corpus/${fuzzer}")
        set(_corpus "${CMAKE_CURRENT_SOURCE_DIR}/corpus/${fuzzer}")
    else()
        set(_corpus "")
    endif()
    add_test(
        NAME ${fuzzer}_smoke
        COMMAND ${fuzzer} -runs=20000 -max_total_time=30 -timeout=10 ${_corpus}
    )
endforeach()
