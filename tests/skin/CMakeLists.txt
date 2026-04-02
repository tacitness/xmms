find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(SKIN_TEST_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(SKIN_TEST_FIXTURE_DIR "${SKIN_TEST_GENERATED_DIR}/fixtures")

add_custom_command(
    OUTPUT
        "${SKIN_TEST_FIXTURE_DIR}/classic.wsz"
        "${SKIN_TEST_FIXTURE_DIR}/custom-overlay.wsz"
        "${SKIN_TEST_FIXTURE_DIR}/missing-eqmain.wsz"
        "${SKIN_TEST_FIXTURE_DIR}/shape-region.wsz"
        "${SKIN_TEST_FIXTURE_DIR}/malicious-traversal.wsz"
        "${SKIN_TEST_FIXTURE_DIR}/corrupt.wsz"
    COMMAND
        ${CMAKE_COMMAND} -E make_directory "${SKIN_TEST_FIXTURE_DIR}"
    COMMAND
        ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/generate_skin_fixtures.py"
        --output-dir "${SKIN_TEST_FIXTURE_DIR}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/generate_skin_fixtures.py"
)

add_custom_target(skin_test_fixtures ALL
    DEPENDS
        "${SKIN_TEST_FIXTURE_DIR}/classic.wsz"
        "${SKIN_TEST_FIXTURE_DIR}/custom-overlay.wsz"
        "${SKIN_TEST_FIXTURE_DIR}/missing-eqmain.wsz"
        "${SKIN_TEST_FIXTURE_DIR}/shape-region.wsz"
        "${SKIN_TEST_FIXTURE_DIR}/malicious-traversal.wsz"
        "${SKIN_TEST_FIXTURE_DIR}/corrupt.wsz"
)

add_executable(skin_preview_tool
    skin_preview_tool.c
    skin_preview.c
    "${CMAKE_SOURCE_DIR}/xmms/skin_archive.c"
)

target_compile_options(skin_preview_tool PRIVATE ${XMMS_C_WARNINGS})
target_compile_definitions(skin_preview_tool PRIVATE ${XMMS_DEFINES})
target_include_directories(skin_preview_tool PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/xmms"
    "${CMAKE_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(skin_preview_tool PRIVATE
    PkgConfig::GTK
    PkgConfig::GLIB
    m
)
add_dependencies(skin_preview_tool skin_test_fixtures)

add_executable(test_skin_archive
    test_skin_archive.c
    skin_preview.c
    "${CMAKE_SOURCE_DIR}/xmms/skin_archive.c"
)

target_compile_options(test_skin_archive PRIVATE ${XMMS_C_WARNINGS})
target_compile_definitions(test_skin_archive PRIVATE
    ${XMMS_DEFINES}
    SKIN_TEST_FIXTURE_DIR="${SKIN_TEST_FIXTURE_DIR}"
    SKIN_TEST_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
)
target_include_directories(test_skin_archive PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/xmms"
    "${CMAKE_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(test_skin_archive PRIVATE
    PkgConfig::GTK
    PkgConfig::GLIB
    m
)
add_dependencies(test_skin_archive skin_test_fixtures)

add_test(NAME skin_archive COMMAND test_skin_archive)
set_tests_properties(skin_archive PROPERTIES LABELS "skin")
