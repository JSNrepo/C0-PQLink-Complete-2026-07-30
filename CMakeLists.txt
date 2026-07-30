cmake_minimum_required(VERSION 3.16)
project(c0pqlink VERSION 0.1.0 LANGUAGES C CXX)

option(C0PQLINK_BUILD_TESTS "Build C0-PQLink tests" ON)
option(C0PQLINK_ENABLE_SANITIZERS "Enable address/undefined sanitizers" OFF)

add_library(c0pqlink STATIC
    src/core/keccak.c
    src/core/sha256.c
    src/core/mlkem512_stream.c
    src/core/ascon_aead128.c
    src/session/crypto.c
    src/session/preflight.c
    src/session/fragment.c
    src/session/ratchet.c
    src/session/migration.c
)

target_include_directories(c0pqlink
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(c0pqlink PUBLIC c_std_99)
target_compile_options(c0pqlink PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wshadow;-Wstrict-prototypes>
)

if(C0PQLINK_ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(c0pqlink PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(c0pqlink PRIVATE -fsanitize=address,undefined)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

configure_package_config_file(
    cmake/c0pqlinkConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/c0pqlinkConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/c0pqlink
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/c0pqlinkConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(
    TARGETS c0pqlink
    EXPORT c0pqlinkTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(
    EXPORT c0pqlinkTargets
    FILE c0pqlinkTargets.cmake
    NAMESPACE C0PQLink::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/c0pqlink
)
install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/c0pqlinkConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/c0pqlinkConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/c0pqlink
)

if(C0PQLINK_BUILD_TESTS)
    enable_testing()
    add_executable(c0pqlink_tests
        tests/test_main.c
        tests/test_core.c
        tests/test_session.c
    )
    target_link_libraries(c0pqlink_tests PRIVATE c0pqlink)
    target_include_directories(c0pqlink_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(c0pqlink_tests PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wshadow>
    )
    if(C0PQLINK_ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(c0pqlink_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(c0pqlink_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME c0pqlink_tests COMMAND c0pqlink_tests)
endif()
