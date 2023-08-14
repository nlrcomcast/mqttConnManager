/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MQTTCM_CONN_H_INCLUDED
#define MQTTCM_CONN_H_INCLUDED

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "mqttcm_log.h"

#define MQTT_PUBLISH_PARAM        "Device.X_RDK_MQTT.Publish"
#define MQTT_CONNSTATUS_PARAM     "Device.X_RDK_MQTT.ConnectionStatus"

int mqttcm_conn_msg_process(void *msg, long mesg_len, bool do_compress, char *topic, char *qos);
int mqttcm_conn_init();
int mqttcm_conn_finish();

#endif /* MQTTCM_CONN_H_INCLUDED */
