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

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <ctype.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <mosquitto.h>
#include <mqtt_protocol.h>
#include <rbus/rbus.h>
#include <rbus/rbus_object.h>
#include <rbus/rbus_property.h>
#include <rbus/rbus_value.h>
#if ! defined(DEVICE_EXTENDER)
#include <cimplog.h>
#endif
#include <uuid/uuid.h>

#define MQTT_COMPONENT_NAME  "mqttConnManager"

#define MQTT_CONFIG_FILE     "/tmp/.mqttconfig"
#define MOSQ_TLS_VERSION     "tlsv1.2"
#define OPENSYNC_CERT        "/etc/mqttcm/mqtt_cert_init.sh"
#define KEEPALIVE            60
#define MQTT_PORT            443
#define MAX_MQTT_LEN         128
#define SINGLE_CONN_ELEMENTS 7
#define MAX_BUF_SIZE         255
#define maxParamLen          128
#define MQTT_SUBSCRIBER_FILE "/tmp/mqtt_subscriber_list"

#define MQTT_LOCATIONID_PARAM     "Device.X_RDK_MQTT.LocationID"
#define MQTT_BROKER_PARAM         "Device.X_RDK_MQTT.BrokerURL"
#define MQTT_CLIENTID_PARAM       "Device.X_RDK_MQTT.ClientID"
#define MQTT_PORT_PARAM           "Device.X_RDK_MQTT.Port"

#define MQTT_CONNECTMODE_PARAM    "Device.X_RDK_MQTT.ConnectionMode"
#define MQTT_CONNSTATUS_PARAM     "Device.X_RDK_MQTT.ConnectionStatus"
#define MQTT_SUBSCRIBE_PARAM      "Device.X_RDK_MQTT.Subscribe"
#define MQTT_PUBLISH_PARAM        "Device.X_RDK_MQTT.Publish"

#define MAX_MQTT_RETRY 8
#define MQTT_RETRY_ERR -1
#define MQTT_RETRY_SHUTDOWN 1
#define MQTT_DELAY_TAKEN 0

#define MQTTCM_FREE(__x__) if(__x__ != NULL) { free((void*)(__x__)); __x__ = NULL;} else {printf("Trying to free null pointer\n");}

#ifdef BUILD_YOCTO
#define DEVICE_PROPS_FILE       "/etc/device.properties"
#else
#define DEVICE_PROPS_FILE       "/tmp/device.properties"
#endif

typedef struct {
  struct timespec ts;
  int count;
  int max_count;
  int delay;
} mqtt_timer_t;

typedef struct comp_topic_name
{
	char compName[32];
	char topic[64];
	int subscribeOnFlag;
	int subscribeId;
	struct comp_topic_name *next;
} comp_topic_name_t;

int AddToSubscriptionList(char *compName, char *topic, int writeFlag);
const char *getComponentFromTopicName(char *topic);
void on_connect(struct mosquitto *mosq, void *obj, int reason_code, int flag, const mosquitto_property *props);
void on_disconnect(struct mosquitto *mosq, void *obj, int reason_code, const mosquitto_property *props);
void on_subscribe(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos, const mosquitto_property *props);
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg, const mosquitto_property *props);
void on_publish(struct mosquitto *mosq, void *obj, int mid, int reason_code, const mosquitto_property *props);
int isReconnectNeeded();
int GetTopicFromFileandUpdateList();
char* GetTopicFromSubcribeId(int subscribeId);
void printList();

void init_mqtt_timer (mqtt_timer_t *timer, int max_count);
void convertToUppercase(char *deviceId);
int writeToDBFile(char *db_file_path, char *data, size_t size);
void get_from_file(char *key, char **val, char *filepath);
void publish_notify_mqtt(char *pub_topic, void *payload, ssize_t len);
int get_global_mqtt_connected();
void reset_global_mqttConnected();
void set_global_mqttConnected();
int checkMqttParamSet();
pthread_mutex_t *get_global_mqtt_retry_mut(void);
pthread_cond_t *get_global_mqtt_retry_cond(void);
int validateForMqttInit();
pthread_cond_t *get_global_mqtt_cond(void);
pthread_mutex_t *get_global_mqtt_mut(void);
int regMqttDataModel();
int execute_mqtt_script(char *name);
int getHostIPFromInterface(char *interface, char **ip);
void fetchMqttParamsFromDB();
int mqtt_subscribe(char *comp, char *topic);
int mqttCMRbusInit();
bool isRbusEnabled();
void mqttCMRbus_Uninit();
bool mqttCMConnectBroker();
void registerRbusLogger();
void get_interface(char **interface);
pthread_cond_t *get_global_mqtt1_con(void);
pthread_mutex_t *get_global_mqtt1_mut(void);
rbusHandle_t get_global_rbus_handle(void);
void stripAndAddModuleName(char *str, const char *substr, const char *newstr);
