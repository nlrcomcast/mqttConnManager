#   Copyright 2023 Comcast Cable Communications Management, LLC
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

cmake_minimum_required(VERSION 2.8.7)

project(mqttconmgr)

include(ExternalProject)
include(CTest)

add_definitions(-std=c99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -g -Werror -Wall -Wno-sizeof-pointer-memaccess -Wno-format-truncation -Wno-unused-parameter -D_GNU_SOURCE=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c99 -g -Werror -Wall -Wno-sizeof-pointer-memaccess -Wno-format-truncation -Wno-unused-parameter -D_GNU_SOURCE=1")

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
set(CMAKE_MACOSX_RPATH 1)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -undefined dynamic_lookup")
endif()

if (INCLUDE_BREAKPAD)
add_definitions(-DINCLUDE_BREAKPAD)
endif (INCLUDE_BREAKPAD)

set(INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/_install)
set(PREFIX_DIR ${CMAKE_CURRENT_BINARY_DIR}/_prefix)
set(INCLUDE_DIR ${INSTALL_DIR}/include)
set(LIBRARY_DIR ${INSTALL_DIR}/lib)
set(LIBRARY_DIR64 ${INSTALL_DIR}/lib64)
set(COMMON_LIBRARY_DIR ${INSTALL_DIR}/lib/${CMAKE_LIBRARY_ARCHITECTURE})
set(TEST_RESULTS_DIR ${CMAKE_CURRENT_BINARY_DIR}/test_results)
file(MAKE_DIRECTORY ${TEST_RESULTS_DIR})

include_directories(${INCLUDE_DIR}
                    ${INCLUDE_DIR}/cjson
                    ${INCLUDE_DIR}/mosquitto
                    ${INCLUDE_DIR}/rbus
		    ${INCLUDE_DIR}/rtmessage
		    ${INCLUDE_DIR}/cimplog
                    )


if (NOT BUILD_YOCTO)

# mosquitto external dependency
#-------------------------------------------------------------------------------
ExternalProject_Add(mosquitto
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/_prefix/mosquitto
    GIT_REPOSITORY https://github.com/eclipse/mosquitto.git
    GIT_TAG "master"
    CMAKE_ARGS += -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} -DWITH_CJSON=no -DWITH_BROKER=ON
)
add_library(libmosquitto STATIC SHARED IMPORTED)
add_dependencies(libmosquitto mosquitto)

# cimplog external dependency
#-------------------------------------------------------------------------------
ExternalProject_Add(cimplog
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/_prefix/cimplog
    GIT_REPOSITORY https://github.com/Comcast/cimplog.git
    GIT_TAG "1.0.2"
    CMAKE_ARGS += -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}
)
add_library(libcimplog STATIC SHARED IMPORTED)
add_dependencies(libcimplog cimplog)

# cJSON external dependency
#-------------------------------------------------------------------------------
ExternalProject_Add(cJSON
    PREFIX ${PREFIX_DIR}/cJSON
    GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
    GIT_TAG "39853e5148dad8dc5d32ea2b00943cf4a0c6f120"
    CMAKE_ARGS += -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}
)
add_library(libcJSON STATIC IMPORTED)
add_dependencies(libcJSON cJSON)

# rbus external dependency
#-------------------------------------------------------------------------------
ExternalProject_Add(rbus
     PREFIX ${CMAKE_CURRENT_BINARY_DIR}/_prefix/rbus
     GIT_REPOSITORY https://github.com/rdkcentral/rbus.git
     GIT_TAG main
     CMAKE_ARGS += -DBUILD_FOR_DESKTOP=ON -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} -DBUILD_TESTING=OFF
)

add_library(librbuscore STATIC SHARED IMPORTED)
add_dependencies(librbuscore rbuscore)

add_library(librtMessage STATIC SHARED IMPORTED)
add_dependencies(librtMessage rtMessage)

add_library(librbus STATIC SHARED IMPORTED)
add_dependencies(librbus rbus)

endif ()
link_directories ( ${LIBRARY_DIR} ${COMMON_LIBRARY_DIR} ${LIBRARY_DIR64} )


add_subdirectory(src)
add_subdirectory(lib)

if (BUILD_TESTING)
	add_subdirectory(tests)
endif (BUILD_TESTING)
