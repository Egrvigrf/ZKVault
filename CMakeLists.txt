cmake_minimum_required(VERSION 3.16)
project(ZKVault LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

option(
    ZKVAULT_BUILD_DEV_TOOLS
    "Build the private-post protocol developer tools"
    ON
)
option(
    ZKVAULT_BUILD_LEGACY
    "Build the legacy local vault CLI, shell, and TUI prototype"
    OFF
)

find_package(OpenSSL REQUIRED)

set(ZKVAULT_CORE_SOURCES
    src/content/private_post.cpp
    src/content/private_post_file.cpp
    src/crypto/aead.cpp
    src/crypto/hex.cpp
    src/crypto/kdf.cpp
    src/crypto/random.cpp
    src/storage/atomic_file.cpp
)

set(ZKVAULT_TERMINAL_SUPPORT_SOURCES
    src/terminal/prompt.cpp
)

add_library(zkvault_core
    ${ZKVAULT_CORE_SOURCES}
)

target_include_directories(zkvault_core PUBLIC
    src
    external
)

target_link_libraries(zkvault_core PUBLIC OpenSSL::Crypto)

add_library(zkvault_terminal_support
    ${ZKVAULT_TERMINAL_SUPPORT_SOURCES}
)

target_include_directories(zkvault_terminal_support PUBLIC
    src
)

target_link_libraries(zkvault_terminal_support PUBLIC zkvault_core)

if(ZKVAULT_BUILD_DEV_TOOLS)
    add_executable(zkvault
        src/tools/private_post_cli.cpp
    )

    target_link_libraries(zkvault PRIVATE
        zkvault_core
        zkvault_terminal_support
    )
endif()

if(ZKVAULT_BUILD_LEGACY)
    set(ZKVAULT_LEGACY_CORE_SOURCES
        src/app/frontend_contract.cpp
        src/app/vault_app.cpp
        src/app/vault_codec.cpp
        src/app/vault_session.cpp
        src/storage/json_storage.cpp
        src/storage/master_key_storage.cpp
    )

    set(ZKVAULT_LEGACY_SERVICE_SOURCES
        src/service/vault_runtime.cpp
    )

    set(ZKVAULT_LEGACY_TERMINAL_FRONTEND_SOURCES
        src/shell/interactive_shell.cpp
        src/shell/shell_runtime.cpp
        src/terminal/display.cpp
        src/tui/terminal_ui_commands.cpp
        src/tui/terminal_ui_confirmations.cpp
        src/tui/terminal_ui_flow.cpp
        src/tui/terminal_ui_forms.cpp
        src/tui/terminal_ui_input.cpp
        src/tui/terminal_ui_render.cpp
        src/tui/terminal_ui_runtime.cpp
        src/tui/terminal_ui.cpp
    )

    add_library(zkvault_legacy_core
        ${ZKVAULT_LEGACY_CORE_SOURCES}
    )

    target_include_directories(zkvault_legacy_core PUBLIC
        src
    )

    target_link_libraries(zkvault_legacy_core PUBLIC zkvault_core)

    add_library(zkvault_legacy_service
        ${ZKVAULT_LEGACY_SERVICE_SOURCES}
    )

    target_link_libraries(zkvault_legacy_service PUBLIC zkvault_legacy_core)

    add_library(zkvault_legacy_terminal_frontends
        ${ZKVAULT_LEGACY_TERMINAL_FRONTEND_SOURCES}
    )

    target_include_directories(zkvault_legacy_terminal_frontends PUBLIC
        src
    )

    target_link_libraries(zkvault_legacy_terminal_frontends PUBLIC
        zkvault_legacy_service
        zkvault_terminal_support
    )

    add_executable(zkvault_legacy
        src/main.cpp
    )

    set_target_properties(zkvault_legacy PROPERTIES
        OUTPUT_NAME "zkvault-legacy"
    )

    target_link_libraries(zkvault_legacy PRIVATE
        zkvault_core
        zkvault_legacy_core
        zkvault_legacy_terminal_frontends
        zkvault_terminal_support
    )
endif()

if(BUILD_TESTING)
    add_executable(zkvault_private_post
        tests/private_post_test.cpp
    )

    target_link_libraries(zkvault_private_post PRIVATE zkvault_core)

    add_test(
        NAME zkvault_private_post
        COMMAND $<TARGET_FILE:zkvault_private_post>
    )

    if(ZKVAULT_BUILD_DEV_TOOLS)
        add_test(
            NAME zkvault_private_post_cli_smoke
            COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/private_post_cli_smoke_test.sh $<TARGET_FILE:zkvault>
        )
    endif()

    if(ZKVAULT_BUILD_LEGACY)
        add_executable(zkvault_app_contract
            tests/app_contract_test.cpp
        )

        add_executable(zkvault_prompt_backspace
            tests/prompt_backspace_test.cpp
            src/terminal/prompt.cpp
        )

        add_executable(zkvault_terminal_display
            tests/terminal_display_test.cpp
            src/terminal/display.cpp
        )

        add_executable(zkvault_shell_idle_lock
            tests/shell_idle_lock_test.cpp
        )

        add_executable(zkvault_tui_smoke
            tests/tui_smoke_test.cpp
        )

        add_executable(zkvault_frontend_contract
            tests/frontend_contract_test.cpp
        )

        target_link_libraries(zkvault_app_contract PRIVATE zkvault_legacy_core)
        target_link_libraries(zkvault_frontend_contract PRIVATE zkvault_legacy_core)
        target_link_libraries(zkvault_prompt_backspace PRIVATE zkvault_core util)
        target_link_libraries(zkvault_shell_idle_lock PRIVATE util)
        target_link_libraries(zkvault_tui_smoke PRIVATE util)
        target_link_libraries(zkvault_terminal_display PRIVATE util)
        target_include_directories(zkvault_terminal_display PRIVATE
            src
        )

        add_test(
            NAME zkvault_cli_smoke
            COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_smoke_test.sh $<TARGET_FILE:zkvault_legacy>
        )
        add_test(
            NAME zkvault_cli_error_paths
            COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_error_paths_test.sh $<TARGET_FILE:zkvault_legacy>
        )
        add_test(
            NAME zkvault_shell_smoke
            COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/shell_smoke_test.sh $<TARGET_FILE:zkvault_legacy>
        )
        add_test(
            NAME zkvault_app_contract
            COMMAND $<TARGET_FILE:zkvault_app_contract>
        )
        add_test(
            NAME zkvault_prompt_backspace
            COMMAND $<TARGET_FILE:zkvault_prompt_backspace>
        )
        add_test(
            NAME zkvault_terminal_display
            COMMAND $<TARGET_FILE:zkvault_terminal_display>
        )
        add_test(
            NAME zkvault_shell_idle_lock
            COMMAND $<TARGET_FILE:zkvault_shell_idle_lock> $<TARGET_FILE:zkvault_legacy>
        )
        add_test(
            NAME zkvault_tui_smoke
            COMMAND $<TARGET_FILE:zkvault_tui_smoke> $<TARGET_FILE:zkvault_legacy>
        )
        add_test(
            NAME zkvault_frontend_contract
            COMMAND $<TARGET_FILE:zkvault_frontend_contract>
        )
    endif()
endif()
