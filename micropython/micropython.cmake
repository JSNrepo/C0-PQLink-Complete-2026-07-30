set(C0PQLINK_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

add_library(usermod_c0pqlink INTERFACE)

target_sources(usermod_c0pqlink INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/modc0pqlink.c
    ${C0PQLINK_ROOT}/src/core/keccak.c
    ${C0PQLINK_ROOT}/src/core/sha256.c
    ${C0PQLINK_ROOT}/src/core/mlkem512_stream.c
    ${C0PQLINK_ROOT}/src/core/ascon_aead128.c
    ${C0PQLINK_ROOT}/src/session/crypto.c
    ${C0PQLINK_ROOT}/src/session/preflight.c
    ${C0PQLINK_ROOT}/src/session/fragment.c
    ${C0PQLINK_ROOT}/src/session/ratchet.c
    ${C0PQLINK_ROOT}/src/session/migration.c
)

target_include_directories(usermod_c0pqlink INTERFACE
    ${C0PQLINK_ROOT}/include
    ${C0PQLINK_ROOT}/src
)

target_link_libraries(usermod INTERFACE usermod_c0pqlink)
