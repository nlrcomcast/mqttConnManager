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

set(PROJ_MQTTCMLIB mqttcm_lib)

get_filename_component(PARENT_DIR ../ ABSOLUTE)
include_directories(${PARENT_DIR}/src)

if (NOT DEVICE_EXTENDER)
set(COMMON_LIBS ${COMMON_LIBS} -lcimplog)
else ()
add_definitions(-DDEVICE_EXTENDER)
endif()

set(HEADERS mqttcm_conn.h ${PARENT_DIR}/src/mqttcm_log.h)
set(SOURCES mqttcm_conn.c)

add_library(${PROJ_MQTTCMLIB}.shared SHARED ${HEADERS} ${SOURCES})
set_target_properties(${PROJ_MQTTCMLIB}.shared PROPERTIES OUTPUT_NAME ${PROJ_MQTTCMLIB})
set_property(TARGET ${PROJ_MQTTCMLIB}.shared PROPERTY C_STANDARD 99)
set(COMMON_LIBS -lpthread -lrbus -lmosquitto -luuid -lz)

target_link_libraries (${PROJ_MQTTCMLIB}.shared ${CMAKE_THREAD_LIBS_INIT} ${COMMON_LIBS})
install (TARGETS ${PROJ_MQTTCMLIB}.shared DESTINATION lib${LIB_SUFFIX})
install (FILES ${HEADERS} DESTINATION include/${PROJ_MQTTCMLIB})
