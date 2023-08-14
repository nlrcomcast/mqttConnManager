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

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -W -g -fprofile-arcs -ftest-coverage -O0")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DTEST ")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -W  -g -fprofile-arcs -ftest-coverage -O0")
set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage -O0")

if(NOT DISABLE_VALGRIND)
set (MEMORY_CHECK valgrind --leak-check=full --show-reachable=yes --track-origins=yes -v)
endif ()

link_directories ( ${LIBRARY_DIR} )

#-------------------------------------------------------------------------------
#   test_mqttcm_connect
#-------------------------------------------------------------------------------
add_test(NAME test_mqttcm_connect COMMAND ${MEMORY_CHECK} ./test_mqttcm_connect)
set(SOURCES test_mqttcm_connect.c ../src/mqttcm_connect.c ../src/mqttcm_generic.c ../src/mqttcm_privilege.c ../src/mqttcm_webcfg.c )
add_executable(test_mqttcm_connect ${SOURCES})
target_link_libraries (test_mqttcm_connect -lcunit -lmsgpackc -lcimplog -lrbus -lpthread -lmosquitto -luuid -lcjson)
target_link_libraries (test_mqttcm_connect gcov -Wl,--no-as-needed )


#-------------------------------------------------------------------------------
#   test_mqttcm_component
#-------------------------------------------------------------------------------

add_test(NAME test_mqttcm_component COMMAND ${MEMORY_CHECK} ./test_mqttcm_component)
set(SOURCES test_mqttcm_component.c ../src/mqttcm_connect.c ../src/mqttcm_generic.c ../src/mqttcm_privilege.c ../src/mqttcm_webcfg.c )
add_executable(test_mqttcm_component ${SOURCES})
target_link_libraries (test_mqttcm_component -lcunit -lmsgpackc -lcimplog -lrbus -lpthread -lmosquitto -luuid -lcjson)

target_link_libraries (test_mqttcm_component gcov -Wl,--no-as-needed )


#-------------------------------------------------------------------------------
#   test_mqttcm_timer
#-------------------------------------------------------------------------------

add_test(NAME test_mqttcm_timer COMMAND ${MEMORY_CHECK} ./test_mqttcm_timer)
set(SOURCES test_mqttcm_timer.c ../src/mqttcm_connect.c ../src/mqttcm_generic.c ../src/mqttcm_privilege.c ../src/mqttcm_webcfg.c )
add_executable(test_mqttcm_timer ${SOURCES})
target_link_libraries (test_mqttcm_timer -lcunit -lmsgpackc -lcimplog -lrbus -lpthread -lmosquitto -luuid -lcjson)

target_link_libraries (test_mqttcm_timer gcov -Wl,--no-as-needed )


# Code coverage

add_custom_target(coverage
COMMAND lcov -q --capture --directory
${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_mqttcm_connect.dir/__/src --output-file test_mqttcm_connect.info
COMMAND lcov -q --capture --directory
${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_mqttcm_component.dir/__/src --output-file test_mqttcm_component.info
COMMAND lcov -q --capture --directory
${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_mqttcm_timer.dir/__/src --output-file test_mqttcm_timer.info


COMMAND lcov
-a test_mqttcm_connect.info
-a test_mqttcm_component.info
-a test_mqttcm_timer.info

COMMAND genhtml coverage.info
WORKING_DIRECTORY ${CMAKE_BINARY_DIR})