cmake_minimum_required (VERSION 3.12)

########################################
# Project versioning
########################################

# Set project version from git tag or version.txt file
function(get_versions versionstring VERSION_MAJOR VERSION_MINOR VERSION_PATCH)
    string(REGEX REPLACE "^([vV])([0-9]*)([.][0-9]*[.][0-9]*-?.*)$" "\\2" numbers ${versionstring} )
    set(VERSION_MAJOR ${numbers} PARENT_SCOPE)
    string(REGEX REPLACE "^([vV][0-9]*[.])([0-9]*)([.][0-9]*-?.*)$" "\\2" numbers ${versionstring} )
    set(VERSION_MINOR ${numbers} PARENT_SCOPE)
    string(REGEX REPLACE "^([vV][0-9]*[.][0-9]*[.])([0-9]*)(-?.*)$" "\\2" numbers ${versionstring} )
    set(VERSION_PATCH ${numbers} PARENT_SCOPE)
endfunction()

execute_process(
    COMMAND git describe --match "v[0-9]*.[0-9]*.[0-9]*" --abbrev=0 --tags
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE RESULT
    OUTPUT_VARIABLE OUTPUT)

if(NOT RESULT EQUAL 0)
  find_program(GIT_COMMAND "git")
  if(GIT_COMMAND)
    message(WARNING "There is no valid git repository in your working directory \"${CMAKE_SOURCE_DIR}\" .")
  else()
    message(WARNING "You have no git tool installed.")
  endif()

  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/version.txt")
    message(FATAL_ERROR
        "version.txt file doesn't exist!\n"
        "Since your working directory doesn't contain a git repository you must provide \"${CMAKE_SOURCE_DIR}/version.txt\" file containing a valid version string.\n"
        "The version string provided to version.txt must match the following format:\n\tv[VERSION_MAJOR].[VERSION_MINOR].[VERSION_PATCH]\n"
        "To get the information on version of the downloaded library please follow the link below:\n\t https://github.com/AcademySoftwareFoundation/openapv"
        )
  endif()

  message("Version string has been taken from version.txt file.")
  file(STRINGS "version.txt" VERSION_STRING)

else()
  message("Version string has been taken from git tag.")
  set(VERSION_STRING ${OUTPUT})
  string(REGEX REPLACE "\n$" "" VERSION_STRING "${VERSION_STRING}")
endif()

if(NOT ${VERSION_STRING} MATCHES "^([vV])([0-9]*)([.][0-9]*[.][0-9]*-?.*)$")
  message(FATAL_ERROR "Version string ${VERSION_STRING} doesn't match required format v[VERSION_MAJOR].[VERSION_MINOR].[VERSION_PATCH]")
endif()

get_versions(${VERSION_STRING} VERSION_MAJOR VERSION_MINOR VERSION_PATCH)
if(VERSION_MAJOR STREQUAL ${VERSION_STRING})
  message(FATAL_ERROR "Version string parsing error")
endif()
message("OAPV VERSION=${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

project (OAPV VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH} LANGUAGES C)

########################################
# Input arguments.
########################################

# Check input arguments.
option(OAPV_APP_STATIC_BUILD "oapv_app will be statically linked against static oapv library" ON)
if(OAPV_APP_STATIC_BUILD)
    add_definitions(-DOAPV_STATIC_DEFINE)
endif(OAPV_APP_STATIC_BUILD)

# To build for arm provide in command line: -DARM=TRUE
if(NOT ARM)
  set(ARM "FALSE")
else()
  add_definitions(-DARM=1)
  set(ARM "TRUE")
endif()

########################################
# Compilation flags
########################################

# Set compiler flags and options.
if( MSVC )
  message("Not supported yet!")
elseif( UNIX OR MINGW )
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

    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OPT_DBG} -${OPT_LV} -fomit-frame-pointer -pthread -std=c99")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unused-function -Wno-pointer-sign -Wno-pointer-to-int-cast")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lm")
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
add_subdirectory(app)

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
set(CPACK_PACKAGE_VENDOR "OpenAPV")
set(CPACK_PACKAGE_CONTACT "https://github.com/openapv")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/openapv/oapv/releases")
set(CMAKE_PROJECT_HOMEPAGE_URL "https://github.com/openapv/oapv")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Open Advanced Professional Video Codec")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_CHECKSUM MD5)

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "OpenAPV")

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
