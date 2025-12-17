cmake_minimum_required(VERSION 3.22)

project(stump C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Defaulting to MinSizeRel build for the smallest binary size.")
    set(CMAKE_BUILD_TYPE MinSizeRel CACHE STRING "Choose the type of build." FORCE)
endif()

set(MINSIZE_C_FLAGS "-Os -ffunction-sections -fdata-sections -fomit-frame-pointer -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-stack-protector -fno-stack-clash-protection -fcf-protection=none -fno-plt -fno-pic -fno-PIE -fno-pie")
set(MINSIZE_CXX_FLAGS "${MINSIZE_C_FLAGS} -fvisibility=hidden")
set(MINSIZE_LINKER_FLAGS "-fuse-ld=lld -s -Wl,--gc-sections -Wl,--as-needed -Wl,-O1 -Wl,-z,norelro -Wl,-z,lazy -Wl,-z,noseparate-code -Wl,--icf=all -no-pie")

set(CMAKE_C_FLAGS_MINSIZEREL "${MINSIZE_C_FLAGS}" CACHE STRING "C flags for MinSizeRel." FORCE)
set(CMAKE_CXX_FLAGS_MINSIZEREL "${MINSIZE_CXX_FLAGS}" CACHE STRING "CXX flags for MinSizeRel." FORCE)
set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${MINSIZE_LINKER_FLAGS}" CACHE STRING "Linker flags for MinSizeRel executables." FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL "${MINSIZE_LINKER_FLAGS}" CACHE STRING "Linker flags for MinSizeRel shared libraries." FORCE)

set(APPEND_CFLAGS "${MINSIZE_C_FLAGS}" CACHE STRING "C flags appended for downstream builds." FORCE)
set(APPEND_CXXFLAGS "${MINSIZE_CXX_FLAGS}" CACHE STRING "C++ flags appended for downstream builds." FORCE)
set(APPEND_LDFLAGS "${MINSIZE_LINKER_FLAGS}" CACHE STRING "Linker flags appended for downstream builds." FORCE)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

set(BITCOIN_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../bitcoin")
if(NOT EXISTS "${BITCOIN_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Unable to locate bitcoin sources at ${BITCOIN_SOURCE_DIR}")
endif()

# Configure the Bitcoin Core build to only produce the bitcoinkernel library.
set(BUILD_BITCOIN_BIN OFF CACHE BOOL "Build bitcoin executable." FORCE)
set(BUILD_DAEMON OFF CACHE BOOL "Build bitcoind executable." FORCE)
set(BUILD_GUI OFF CACHE BOOL "Build bitcoin-qt executable." FORCE)
set(BUILD_CLI OFF CACHE BOOL "Build bitcoin-cli executable." FORCE)
set(BUILD_TESTS OFF CACHE BOOL "Build test executables." FORCE)
set(BUILD_TX OFF CACHE BOOL "Build bitcoin-tx executable." FORCE)
set(BUILD_UTIL OFF CACHE BOOL "Build bitcoin-util executable." FORCE)
set(BUILD_UTIL_CHAINSTATE OFF CACHE BOOL "Build experimental bitcoin-chainstate executable." FORCE)
set(BUILD_BENCH OFF CACHE BOOL "Build bench_bitcoin executable." FORCE)
set(BUILD_FUZZ_BINARY OFF CACHE BOOL "Build fuzz binary." FORCE)
set(BUILD_FOR_FUZZING OFF CACHE BOOL "Build for fuzzing." FORCE)
set(BUILD_GUI_TESTS OFF CACHE BOOL "Build Qt tests." FORCE)
set(BUILD_KERNEL_LIB ON CACHE BOOL "Build experimental bitcoinkernel library." FORCE)
set(BUILD_KERNEL_TEST OFF CACHE BOOL "Build tests for bitcoinkernel library." FORCE)
set(WITH_CCACHE OFF CACHE BOOL "Attempt to use ccache for compiling." FORCE)
set(ENABLE_WALLET OFF CACHE BOOL "Enable wallet support." FORCE)
set(ENABLE_EXTERNAL_SIGNER OFF CACHE BOOL "Enable external signer support." FORCE)
set(ENABLE_IPC OFF CACHE BOOL "Enable multiprocess binaries." FORCE)

add_subdirectory("${BITCOIN_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/bitcoin" EXCLUDE_FROM_ALL)

if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    set(COMPILE_COMMANDS_LINK "${CMAKE_SOURCE_DIR}/compile_commands.json")
    if(EXISTS "${COMPILE_COMMANDS_LINK}" OR IS_SYMLINK "${COMPILE_COMMANDS_LINK}")
        file(REMOVE "${COMPILE_COMMANDS_LINK}")
    endif()
    file(CREATE_LINK
        "${CMAKE_BINARY_DIR}/compile_commands.json"
        "${COMPILE_COMMANDS_LINK}"
        SYMBOLIC
    )
endif()

# Local executable
add_executable(stump
    src/main.c
    src/hex.c
    src/util.c
    src/stump.c
    src/varint.c
    src/utreexo_proof.c
    src/utreexo_proof_io.c
    src/utreexo_roots_io.c
    src/target_preimage.c
    src/utxo_extract.c
)

find_package(OpenSSL REQUIRED)

target_include_directories(stump PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"
    "${OPENSSL_INCLUDE_DIR}"
)

target_link_libraries(stump PRIVATE
    bitcoinkernel
    ${OPENSSL_LIBRARIES}
)

if(MSVC)
    target_compile_options(stump PRIVATE /Os)
    target_link_options(stump PRIVATE /OPT:REF /OPT:ICF)
else()
    target_compile_options(stump PRIVATE
        -Os
        -ffunction-sections
        -fdata-sections
        -fomit-frame-pointer
        -fno-asynchronous-unwind-tables
        -fno-unwind-tables
        -fno-stack-protector
        -fno-stack-clash-protection
        -fcf-protection=none
        -fno-plt
        -fno-pic
        -fno-PIE
        -fno-pie
    )
    target_link_options(stump PRIVATE
        -fuse-ld=lld
        -s
        -Wl,--gc-sections
        -Wl,-O1
        -Wl,--as-needed
        -Wl,-z,norelro
        -Wl,-z,lazy
        -Wl,-z,noseparate-code
        -Wl,--icf=all
        -no-pie
    )
endif()

set_property(TARGET stump PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
set_target_properties(stump PROPERTIES POSITION_INDEPENDENT_CODE OFF)
