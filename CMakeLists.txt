cmake_minimum_required (VERSION 3.12)

########################################
# Project versioning
########################################

set(OAPV_H_HEADER_FILE "${CMAKE_SOURCE_DIR}/inc/oapv.h")

if(NOT EXISTS ${OAPV_H_HEADER_FILE})
  message(FATAL_ERROR
      "${CMAKE_SOURCE_DIR}/inc/oapv.h file doesn't exist!\n"
      "File contains OAPV_VER_APISET, OAPV_VER_MAJOR, OAPV_VER_MINOR, OAPV_VER_PATCH definitions that specifie openAPV project version.\n"
      "To get the information on version of the downloaded library please follow the link below:\n\t https://github.com/AcademySoftwareFoundation/openapv"
      )
endif()

file(READ ${OAPV_H_HEADER_FILE} HEADER_CONTENT)

# Regular expressions for extracting versions
string(REGEX MATCH "(#define)([ ]+)(OAPV_VER_APISET)([ ]+)(\\()([ ]*)([0-9]+)([ ]*)(\\))" APISET_MATCH ${HEADER_CONTENT})
string(REGEX MATCH "(#define)([ ]+)(OAPV_VER_MAJOR)([ ]+)(\\()([ ]*)([0-9]+)([ ]*)(\\))" MAJOR_MATCH ${HEADER_CONTENT})
string(REGEX MATCH "(#define)([ ]+)(OAPV_VER_MINOR)([ ]+)(\\()([ ]*)([0-9]+)([ ]*)(\\))" MINOR_MATCH ${HEADER_CONTENT})
string(REGEX MATCH "(#define)([ ]+)(OAPV_VER_PATCH)([ ]+)(\\()([ ]*)([0-9]+)([ ]*)(\\))" PATCH_MATCH ${HEADER_CONTENT})

# Extract the values OAPV_VER_APISET, OAPV_VER_MAJOR, OAPV_VER_MINOR, OAPV_VER_PATCH from the oapv.h file
if(APISET_MATCH)
  string(REGEX REPLACE "(#define)([ ]+)(OAPV_VER_APISET)([ ]+)(\\()([ ]*)([0-9]+)([ ]*)(\\))" "\\7" VERSION_APISET ${APISET_MATCH})
else()
  message(WARRNING "In the file oapv.h, no matching string was found for the regular expression (regex) used to match the pattern containing #define OAPV_VER_APISET (number).")
endif()

if(MAJOR_MATCH)
  string(REGEX REPLACE "(#define)([ ]+)(OAPV_VER_MAJOR)([ ]+)(\\()([ ]*)([0-9]+)([ ]*)(\\))" "\\7" VERSION_MAJOR ${MAJOR_MATCH})
else()
  message(WARRNING "In the file oapv.h, no matching string was found for the regular expression (regex) used to match the pattern containing #define OAPV_VER_MAJOR (number).")
endif()

if(MINOR_MATCH)
    string(REGEX REPLACE "(#define)([ ]+)(OAPV_VER_MINOR)([ ]+)(\\()([ ]*)([0-9]+)([ ]*)(\\))" "\\7" VERSION_MINOR ${MINOR_MATCH})
else()
  message(WARRNING "In the file oapv.h, no matching string was found for the regular expression (regex) used to match the pattern containing #define OAPV_VER_MINOR (number).")
endif()

if(PATCH_MATCH)
    string(REGEX REPLACE "(#define)([ ]+)(OAPV_VER_PATCH)([ ]+)(\\()([ ]*)([0-9]+)([ ]*)(\\))" "\\7" VERSION_PATCH ${PATCH_MATCH})
else()
  message(WARRNING "In the file oapv.h, no matching string was found for the regular expression (regex) used to match the pattern containing #define OAPV_VER_PATCH (number).")
endif()

message(DEBUG "OAPV_VER_APISET: ${VERSION_APISET}")
message(DEBUG "OAPV_VER_MAJOR: ${VERSION_MAJOR}")
message(DEBUG "OAPV_VER_MINOR: ${VERSION_MINOR}")
message(DEBUG "OAPV_VER_PATCH: ${VERSION_PATCH}")

message(STATUS "OAPV version has been taken from ${OAPV_H_HEADER_FILE} file.")
message(STATUS "OAPV version: ${VERSION_APISET}.${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

project(OAPV VERSION ${VERSION_APISET}.${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH} LANGUAGES C)

########################################
# Input arguments.
########################################

# Check input arguments.
option(OAPV_APP_STATIC_BUILD "oapv_app will be statically linked against static oapv library" ON)

option(OAPV_BUILD_APPS "Build the included command line apps" ON)
option(OAPV_BUILD_STATIC_LIB "Build oapv static library" ON)
option(OAPV_BUILD_SHARED_LIB "Build oapv shared library" ON)

option(ENABLE_COVERAGE "Enable code coverage measurement" OFF)

if(OAPV_BUILD_APPS AND OAPV_APP_STATIC_BUILD AND NOT OAPV_BUILD_STATIC_LIB)
  message(FATAL_ERROR "Cannot build static apps without static lib.")
endif()

if(OAPV_BUILD_APPS AND NOT OAPV_APP_STATIC_BUILD AND NOT OAPV_BUILD_SHARED_LIB)
  message(FATAL_ERROR "Cannot build shared apps without shared lib.")
endif()

if(NOT OAPV_BUILD_STATIC_LIB AND NOT OAPV_BUILD_SHARED_LIB)
  message(FATAL_ERROR "At least one of static or shared lib build needs to be enabled.")
endif()

# To build for arm provide in command line: -DARM=TRUE
if(NOT ARM)
  set(ARM "FALSE")
else()
  add_definitions(-DARM=1)
  set(ARM "TRUE")
endif()

# To build for universal architectures provide in command line: -DUNIVERSAL=TRUE
if(NOT UNIVERSAL)
  set(UNIVERSAL "FALSE")
else()
  add_definitions(-DUNIVERSAL=1)
  set(UNIVERSAL "TRUE")
endif()

########################################
# Compilation flags
########################################

# Set compiler flags and options.
if(MSVC)
  message("Not supported yet!")
elseif(UNIX OR MINGW)
    if(ENABLE_COVERAGE)
        set(CMAKE_BUILD_TYPE "Debug")
    endif()
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release")
    endif()
    message("CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")

    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        set(OPT_LV "O0")
        set(OPT_DBG "-g")
    else()
        set(OPT_LV "O3")
        set(OPT_DBG "-DNDEBUG") # disable assert
    endif()

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OPT_DBG} -${OPT_LV} -fomit-frame-pointer -pthread -std=c99")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unused-function")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lm")

    if(ENABLE_COVERAGE)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    endif()
else()
    message("Unknown compiler")
endif()

# Command to output information to the console
message ("c Flags: " ${CMAKE_C_FLAGS})
message ("linker Flags: " ${CMAKE_EXE_LINKER_FLAGS})

########################################
# Configuration
########################################

set(CMAKE_C_STANDARD 99)
cmake_policy(SET CMP0048 NEW)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Sub-directories where more CMakeLists.txt exist
add_subdirectory(src)

if(OAPV_BUILD_APPS)
  add_subdirectory(app)
endif()

########################################
# Targets
########################################

# uninstall target
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()

########################################
# CPack project packaging
########################################
# Check the operating system
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Linux system")
    # Read the /etc/os-release file to determine the distribution
    file(READ "/etc/os-release" OS_RELEASE_CONTENT)

    if(OS_RELEASE_CONTENT MATCHES "ID=debian" OR OS_RELEASE_CONTENT MATCHES "ID=ubuntu")
        message(STATUS "Debian-based system detected")
        message(STATUS "Use DEB generator while generating installation package using CPack")
        set(CPACK_GENERATOR "DEB")
    elseif(OS_RELEASE_CONTENT MATCHES "ID=rhel" OR OS_RELEASE_CONTENT MATCHES "ID=fedora" OR OS_RELEASE_CONTENT MATCHES "ID=centos")
        message(STATUS "Red Hat-based system detected")
        message(STATUS "Use RPM generator while generating installation package using CPack")
        set(CPACK_GENERATOR "RPM")
    elseif(OS_RELEASE_CONTENT MATCHES "ID=opensuse")
        message(STATUS "SUSE-based system detected")
        message(STATUS "Use RPM generator while generating installation package using CPack")
        set(CPACK_GENERATOR "RPM")
    else()
        message(STATUS "Other Linux distribution detected")
        message(STATUS "Use TGZ generator while generating installation package using CPack")
        set(CPACK_GENERATOR "TGZ")
    endif()

elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Windows system")
    
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        # Check if the compiler path contains 'ucrt64'
        if(CMAKE_C_COMPILER MATCHES "ucrt64")
            message(STATUS "UCRT64 environment detected")
            message(STATUS "Use NSIS generator while generating installation package using CPack")
            set(CPACK_GENERATOR "NSIS")
        else()
            message(STATUS "Not using UCRT64 compiler. Compiler ID: ${CMAKE_C_COMPILER}")
            message(STATUS "Use TGZ generator while generating installation package using CPack")
            set(CPACK_GENERATOR "TGZ")
        endif()
    # Check if the compiler is MSVC
    elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        message(STATUS "Using Microsoft Visual Studio (MSVC) compiler")
        message(STATUS "Use NSIS generator while generating installation package using CPack")
        set(CPACK_GENERATOR "NSIS")
    else()
        message(STATUS "Not using MSVC compiler. Compiler ID: ${CMAKE_C_COMPILER_ID}.")
        message(STATUS "Use ZIP generator while generating installation package using CPack")
        set(CPACK_GENERATOR "ZIP")
    endif()
else()
    message(STATUS "Other OS: ${CMAKE_SYSTEM_NAME}")
    message(STATUS "Use ZIP generator while generating installation package using CPack")
    set(CPACK_GENERATOR "ZIP")
endif()

# Packaging
include(InstallRequiredSystemLibraries)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

set(CPACK_PACKAGE_NAME "OpenAPV")
set(CPACK_PACKAGE_VENDOR "AcademySoftwareFoundation")
set(CPACK_PACKAGE_CONTACT "https://github.com/AcademySoftwareFoundation")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/AcademySoftwareFoundation/openapv/releases")
set(CMAKE_PROJECT_HOMEPAGE_URL "https://github.com/AcademySoftwareFoundation/openapv")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Open Advanced Professional Video Codec")
set(CPACK_PACKAGE_VERSION "${VERSION_APISET}.${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
set(CPACK_PACKAGE_CHECKSUM MD5)

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "AcademySoftwareFoundation")
set(CPACK_DEBIAN_PACKAGE_SECTION "video")
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")

include(CPack)

########################################
# Testing
########################################

option(ENABLE_TESTS "Enable tests" ON)
if (${ENABLE_TESTS})
    enable_testing()
endif()

# Test - Check if encoder starts
add_test(NAME Encoder_runs COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bin/oapv_app_enc)

# Test - Check if decoder starts
add_test(NAME Decoder_runs COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bin/oapv_app_dec)

# Test - encode
add_test(NAME encode COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bin/oapv_app_enc -i ${CMAKE_CURRENT_SOURCE_DIR}/test/sequence/pattern1_yuv422p10le_320x240_25fps.y4m -w 320 -h 240 -z 25 -o out.oapv)
set_tests_properties(encode PROPERTIES
    TIMEOUT 20
    FAIL_REGULAR_EXPRESSION "Encoded frame count               = 0"
    PASS_REGULAR_EXPRESSION "Encoded frame count               = 125"
    RUN_SERIAL TRUE
)

# Test - decode
add_test(NAME decode COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bin/oapv_app_dec -i out.oapv)
set_tests_properties(decode PROPERTIES
    TIMEOUT 10
    DEPENDS encode
    FAIL_REGULAR_EXPRESSION "Decoded frame count               = 0"
    PASS_REGULAR_EXPRESSION "Decoded frame count               = 125"
    RUN_SERIAL TRUE
)

# Test - decode qp_A.apv
add_test(NAME decode_qp_A COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bin/oapv_app_dec -i ${CMAKE_CURRENT_SOURCE_DIR}/test/bitstream/qp_A.apv -v 3 --hash)
set_tests_properties(decode_qp_A PROPERTIES
    TIMEOUT 10
    FAIL_REGULAR_EXPRESSION "Decoded frame count               = 0"
    PASS_REGULAR_EXPRESSION "Decoded frame count               = 3"
    PASS_REGULAR_EXPRESSION ".*hash:match.*"
)

# Test - decode qp_B.apv
add_test(NAME decode_qp_B COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bin/oapv_app_dec -i ${CMAKE_CURRENT_SOURCE_DIR}/test/bitstream/qp_B.apv -v 3 --hash)
set_tests_properties(decode_qp_B PROPERTIES
    TIMEOUT 10
    FAIL_REGULAR_EXPRESSION "Decoded frame count               = 0"
    PASS_REGULAR_EXPRESSION "Decoded frame count               = 3"
    PASS_REGULAR_EXPRESSION ".*hash:match.*"
)

# Test - decode qp_C.apv
add_test(NAME decode_qp_C COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bin/oapv_app_dec -i ${CMAKE_CURRENT_SOURCE_DIR}/test/bitstream/qp_C.apv -v 3 --hash)
set_tests_properties(decode_qp_C PROPERTIES
    TIMEOUT 10
    FAIL_REGULAR_EXPRESSION "Decoded frame count               = 0"
    PASS_REGULAR_EXPRESSION "Decoded frame count               = 3"
    PASS_REGULAR_EXPRESSION ".*hash:match.*"
)

# Test - decode qp_D.apv
add_test(NAME decode_qp_D COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bin/oapv_app_dec -i ${CMAKE_CURRENT_SOURCE_DIR}/test/bitstream/qp_D.apv -v 3 --hash)
set_tests_properties(decode_qp_D PROPERTIES
    TIMEOUT 10
    FAIL_REGULAR_EXPRESSION "Decoded frame count               = 0"
    PASS_REGULAR_EXPRESSION "Decoded frame count               = 3"
    PASS_REGULAR_EXPRESSION ".*hash:match.*"
)

# Test - decode qp_E.apv
add_test(NAME decode_qp_E COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bin/oapv_app_dec -i ${CMAKE_CURRENT_SOURCE_DIR}/test/bitstream/qp_E.apv -v 3 --hash)
set_tests_properties(decode_qp_E PROPERTIES
    TIMEOUT 10
    FAIL_REGULAR_EXPRESSION "Decoded frame count               = 0"
    PASS_REGULAR_EXPRESSION "Decoded frame count               = 3"
    PASS_REGULAR_EXPRESSION ".*hash:match.*"
)

# Test - decode syn_A.apv
add_test(NAME decode_syn_A COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bin/oapv_app_dec -i ${CMAKE_CURRENT_SOURCE_DIR}/test/bitstream/syn_A.apv -v 3 --hash)
set_tests_properties(decode_syn_A PROPERTIES
    TIMEOUT 10
    FAIL_REGULAR_EXPRESSION "Decoded frame count               = 0"
    PASS_REGULAR_EXPRESSION "Decoded frame count               = 3"
    PASS_REGULAR_EXPRESSION ".*hash:match.*"
)

# Test - decode syn_B.apv
add_test(NAME decode_syn_B COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bin/oapv_app_dec -i ${CMAKE_CURRENT_SOURCE_DIR}/test/bitstream/syn_B.apv -v 3 --hash)
set_tests_properties(decode_syn_B PROPERTIES
    TIMEOUT 10
    FAIL_REGULAR_EXPRESSION "Decoded frame count               = 0"
    PASS_REGULAR_EXPRESSION "Decoded frame count               = 3"
    PASS_REGULAR_EXPRESSION ".*hash:match.*"
)

# Test - decode tile_A.apv
add_test(NAME decode_tile_A COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bin/oapv_app_dec -i ${CMAKE_CURRENT_SOURCE_DIR}/test/bitstream/tile_A.apv -v 3 --hash)
set_tests_properties(decode_tile_A PROPERTIES
    TIMEOUT 10
    FAIL_REGULAR_EXPRESSION "Decoded frame count               = 0"
    PASS_REGULAR_EXPRESSION "Decoded frame count               = 3"
    PASS_REGULAR_EXPRESSION ".*hash:match.*"
)

# Test - decode tile_B.apv
add_test(NAME decode_tile_B COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bin/oapv_app_dec -i ${CMAKE_CURRENT_SOURCE_DIR}/test/bitstream/tile_B.apv -v 3 --hash)
set_tests_properties(decode_tile_B PROPERTIES
    TIMEOUT 10
    FAIL_REGULAR_EXPRESSION "Decoded frame count               = 0"
    PASS_REGULAR_EXPRESSION "Decoded frame count               = 3"
    PASS_REGULAR_EXPRESSION ".*hash:match.*"
)

# Test - decode tile_C.apv
add_test(NAME decode_tile_C COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bin/oapv_app_dec -i ${CMAKE_CURRENT_SOURCE_DIR}/test/bitstream/tile_C.apv -v 3 --hash)
set_tests_properties(decode_tile_C PROPERTIES
    TIMEOUT 10
    FAIL_REGULAR_EXPRESSION "Decoded frame count               = 0"
    PASS_REGULAR_EXPRESSION "Decoded frame count               = 3"
    PASS_REGULAR_EXPRESSION ".*hash:match.*"
)

# Test - decode tile_D.apv
add_test(NAME decode_tile_D COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bin/oapv_app_dec -i ${CMAKE_CURRENT_SOURCE_DIR}/test/bitstream/tile_D.apv -v 3 --hash)
set_tests_properties(decode_tile_D PROPERTIES
    TIMEOUT 10
    FAIL_REGULAR_EXPRESSION "Decoded frame count               = 0"
    PASS_REGULAR_EXPRESSION "Decoded frame count               = 3"
    PASS_REGULAR_EXPRESSION ".*hash:match.*"
)

# Test - decode tile_E.apv
add_test(NAME decode_tile_E COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bin/oapv_app_dec -i ${CMAKE_CURRENT_SOURCE_DIR}/test/bitstream/tile_E.apv -v 3 --hash)
set_tests_properties(decode_tile_E PROPERTIES
    TIMEOUT 10
    FAIL_REGULAR_EXPRESSION "Decoded frame count               = 0"
    PASS_REGULAR_EXPRESSION "Decoded frame count               = 3"
    PASS_REGULAR_EXPRESSION ".*hash:match.*"
)
