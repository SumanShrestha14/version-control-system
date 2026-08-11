cmake_minimum_required(VERSION 3.20)
project(synk CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # handy for editor/clangd integration

# --- Dependencies -----------------------------------------------------------
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(CURL REQUIRED)

# --- Source files ------------------------------------------------------------
# Explicit list on purpose: with GLOB, CMake still only re-scans on
# reconfigure (cmake -B build), so it buys nothing but hides which files
# are actually part of the build. When you add a .cpp, add it here too,
# AND re-run `cmake -B build` -- `cmake --build build` alone will not
# pick up either change.

set(SYNK_SOURCES
    src/main.cpp

    # core/
    src/core/object_id.cpp
    src/core/git_object.cpp
    src/core/blob.cpp
    src/core/tree_entry.cpp
    src/core/tree.cpp
    src/core/commit.cpp
    src/core/git_timestamp.cpp

    # storage/
    src/storage/compressor.cpp
    src/storage/object_store.cpp

    # repository/
    src/repository/ref_store.cpp
    src/repository/repository.cpp

    # workdir/
    src/workdir/tree_builder.cpp
    src/workdir/checkout.cpp

    # net/
    src/net/http_client.cpp
    src/net/pktline_codec.cpp
    src/net/packfile_parser.cpp
    src/net/delta_resolver.cpp
    src/net/clone_operation.cpp

    # cli/
    src/cli/command_parser.cpp
    src/cli/init_command.cpp
    src/cli/cat_file_command.cpp
    src/cli/hash_object_command.cpp
    src/cli/ls_tree_command.cpp
    src/cli/write_tree_command.cpp
    src/cli/commit_tree_command.cpp
    src/cli/clone_command.cpp
)

add_executable(synk ${SYNK_SOURCES})

target_include_directories(synk PRIVATE src)

target_link_libraries(synk PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    CURL::libcurl
)

# --- Warnings ------------------------------------------------------------
# Worth having on for an OOP-heavy refactor: -Woverloaded-virtual catches
# accidentally hiding a base class method instead of overriding it (an easy
# mistake with a Command hierarchy), and -Wnon-virtual-dtor would have
# caught it if GitObject's destructor weren't already virtual.
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(synk PRIVATE -Wall -Wextra -Woverloaded-virtual -Wnon-virtual-dtor)
endif()
