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

#include "mqttcm_connect.h"
#include "mqttcm_log.h"
#include "mqttcm_generic.h"
#include "mqttcm_webcfg.h"

struct mosquitto *mosq = NULL;
static bool isRbus = false ;
static char* locationId = NULL;
static char* clientId = NULL;
static char* Port =NULL;
static char* broker = NULL;
static char* connMode = NULL;
static int mqinit = 0;
rbusHandle_t rbus_handle;
static char* mqttdata = NULL;
static int broker_connect = 0;
static int reconnectFlag = 0;
static int valueChangeFlag= 0;
static int webcfg_subscribed = 0;

pthread_mutex_t mqtt_retry_mut=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t mqtt_retry_con=PTHREAD_COND_INITIALIZER;
pthread_mutex_t mqtt_mut=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t mqtt_con=PTHREAD_COND_INITIALIZER;

void init_mqtt_timer (mqtt_timer_t *timer, int max_count);
static comp_topic_name_t *g_head = NULL;

rbusHandle_t get_global_rbus_handle(void)
{
     return rbus_handle;
}

int get_global_shutdown()
{
	return 0;
}
void convertToUppercase(char* deviceId)
{
	int j =0;
	while (deviceId[j])
	{
		deviceId[j] = toupper(deviceId[j]);
		j++;
	}
}

pthread_cond_t *get_global_mqtt_retry_cond(void)
{
    return &mqtt_retry_con;
}

pthread_mutex_t *get_global_mqtt_retry_mut(void)
{
    return &mqtt_retry_mut;
}

pthread_cond_t *get_global_mqtt_cond(void)
{
    return &mqtt_con;
}

pthread_mutex_t *get_global_mqtt_mut(void)
{
    return &mqtt_mut;
}

int isReconnectNeeded()
{
	return reconnectFlag;
}

int valueChangeCheck(char *valueStored, char *valueChanged)
{
	if(valueStored != NULL && valueChanged != NULL)
	{

		if(strcmp(valueStored, valueChanged)!= 0)
		{
			valueChangeFlag = 1;
			return 1;
		}
	}
	return 0;
}

bool isRbusEnabled() 
{
	if(RBUS_ENABLED == rbus_checkStatus())
	{
		isRbus = true;
	}
	else
	{
		isRbus = false;
	}
	MqttCMInfo("MQTTCM RBUS mode active status = %s\n", isRbus ? "true":"false");
	return isRbus;
}

void mosquittoTriggerDisconnect()
{
	if(reconnectFlag!=1)
	{
		mosquitto_disconnect(mosq);
	}
	else
	{
		MqttCMInfo("Reconnect is in progress, so skipping this reconnect request\n");
	}
	valueChangeFlag = 0;
}

//Rbus registration with mqttCM
int mqttCMRbusInit(char *pComponentName)
{
	int ret = RBUS_ERROR_SUCCESS;

	MqttCMInfo("rbus_open for component %s\n", pComponentName);
	ret = rbus_open(&rbus_handle, pComponentName);
	if(ret != RBUS_ERROR_SUCCESS)
	{
		MqttCMError("mqttCMRbusInit failed with error code %d\n", ret);
		return 0;
	}
	MqttCMInfo("mqttCMRbusInit is success. ret is %d\n", ret);
	return 1;
}

int mqttCMRbus_Uninit()
{
    rbus_close(rbus_handle);
    return 1;
}

//Initialize mqtt library and connect to mqtt broker
bool mqttCMConnectBroker()
{
	char *username = NULL;
	int rc;
	int port = 0;
	mqtt_timer_t mqtt_timer;
	int tls_count = 0;
	int rt = 0;
	char *bind_interface = NULL;
	char *hostip = NULL;

	checkMqttParamSet();
	
	res_init();
	
	MqttCMInfo("Initializing MQTT library\n");
	mosquitto_lib_init();

	int clean_session = false;

	if (clientId !=NULL)
	{
		while(1)
		{
			username = clientId;
			MqttCMInfo("clientId is %s username is %s\n", clientId, username);

			execute_mqtt_script(OPENSYNC_CERT);

			if(clientId !=NULL)
			{
				mosq = mosquitto_new(clientId, clean_session, NULL);
			}
			else
			{
				MqttCMInfo("clientId is NULL, init with clean_session true\n");
				mosq = mosquitto_new(NULL, true, NULL);
			}
			if(!mosq)
			{
				MqttCMError("Error initializing mosq instance\n");
				return MOSQ_ERR_NOMEM;
			}
			mosquitto_int_option(mosq, MOSQ_OPT_PROTOCOL_VERSION, MQTT_PROTOCOL_V5);
			struct libmosquitto_tls *tls;
			tls = malloc (sizeof (struct libmosquitto_tls));
			if(tls)
			{
				memset(tls, 0, sizeof(struct libmosquitto_tls));

				char * CAFILE, *CERTFILE , *KEYFILE = NULL;

				get_from_file("CA_FILE_PATH=", &CAFILE, MQTT_CONFIG_FILE);
				get_from_file("CERT_FILE_PATH=", &CERTFILE, MQTT_CONFIG_FILE);
				get_from_file("KEY_FILE_PATH=", &KEYFILE, MQTT_CONFIG_FILE);

				if(CAFILE !=NULL && CERTFILE!=NULL && KEYFILE !=NULL)
				{
					MqttCMInfo("CAFILE %s, CERTFILE %s, KEYFILE %s MOSQ_TLS_VERSION %s\n", CAFILE, CERTFILE, KEYFILE, MOSQ_TLS_VERSION);

					tls->cafile = CAFILE;
					tls->certfile = CERTFILE;
					tls->keyfile = KEYFILE;
					tls->tls_version = MOSQ_TLS_VERSION;

					rc = mosquitto_tls_set(mosq, tls->cafile, tls->capath, tls->certfile, tls->keyfile, tls->pw_callback);
					MqttCMInfo("mosquitto_tls_set rc %d\n", rc);
					if(rc)
					{
						MqttCMError("Failed in mosquitto_tls_set %d %s\n", rc, mosquitto_strerror(rc));
					}
					else
					{
						rc = mosquitto_tls_opts_set(mosq, tls->cert_reqs, tls->tls_version, tls->ciphers);
						MqttCMInfo("mosquitto_tls_opts_set rc %d\n", rc);
						if(rc)
						{
							MqttCMError("Failed in mosquitto_tls_opts_set %d %s\n", rc, mosquitto_strerror(rc));
						}
					}

				}
				else
				{
					MqttCMError("Failed to get tls cert files\n");
					rc = 1;
				}

				if(rc != MOSQ_ERR_SUCCESS)
				{
					if(tls_count < 3)
					{
						sleep(10);
						MqttCMInfo("Mqtt tls cert Retry %d in progress\n", tls_count+1);
						mosquitto_destroy(mosq);
						tls_count++;
					}
					else
					{
						MqttCMError("Mqtt tls cert retry failed!!!, Abort the process\n");

						mosquitto_destroy(mosq);

						MQTTCM_FREE(CAFILE);
						MQTTCM_FREE(CERTFILE);
						MQTTCM_FREE(KEYFILE);
						abort();
					}
				}
				else
				{
					tls_count = 0;
					//connect to mqtt broker
					mosquitto_connect_v5_callback_set(mosq, on_connect);
					mosquitto_disconnect_v5_callback_set(mosq, on_disconnect);
					mosquitto_subscribe_v5_callback_set(mosq, on_subscribe);
					mosquitto_message_v5_callback_set(mosq, on_message);
					mosquitto_publish_v5_callback_set(mosq, on_publish);

					MqttCMDebug("port %d\n", port);

					init_mqtt_timer(&mqtt_timer, MAX_MQTT_RETRY);

					get_interface(&bind_interface);
					if(bind_interface != NULL)
					{
						MqttCMInfo("Interface fetched for mqtt connect bind is %s\n", bind_interface);
						rt = getHostIPFromInterface(bind_interface, &hostip);
						if(rt == 1)
						{
							MqttCMInfo("hostip fetched from getHostIPFromInterface is %s\n", hostip);
						}
						else
						{
							MqttCMError("getHostIPFromInterface failed %d\n", rt);
						}
					}
					while(1)
					{

						MqttCMInfo("Port fetched from TR181 is %s\n", Port);
						if(Port !=NULL && strlen(Port) > 0)
						{
							port = atoi(Port);
						}
						else
						{
							port = MQTT_PORT;
						}
						MqttCMDebug("port int %d\n", port);

						rc = mosquitto_connect_bind_v5(mosq, broker, port, KEEPALIVE, hostip, NULL);

						MqttCMInfo("mosquitto_connect_bind rc %d\n", rc);
						if(rc != MOSQ_ERR_SUCCESS)
						{

							MqttCMError("mqtt connect Error: %s\n", mosquitto_strerror(rc));
							if(mqtt_retry(&mqtt_timer) != MQTT_DELAY_TAKEN)
							{
								mosquitto_destroy(mosq);

								MQTTCM_FREE(CAFILE);
								MQTTCM_FREE(CERTFILE);
								MQTTCM_FREE(KEYFILE);
								return rc;
							}
						}
						else
						{
							MqttCMInfo("mqtt broker connect success %d\n", rc);
							break;
						}
					}

					MqttCMDebug("mosquitto_loop_forever\n");
					rc = mosquitto_loop_forever(mosq, -1, 1);
					if(rc != MOSQ_ERR_SUCCESS)
					{
						mosquitto_destroy(mosq);
						MqttCMError("mosquitto_loop_start Error: %s\n", mosquitto_strerror(rc));

						MQTTCM_FREE(CAFILE);
						MQTTCM_FREE(CERTFILE);
						MQTTCM_FREE(KEYFILE);
						return rc;
					}
					else
					{
						mosquitto_destroy(mosq);
						mosquitto_lib_cleanup();
						MqttCMDebug("after loop rc is %d\n", rc);
						break;
					}
				}
			}
			else
			{
				MqttCMError("Allocation failed\n");
				rc = MOSQ_ERR_NOMEM;
			}
		}

	}
	else
	{
		MqttCMError("Failed to get clientId\n");
		return 1;

	}
	return rc;
}

int checkMqttParamSet()
{
	if( !validateForMqttInit())
	{
		MqttCMDebug("Validation success for mqtt parameters, proceed to mqtt init\n");
		return 1;
	}
	else
	{
		pthread_mutex_lock(get_global_mqtt_mut());
		pthread_cond_wait(get_global_mqtt_cond(), get_global_mqtt_mut());
		pthread_mutex_unlock(get_global_mqtt_mut());
		MqttCMInfo("Received mqtt signal proceed to mqtt init\n");
		return 0;
	}
}

int validateForMqttInit()
{
	if(mqinit == 0)
	{
		MqttCMDebug("validateForMqttInit. locationId %s clientId %s broker %s \n", locationId, clientId, broker);
		if (locationId != NULL && clientId != NULL && broker != NULL)
		{
			if ((strlen(locationId) != 0) && (strlen(clientId) != 0) && (strlen(broker) !=0))
			{
				MqttCMInfo("All 3 mandatory params locationId, NodeId and broker are set, proceed to mqtt init\n");
				mqinit = 1;
				pthread_mutex_lock (&mqtt_mut);
				pthread_cond_signal(&mqtt_con);
				pthread_mutex_unlock (&mqtt_mut);
				return 0;
			}
			else
			{
				MqttCMInfo("All 3 mandatory params locationId, NodeId and broker are not set, waiting..\n");
				return 1;
			}
		}
		else
		{
			MqttCMInfo("All 3 mandatory params locationId, NodeId and broker are not set, waiting..\n");
			return 1;
		}
	}
	return 0;
}

// callback called when the client receives a CONNACK message from the broker
void on_connect(struct mosquitto *mosq, void *obj, int reason_code, int flag, const mosquitto_property *props)
{
        MqttCMInfo("on_connect: reason_code %d %s\n", reason_code, mosquitto_connack_string(reason_code));
        if(reason_code != 0)
	{
		MqttCMError("on_connect received error\n");
                //reconnect
                mosquitto_disconnect(mosq);
		return;
        }

	MqttCMInfo("on_connect: success. broker_connect set to 1\n");
	broker_connect = 1;

	//For Mqtt reconnection case, use in-memory subscribe list to subscribe the components again
	if(reconnectFlag)
	{
		//printList();
		comp_topic_name_t* temp = g_head;

		while (temp != NULL)
		{
			MqttCMDebug("Inside mqtt subscribe of %s\n", temp->compName);
			mqtt_subscribe(temp->compName, temp->topic);
			temp = temp->next;
		}

		reconnectFlag = 0;
	}
	else  //For crash or process restart case to get the subscribe details from file
	{
		if(GetTopicFromFileandUpdateList())
		{
			MqttCMInfo("Get subscriber topic from file and update list is successful\n");
			//printList();
			comp_topic_name_t* temp = g_head;

			while (temp != NULL)
			{
				MqttCMDebug("Inside mqtt else case subscribe of %s\n", temp->compName);
				mqtt_subscribe(temp->compName, temp->topic);
				temp = temp->next;
			}
		}
	}

}

// callback called when the broker sends a SUBACK in response to a SUBSCRIBE.
void on_subscribe(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos, const mosquitto_property *props)
{
        int i;
        bool have_subscription = false;

	MqttCMInfo("on_subscribe callback: qos_count is %d mid is %d\n", qos_count, mid);

	char *topicname = GetTopicFromSubcribeId(mid);

	if(topicname != NULL)
	{
		MqttCMInfo("topicname is %s\n", topicname);

		if(strcmp(topicname, SUBSCRIBE_WEBCONFIG) == 0)
		{
			//send on_subscribe callback event to webconfig via rbus.
			webcfg_subscribed = 1;
			sendRbusEventWebcfgOnSubscribe();
		}
	}
	else
	{
		MqttCMError("Failed to get subscribe topic name from mid\n");
	}

        for(i=0; i<qos_count; i++)
	{
                MqttCMInfo("on_subscribe: %d:granted qos = %d\n", i, granted_qos[i]);
		if(granted_qos[i] <= 2)
		{
			have_subscription = true;
		}
        }
        if(have_subscription == false)
	{
                MqttCMError("Error: All subscriptions rejected.\n");
                mosquitto_disconnect(mosq);
        }
}

/* callback called when the client receives a message. */
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg, const mosquitto_property *props)
{
	if(msg !=NULL)
	{
		if(msg->payload !=NULL)
		{
			MqttCMInfo("Received message from %s qos %d payloadlen %d payload %s\n", msg->topic, msg->qos, msg->payloadlen, (char *)msg->payload);

			int dataSize = msg->payloadlen;
			char *topic_name = msg->topic;
			char * data = malloc(sizeof(char) * dataSize+1);
			if(data !=NULL)
			{
				memset(data, 0, sizeof(char) * dataSize+1);
				data = memcpy(data, (char *) msg->payload, dataSize+1);
				data[dataSize] = '\0';

				MqttCMInfo("Received dataSize is %d\n", dataSize);
				MqttCMDebug("write to file /tmp/subscribe_message.bin\n");
				writeToDBFile("/tmp/subscribe_message.bin",(char *)data,dataSize);
				MqttCMInfo("write to file done\n");
			
				if(mqttdata)
				{
					MQTTCM_FREE(mqttdata);
					mqttdata= NULL;
				}

				mqttdata = malloc(sizeof(char) * dataSize);
				if(mqttdata !=NULL)
				{
					memset(mqttdata, 0, sizeof(char) * dataSize);
					mqttdata = memcpy(mqttdata, data, dataSize );
					MQTTCM_FREE(data);
					data = NULL;

					//send on_message callback event to webconfig via rbus.
					MqttCMInfo("The topic received from the broker is %s\n",topic_name);
					const char *CompName = getComponentFromTopicName(topic_name);
					if(CompName != NULL)
					{
						MqttCMInfo("The component name fetched from subscribeList for the received topic is %s\n",CompName);
						if(strcmp(CompName,SUBSCRIBE_WEBCONFIG) == 0)
						{
							//send on_message callback event to webconfig via rbus.
							MqttCMDebug("Before sendRusEventWebcfgOnMessage funciton\n");
							sendRbusEventWebcfgOnMessage(mqttdata, dataSize,topic_name);
						}
						else
						{
							MqttCMError("Couldnt find the topic in the list\n");
						}
					}
					else
					{
						MqttCMError("Component not found or its null\n");
					}

				}
				else
				{
					MqttCMError("mqttdata malloc failed\n");
				}
			}
			else
			{
				MqttCMError("on_message data malloc failed\n");
			}
		}
		else
		{
			MqttCMError("Received payload from mqtt is NULL\n");
		}
	}
	else
	{
		MqttCMError("Received message from mqtt is NULL\n");
	}
}

const char *getComponentFromTopicName(char *topic)
{
	comp_topic_name_t* current = g_head;
	while (current != NULL)
	{
		MqttCMInfo("Component name is %s and the topic is %s\n", current->compName, current->topic);
		if(topic != NULL && (strcmp(current->topic, topic) == 0))
		{
			return current->compName;
		}
		current = current->next;
	}
	// If topic not found
	MqttCMError("Topic is not found\n");
	return NULL;
}

void on_publish(struct mosquitto *mosq, void *obj, int mid, int reason_code, const mosquitto_property *props)
{
	MqttCMInfo("Message with mid %d has been published.\n", mid);

	//send on_publish callback event to webconfig via rbus.
	sendRbusEventWebcfgOnPublish(mid);
}

// callback called when the client gets DISCONNECT command from the broker
void on_disconnect(struct mosquitto *mosq, void *obj, int reason_code, const mosquitto_property *props)
{
        MqttCMInfo("on_disconnect: reason_code %d %s\n", reason_code, mosquitto_reason_string(reason_code));

	//Resetting to trigger sync on wan_restore
	reconnectFlag = 1;
	mqinit = 0;
	webcfg_subscribed = 0;
	broker_connect = 0; //To reset the broker connection status

	comp_topic_name_t* temp = g_head;
	while (temp != NULL)
	{
		temp->subscribeOnFlag = 0;
		MqttCMInfo("%s component is unsubscribed from topic %s\n", temp->compName, temp->topic);
		temp = temp->next;
	}
}

/* Enables rbus ERROR level logs in mqttcm. Modify RBUS_LOG_ERROR check if more debug logs are needed from rbus. */
void rbus_log_handler(
    rbusLogLevel level,
    const char* file,
    int line,
    int threadId,
    char* message)
{
    const char* slevel = "";

    if(level < RBUS_LOG_ERROR)
        return;

    switch(level)
    {
	    case RBUS_LOG_DEBUG:    slevel = "DEBUG";   break;
	    case RBUS_LOG_INFO:     slevel = "INFO";    break;
	    case RBUS_LOG_WARN:     slevel = "WARN";    break;
	    case RBUS_LOG_ERROR:    slevel = "ERROR";   break;
	    case RBUS_LOG_FATAL:    slevel = "FATAL";   break;
    }
    MqttCMInfo("%5s %s:%d -- %s\n", slevel, file, line, message);
}

int registerRbusLogger()
{
	rbus_registerLogHandler(rbus_log_handler);
	MqttCMInfo("Registered rbus log handler\n");
	return 1;
}

void init_mqtt_timer (mqtt_timer_t *timer, int max_count)
{
	timer->count = 1;
	timer->max_count = max_count;
	timer->delay = 3;  //7s,15s,31s....
	clock_gettime (CLOCK_MONOTONIC, &timer->ts);
}

unsigned update_mqtt_delay (mqtt_timer_t *timer)
{
	if (timer->count < timer->max_count)
	{
		timer->count += 1;
		timer->delay = timer->delay + timer->delay + 1;
		// 3,7,15,31 ..
	}
	return (unsigned) timer->delay;
}

unsigned mqtt_rand_secs (int random_num, unsigned max_secs)
{
	unsigned delay_secs = (unsigned) random_num & max_secs;
	if (delay_secs < 3)
		return delay_secs + 3;
	else
		return delay_secs;
}

unsigned mqtt_rand_nsecs (int random_num)
{
	/* random _num is in range 0..2147483648 */
	unsigned n = (unsigned) random_num >> 1;
	/* n is in range 0..1073741824 */
	if (n < 1000000000)
		return n;
	return n - 1000000000;
}

void mqtt_add_timespec (struct timespec *t1, struct timespec *t2)
{
	t2->tv_sec += t1->tv_sec;
	t2->tv_nsec += t1->tv_nsec;
	if (t2->tv_nsec >= 1000000000)
	{
		t2->tv_sec += 1;
		t2->tv_nsec -= 1000000000;
	}
}

void mqtt_rand_expiration (int random_num1, int random_num2, mqtt_timer_t *timer, struct timespec *ts)
{
	unsigned max_secs = update_mqtt_delay (timer); // 3,7,15,31
	struct timespec ts_delay = {3, 0};

	if (max_secs > 3)
	{
		ts_delay.tv_sec = mqtt_rand_secs (random_num1, max_secs);
		ts_delay.tv_nsec = mqtt_rand_nsecs (random_num2);
	}
	MqttCMInfo("Waiting max delay %u mqttRetryTime %lld secs %ld usecs\n",
	max_secs, (long long) ts_delay.tv_sec, ts_delay.tv_nsec/1000);

	/* Add delay to expire time */
	mqtt_add_timespec (&ts_delay, ts);
}

/* mqtt_retry
 *
 * delays for the number of seconds specified in parameter timer
 * g_shutdown can break out of the delay.
 *
 * returns -1 pthread_cond_timedwait error
 *  1   shutdown
 *  0    delay taken
*/
int mqtt_retry(mqtt_timer_t *timer)
{
	struct timespec ts;
	int rtn;

	pthread_condattr_t mqtt_retry_con_attr;

	pthread_condattr_init (&mqtt_retry_con_attr);
	pthread_condattr_setclock (&mqtt_retry_con_attr, CLOCK_MONOTONIC);
	pthread_cond_init (&mqtt_retry_con, &mqtt_retry_con_attr);

	clock_gettime(CLOCK_MONOTONIC, &ts);

	mqtt_rand_expiration(random(), random(), timer, &ts);

	pthread_mutex_lock(&mqtt_retry_mut);
	//The condition variable will only be set if we shut down.
	rtn = pthread_cond_timedwait(&mqtt_retry_con, &mqtt_retry_mut, &ts);
	pthread_mutex_unlock(&mqtt_retry_mut);

	pthread_condattr_destroy(&mqtt_retry_con_attr);

	if (get_global_shutdown())
	return MQTT_RETRY_SHUTDOWN;
	if ((rtn != 0) && (rtn != ETIMEDOUT))
	{
		MqttCMError("pthread_cond_timedwait error (%d) in mqtt_retry.\n", rtn);
		return MQTT_RETRY_ERR;
	}
	return MQTT_DELAY_TAKEN;
}

/* This function is used to publish the messages received from components to Broker.*/
int publish_notify_mqtt(char *pub_topic, void *payload, ssize_t len)
{
        int rc;

	mosquitto_property *props = NULL;
	uuid_t uuid;
	uuid_generate_time(uuid);

	char uuid_str[37];
	uuid_unparse(uuid, uuid_str);

	MqttCMInfo("uuidv1 generated is %s\n", uuid_str);

	int ret = mosquitto_property_add_string_pair(&props, MQTT_PROP_USER_PROPERTY, "uuid", uuid_str);

	if(ret != MOSQ_ERR_SUCCESS)
	{
		MqttCMError("Failed to add property: %d\n", ret);
	}

	rc = mosquitto_publish_v5(mosq, NULL, pub_topic, len, payload, 2, false, props);

	MqttCMInfo("Publish rc %d\n", rc);
        if(rc != MOSQ_ERR_SUCCESS)
	{
                MqttCMError("Error publishing: %s\n", mosquitto_strerror(rc));
        }
	else
	{
		MqttCMInfo("Publish payload success %d\n", rc);
	}
	mosquitto_loop(mosq, 0, 1);
	mosquitto_property_free_all(&props);
	MqttCMDebug("Publish mosquitto_loop done\n");
	return rc;
}

void get_from_file(char *key, char **val, char *filepath)
{
        FILE *fp = fopen(filepath, "r");

        if (NULL != fp)
        {
                char str[255] = {'\0'};
                while (fgets(str, sizeof(str), fp) != NULL)
                {
                    char *value = NULL;

                    if(NULL != (value = strstr(str, key)))
                    {
                        value = value + strlen(key);
                        value[strlen(value)-1] = '\0';
                        *val = strdup(value);
                        break;
                    }

                }
                fclose(fp);
        }

        if (NULL == *val)
        {
                MqttCMError("val is not present in file\n");

        }
        else
        {
                MqttCMDebug("val fetched is %s\n", *val);
        }
}

int execute_mqtt_script(char *name)
{
    FILE* out = NULL, *file = NULL;
    char command[100] = {'\0'};

    if(strlen(name)>0)
    {
        file = fopen(name, "r");
        if(file)
        {
            snprintf(command,sizeof(command),"%s mqttcert-fetch", name);
            out = popen(command, "r");
            if(out)
            {
		MqttCMInfo("The Tls cert script executed successfully\n");
                pclose(out);

            }
            fclose(file);
	    return 1;
        }
        else
        {
            MqttCMError("File %s open error\n", name);
	    return 0;
        }
    }
    return 0;
}

int getHostIPFromInterface(char *interface, char **ip)
{
	int file, rc;
	struct ifreq infr;

	file = socket(AF_INET, SOCK_DGRAM, 0);
	if(file)
	{
		infr.ifr_addr.sa_family = AF_INET;
		strncpy(infr.ifr_name, interface, IFNAMSIZ-1);
		rc = ioctl(file, SIOCGIFADDR, &infr);
		close(file);
		if(rc == 0)
		{
			MqttCMDebug("%s\n", inet_ntoa(((struct sockaddr_in *)&infr.ifr_addr)->sin_addr));
			*ip = inet_ntoa(((struct sockaddr_in *)&infr.ifr_addr)->sin_addr);
			return 1;
		}
		else
		{
			MqttCMError("Failed in ioctl command to get host ip\n");
		}
	}
	else
	{
		MqttCMError("Failed to get host ip from interface\n");
	}
	return 0;
}

void fetchMqttParamsFromDB()
{
	char tmpLocationId[256]={'\0'};
	char tmpBroker[256]={'\0'};
	char tmpClientId[64]={'\0'};
	char tmpPort[32]={'\0'};
	char *client_id = NULL;

	Get_Mqtt_LocationId(tmpLocationId);
	if(tmpLocationId[0] != '\0')
	{
		locationId = strdup(tmpLocationId);
	}

	Get_Mqtt_Broker(tmpBroker);
	if(tmpBroker[0] != '\0')
	{
		broker = strdup(tmpBroker);
	}
	
	client_id = Get_Mqtt_ClientId();
	if( client_id != NULL && strlen(client_id) !=0 )
	{

              strncpy(tmpClientId, client_id, sizeof(tmpClientId)-1);

              if(tmpClientId[0] != '\0')
	      {
		   clientId = strdup(tmpClientId);
	      }
	}

	Get_Mqtt_Port(tmpPort);
	if(tmpPort[0] != '\0')
	{
		Port = strdup(tmpPort);
	}
	MqttCMInfo("Mqtt params fetched from DB, locationId %s broker %s clientId %s Port %s\n", locationId, broker, clientId,Port);
}

rbusError_t MqttLocationIdSetHandler(rbusHandle_t handle, rbusProperty_t prop, rbusSetHandlerOptions_t* opts)
{
	(void) handle;
	(void) opts;
	char const* paramName = rbusProperty_GetName(prop);

	if(strncmp(paramName, MQTT_LOCATIONID_PARAM, maxParamLen) != 0)
	{
		MqttCMError("Unexpected parameter = %s\n", paramName);
		return RBUS_ERROR_ELEMENT_DOES_NOT_EXIST;
	}

	rbusError_t retPsmSet = RBUS_ERROR_BUS_ERROR;
	MqttCMInfo("Parameter name is %s \n", paramName);
	rbusValueType_t type_t;
	rbusValue_t paramValue_t = rbusProperty_GetValue(prop);
	if(paramValue_t) {
		type_t = rbusValue_GetType(paramValue_t);
	} else {
		MqttCMError("Invalid input to set\n");
		return RBUS_ERROR_INVALID_INPUT;
	}

	if(strncmp(paramName, MQTT_LOCATIONID_PARAM, maxParamLen) == 0)
	{
		if(type_t == RBUS_STRING) {
			char* data = rbusValue_ToString(paramValue_t, NULL, 0);
			if(data) {
				MqttCMInfo("Call datamodel function  with data %s\n", data);

				if(locationId) {
					valueChangeCheck(locationId, data);
					MQTTCM_FREE(locationId);
					locationId = NULL;
				}
				locationId = strdup(data);
				MQTTCM_FREE(data);
				MqttCMInfo("LocationId after processing %s\n", locationId);
				retPsmSet = rbus_StoreValueIntoDB( MQTT_LOCATIONID_PARAM, locationId);
				if (retPsmSet != RBUS_ERROR_SUCCESS)
				{
					MqttCMError("psm_set failed ret %d for parameter %s and value %s\n", retPsmSet, paramName, locationId);
					return retPsmSet;
				}
				else
				{
					MqttCMInfo("psm_set success ret %d for parameter %s and value %s\n", retPsmSet, paramName, locationId);
				}
				validateForMqttInit();
				if(valueChangeFlag)
				{
					mosquittoTriggerDisconnect();
				}
			}
		} else {
			MqttCMError("Unexpected value type for property %s\n", paramName);
			return RBUS_ERROR_INVALID_INPUT;
		}
	}
	return RBUS_ERROR_SUCCESS;
}

rbusError_t MqttBrokerSetHandler(rbusHandle_t handle, rbusProperty_t prop, rbusSetHandlerOptions_t* opts)
{
	(void) handle;
	(void) opts;
	char const* paramName = rbusProperty_GetName(prop);

	if(strncmp(paramName, MQTT_BROKER_PARAM, maxParamLen) != 0)
	{
		MqttCMError("Unexpected parameter = %s\n", paramName);
		return RBUS_ERROR_ELEMENT_DOES_NOT_EXIST;
	}

	rbusError_t retPsmSet = RBUS_ERROR_BUS_ERROR;
	MqttCMInfo("Parameter name is %s \n", paramName);
	rbusValueType_t type_t;
	rbusValue_t paramValue_t = rbusProperty_GetValue(prop);
	if(paramValue_t) {
		type_t = rbusValue_GetType(paramValue_t);
	} else {
		MqttCMError("Invalid input to set\n");
		return RBUS_ERROR_INVALID_INPUT;
	}

	if(strncmp(paramName, MQTT_BROKER_PARAM, maxParamLen) == 0) {

		if(type_t == RBUS_STRING) {
			char* data = rbusValue_ToString(paramValue_t, NULL, 0);
			if(data) {
				MqttCMInfo("Call datamodel function  with data %s\n", data);

				if(broker) {
					valueChangeCheck(broker, data);
					MQTTCM_FREE(broker);
					broker= NULL;
				}
				broker = strdup(data);
				MQTTCM_FREE(data);
				MqttCMInfo("Broker after processing %s\n", broker);
				retPsmSet = rbus_StoreValueIntoDB( MQTT_BROKER_PARAM, broker);
				if (retPsmSet != RBUS_ERROR_SUCCESS)
				{
					MqttCMError("psm_set failed ret %d for parameter %s and value %s\n", retPsmSet, paramName, broker);
					return retPsmSet;
				}
				else
				{
					MqttCMInfo("psm_set success ret %d for parameter %s and value %s\n", retPsmSet, paramName, broker);
				}
				validateForMqttInit();
				if(valueChangeFlag)
				{
					mosquittoTriggerDisconnect();
				}
			}
		} else {
			MqttCMError("Unexpected value type for property %s\n", paramName);
			return RBUS_ERROR_INVALID_INPUT;
		}
	}
	return RBUS_ERROR_SUCCESS;
}

rbusError_t MqttPortSetHandler(rbusHandle_t handle, rbusProperty_t prop, rbusSetHandlerOptions_t* opts)
{
	(void) handle;
	(void) opts;
	char const* paramName = rbusProperty_GetName(prop);

	if(strncmp(paramName, MQTT_PORT_PARAM, maxParamLen) != 0)
	{
		MqttCMError("Unexpected parameter = %s\n", paramName);
		return RBUS_ERROR_ELEMENT_DOES_NOT_EXIST;
	}

	rbusError_t retPsmSet = RBUS_ERROR_BUS_ERROR;
	MqttCMInfo("Parameter name is %s \n", paramName);
	rbusValueType_t type_t;
	rbusValue_t paramValue_t = rbusProperty_GetValue(prop);
	if(paramValue_t) {
		type_t = rbusValue_GetType(paramValue_t);
	} else {
		MqttCMError("Invalid input to set\n");
		return RBUS_ERROR_INVALID_INPUT;
	}

	if(strncmp(paramName, MQTT_PORT_PARAM, maxParamLen) == 0)
	{
		if(type_t == RBUS_STRING) {
			char* data = rbusValue_ToString(paramValue_t, NULL, 0);
			if(data) {
				MqttCMInfo("Call datamodel function  with data %s\n", data);

				if(Port) {
					valueChangeCheck(Port, data);
					MQTTCM_FREE(Port);
					Port = NULL;
				}
				Port = strdup(data);
				MQTTCM_FREE(data);
				MqttCMInfo("Port after processing %s\n", Port);
				retPsmSet = rbus_StoreValueIntoDB( MQTT_PORT_PARAM, Port);
				if (retPsmSet != RBUS_ERROR_SUCCESS)
				{
					MqttCMError("psm_set failed ret %d for parameter %s and value %s\n", retPsmSet, paramName, Port);
					return retPsmSet;
				}
				else
				{
					MqttCMInfo("psm_set success ret %d for parameter %s and value %s\n", retPsmSet, paramName, Port);
				}
				validateForMqttInit();
				if(valueChangeFlag)
				{
					mosquittoTriggerDisconnect();
				}
			}
		} else {
			MqttCMError("Unexpected value type for property %s\n", paramName);
			return RBUS_ERROR_INVALID_INPUT;
		}
	}
	return RBUS_ERROR_SUCCESS;
}

rbusError_t MqttConnModeSetHandler(rbusHandle_t handle, rbusProperty_t prop, rbusSetHandlerOptions_t* opts)
{
	(void) handle;
	(void) opts;
	char const* paramName = rbusProperty_GetName(prop);

	if(strncmp(paramName, MQTT_CONNECTMODE_PARAM, maxParamLen) != 0)
	{
		MqttCMError("Unexpected parameter = %s\n", paramName);
		return RBUS_ERROR_ELEMENT_DOES_NOT_EXIST;
	}

	rbusError_t retPsmSet = RBUS_ERROR_BUS_ERROR;
	MqttCMInfo("Parameter name is %s \n", paramName);
	rbusValueType_t type_t;
	rbusValue_t paramValue_t = rbusProperty_GetValue(prop);
	if(paramValue_t) {
		type_t = rbusValue_GetType(paramValue_t);
	} else {
		MqttCMError("Invalid input to set\n");
		return RBUS_ERROR_INVALID_INPUT;
	}

	if(strncmp(paramName, MQTT_CONNECTMODE_PARAM, maxParamLen) == 0) {

		if(type_t == RBUS_STRING) {
			char* data = rbusValue_ToString(paramValue_t, NULL, 0);
			if(data) {
				//Only Single broker connection is supported, Dual will be revisited in future
				//if(((strcmp (data, "Single") == 0)) || (strcmp (data, "Dual") == 0))
				if((strcmp (data, "Single") == 0))
				{
					MqttCMInfo("Call datamodel function  with data %s\n", data);

					if(connMode) {
						MQTTCM_FREE(connMode);
						connMode= NULL;
					}
					connMode = strdup(data);
					MQTTCM_FREE(data);
					MqttCMInfo("connMode after processing %s\n", connMode);
					retPsmSet = rbus_StoreValueIntoDB( MQTT_CONNECTMODE_PARAM, connMode);
					if (retPsmSet != RBUS_ERROR_SUCCESS)
					{
						MqttCMError("psm_set failed ret %d for parameter %s and value %s\n", retPsmSet, paramName, connMode);
						return retPsmSet;
					}
					else
					{
						MqttCMInfo("psm_set success ret %d for parameter %s and value %s\n", retPsmSet, paramName, connMode);
					}
				}
				else
				{
					MqttCMError("Invalid value to set\n");
					return RBUS_ERROR_INVALID_INPUT;
				}
			}
		} else {
			MqttCMError("Unexpected value type for property %s\n", paramName);
			return RBUS_ERROR_INVALID_INPUT;
		}
	}
	return RBUS_ERROR_SUCCESS;
}

rbusError_t MqttSubscribeMethodHandler(rbusHandle_t handle, char const* methodName, rbusObject_t inParams, rbusObject_t outParams, rbusMethodAsyncHandle_t asyncHandle)
{
	(void)handle;
        (void)asyncHandle;
        char *compname_str = NULL, *topic_str = NULL;

        MqttCMInfo("methodHandler called: %s\n", methodName);
        //rbusObject_fwrite(inParams, 1, stdout);
        if(strncmp(methodName, MQTT_SUBSCRIBE_PARAM, maxParamLen) == 0)
        {
                rbusValue_t compname = rbusObject_GetValue(inParams, "compname");
                if(compname)
                {
                        if(rbusValue_GetType(compname) == RBUS_STRING)
                        {
                                compname_str = (char *) rbusValue_GetString(compname, NULL);
                                if(compname_str)
                                {
                                        MqttCMInfo("compname value received is %s\n",compname_str);
                                }
				else
				{
					MqttCMError("compname is Invalid or NULL\n");
					return RBUS_ERROR_INVALID_INPUT;
				}
                        }

                }
                else
                {
                        MqttCMError("compname is empty\n");
			return RBUS_ERROR_INVALID_INPUT;
                }

                rbusValue_t topic = rbusObject_GetValue(inParams, "topic");
                if(topic)
                {
                        if(rbusValue_GetType(topic) == RBUS_STRING)
                        {
                                topic_str = (char *) rbusValue_GetString(topic, NULL);
				if(topic_str)
				{
					MqttCMInfo("topic value received is %s\n",topic_str);
				}
				else
				{
					MqttCMError("topic_str is Invalid or NULL\n");
					return RBUS_ERROR_INVALID_INPUT;
				}
                        }
                }
                else
                {
                        MqttCMError("topic is empty\n");
			return RBUS_ERROR_INVALID_INPUT;
                }

		if(compname_str)
		{
			MqttCMInfo("%s proceed to mqtt_subscribe\n", compname_str);
			mqtt_subscribe(compname_str, topic_str);
		}
		else
                {
                        MqttCMError("Invalid method value to set\n");
			return RBUS_ERROR_INVALID_INPUT;
                }

	}
	else
	{
		MqttCMError("Unexpected value type for property %s\n", methodName);
		return RBUS_ERROR_INVALID_INPUT;
	}
	return RBUS_ERROR_SUCCESS;
}

rbusError_t MqttPublishMethodHandler(rbusHandle_t handle, char const* methodName, rbusObject_t inParams, rbusObject_t outParams, rbusMethodAsyncHandle_t asyncHandle)
{
	(void)handle;
	(void)asyncHandle;
	void *payload_bytes = NULL;
	char *payload_str = NULL, *topic_str = NULL, *qos_str = NULL;
	int  msg_len = 0;

	//char *pub_get_topic = NULL;

	MqttCMInfo("methodHandler called: %s\n", methodName);
	//rbusObject_fwrite(inParams, 1, stdout);
	if(strncmp(methodName, MQTT_PUBLISH_PARAM, maxParamLen) == 0)
	{
		rbusValue_t payload = rbusObject_GetValue(inParams, "payload");
		if(payload)
		{
			if(rbusValue_GetType(payload) == RBUS_BYTES)
			{
				payload_bytes = (void *) rbusValue_GetBytes(payload, &msg_len);
				if(payload_bytes)
				{
					MqttCMInfo("payload bytes value recieved successfully\n");
				}
			}
			else if(rbusValue_GetType(payload) == RBUS_STRING)
			{
				payload_str = (char *) rbusValue_GetString(payload, NULL);
				if(payload_str)
				{
					MqttCMInfo("payload string value recieved successfully\n");
				}
			}
		}
		else
		{
			MqttCMError("payload is empty\n");
			return RBUS_ERROR_INVALID_INPUT;
		}

		rbusValue_t topic = rbusObject_GetValue(inParams, "topic");
		if(topic)
		{
			if(rbusValue_GetType(topic) == RBUS_STRING)
			{
				topic_str = (char *) rbusValue_GetString(topic, NULL);
				MqttCMInfo("topic value received is %s\n",topic_str);
			}
		}
		else
		{
			MqttCMError("topic is empty\n");
			return RBUS_ERROR_INVALID_INPUT;
		}

		rbusValue_t qos = rbusObject_GetValue(inParams, "qos");
		if(qos)
		{
			if(rbusValue_GetType(qos) == RBUS_STRING)
			{
				qos_str = (char *) rbusValue_GetString(qos,NULL);
				if(qos_str)
				{
					MqttCMInfo("qos from TR181 is %s\n",qos_str);
				}
			}
		}
		else
		{
			MqttCMError("qos is empty");
			return RBUS_ERROR_INVALID_INPUT;
		}

		if (payload_bytes != NULL)
		{
			MqttCMInfo("Length of the payload bytes before publishing is %d\n", msg_len);
			publish_notify_mqtt(topic_str, payload_bytes, msg_len);
		}
		else if (payload_str != NULL)
                {
			MqttCMInfo("Length of the payload string before publishing is %zu\n", strlen(payload_str));
			publish_notify_mqtt(topic_str, payload_str, strlen(payload_str));
		}
		MqttCMInfo("publish_notify_mqtt done\n");
	}
	else 
	{
		MqttCMError("Unexpected value type for property %s\n", methodName);
		return RBUS_ERROR_INVALID_INPUT;
	}
	return RBUS_ERROR_SUCCESS;

}

rbusError_t MqttLocationIdGetHandler(rbusHandle_t handle, rbusProperty_t property, rbusGetHandlerOptions_t* opts)
{

    (void) handle;
    (void) opts;
    char const* propertyName;
    rbusError_t retPsmGet = RBUS_ERROR_BUS_ERROR;

    propertyName = rbusProperty_GetName(property);
    if(propertyName) {
        MqttCMDebug("Property Name is %s \n", propertyName);
    } else {
        MqttCMError("Unable to handle get request for property \n");
        return RBUS_ERROR_INVALID_INPUT;
	}
   if(strncmp(propertyName, MQTT_LOCATIONID_PARAM, maxParamLen) == 0)
   {

	rbusValue_t value;
        rbusValue_Init(&value);

        if(locationId){
            rbusValue_SetString(value, locationId);
	}
        else{
		retPsmGet = rbus_GetValueFromDB( MQTT_LOCATIONID_PARAM, &locationId );
		if (retPsmGet != RBUS_ERROR_SUCCESS){
			MqttCMError("psm_get failed ret %d for parameter %s and value %s\n", retPsmGet, propertyName, locationId);
			if(value)
			{
				rbusValue_Release(value);
			}
			return retPsmGet;
		}
		else{
			MqttCMInfo("psm_get success ret %d for parameter %s and value %s\n", retPsmGet, propertyName, locationId);
			if(locationId)
			{
				rbusValue_SetString(value, locationId);
			}
			else
			{
				MqttCMError("locationId is empty\n");
				rbusValue_SetString(value, "");
			}
		}
	}
        rbusProperty_SetValue(property, value);
        rbusValue_Release(value);

    }
    return RBUS_ERROR_SUCCESS;
}

rbusError_t MqttBrokerGetHandler(rbusHandle_t handle, rbusProperty_t property, rbusGetHandlerOptions_t* opts)
{

    (void) handle;
    (void) opts;
    char const* propertyName;
    rbusError_t retPsmGet = RBUS_ERROR_BUS_ERROR;

    propertyName = rbusProperty_GetName(property);
    if(propertyName) {
        MqttCMDebug("Property Name is %s \n", propertyName);
    } else {
        MqttCMError("Unable to handle get request for property \n");
        return RBUS_ERROR_INVALID_INPUT;
	}
   if(strncmp(propertyName, MQTT_BROKER_PARAM, maxParamLen) == 0)
   {

	rbusValue_t value;
        rbusValue_Init(&value);

        if(broker){
		rbusValue_SetString(value, broker);
	}
        else{
		retPsmGet = rbus_GetValueFromDB( MQTT_BROKER_PARAM, &broker );
		if (retPsmGet != RBUS_ERROR_SUCCESS){
			MqttCMError("psm_get failed ret %d for parameter %s and value %s\n", retPsmGet, propertyName, broker);
			if(value)
			{
				rbusValue_Release(value);
			}
			return retPsmGet;
		}
		else{
			MqttCMInfo("psm_get success ret %d for parameter %s and value %s\n", retPsmGet, propertyName, broker);
			if(broker)
			{
				rbusValue_SetString(value, broker);
			}
			else
			{
				MqttCMError("Broker is empty\n");
				rbusValue_SetString(value, "");
			}
		}
	}
        rbusProperty_SetValue(property, value);
        rbusValue_Release(value);

    }
    return RBUS_ERROR_SUCCESS;
}

rbusError_t MqttConnModeGetHandler(rbusHandle_t handle, rbusProperty_t property, rbusGetHandlerOptions_t* opts)
{

    (void) handle;
    (void) opts;
    char const* propertyName;
    //rbusError_t retPsmGet = RBUS_ERROR_BUS_ERROR;

    propertyName = rbusProperty_GetName(property);
    if(propertyName) {
        MqttCMDebug("Property Name is %s \n", propertyName);
    } else {
        MqttCMError("Unable to handle get request for property \n");
        return RBUS_ERROR_INVALID_INPUT;
	}
   if(strncmp(propertyName, MQTT_CONNECTMODE_PARAM, maxParamLen) == 0)
   {

	rbusValue_t value;
        rbusValue_Init(&value);

        if(connMode){
		rbusValue_SetString(value, connMode);
	}
        else{
		//Only Single broker connection is supported, so db get is disabled for now
		/*retPsmGet = rbus_GetValueFromDB( MQTT_CONNECTMODE_PARAM, &connMode );
		if (retPsmGet != RBUS_ERROR_SUCCESS){
			MqttCMError("psm_get failed ret %d for parameter %s and value %s\n", retPsmGet, propertyName, connMode);
			if(value)
			{
				rbusValue_Release(value);
			}
			return retPsmGet;
		}
		else{
			MqttCMInfo("psm_get success ret %d for parameter %s and value %s\n", retPsmGet, propertyName, connMode);
			if(connMode)
			{
				rbusValue_SetString(value, connMode);
			}
			else
			{
				MqttCMInfo("connMode is empty\n");
				rbusValue_SetString(value, "");
			}
		}*/
		rbusValue_SetString(value, "Single");
	}
        rbusProperty_SetValue(property, value);
        rbusValue_Release(value);

    }
    return RBUS_ERROR_SUCCESS;
}

rbusError_t MqttConnStatusGetHandler(rbusHandle_t handle, rbusProperty_t property, rbusGetHandlerOptions_t* opts)
{

    (void) handle;
    (void) opts;
    char const* propertyName;

    propertyName = rbusProperty_GetName(property);
    if(propertyName) {
        MqttCMDebug("Property Name is %s \n", propertyName);
    } else {
        MqttCMError("Unable to handle get request for property \n");
        return RBUS_ERROR_INVALID_INPUT;
	}
   if(strncmp(propertyName, MQTT_CONNSTATUS_PARAM, maxParamLen) == 0)
   {

	rbusValue_t value;
        rbusValue_Init(&value);

        if(broker_connect){
		rbusValue_SetString(value, "Up");
	}
        else{
		rbusValue_SetString(value, "Down");
	}
        rbusProperty_SetValue(property, value);
        rbusValue_Release(value);

    }
    return RBUS_ERROR_SUCCESS;
}

rbusError_t MqttPortGetHandler(rbusHandle_t handle, rbusProperty_t property, rbusGetHandlerOptions_t* opts)
{

    (void) handle;
    (void) opts;
    char const* propertyName;
    rbusError_t retPsmGet = RBUS_ERROR_BUS_ERROR;

    propertyName = rbusProperty_GetName(property);
    if(propertyName) {
        MqttCMDebug("Property Name is %s \n", propertyName);
    } else {
        MqttCMError("Unable to handle get request for property \n");
        return RBUS_ERROR_INVALID_INPUT;
	}
   if(strncmp(propertyName, MQTT_PORT_PARAM, maxParamLen) == 0)
   {

	rbusValue_t value;
        rbusValue_Init(&value);

        if(Port){
            rbusValue_SetString(value, Port);
	}
        else{
		retPsmGet = rbus_GetValueFromDB( MQTT_PORT_PARAM, &Port );
		if (retPsmGet != RBUS_ERROR_SUCCESS){
			MqttCMError("psm_get failed ret %d for parameter %s and value %s\n", retPsmGet, propertyName, Port);
			if(value)
			{
				rbusValue_Release(value);
			}
			return retPsmGet;
		}
		else{
			MqttCMInfo("psm_get success ret %d for parameter %s and value %s\n", retPsmGet, propertyName, Port);
			if(Port)
			{
				rbusValue_SetString(value, Port);
			}
			else
			{
				MqttCMError("Port is empty\n");
				char * mqtt_port = NULL;
				mqtt_port = (char *)malloc(sizeof(10));
				if(mqtt_port !=NULL)
				{
					snprintf(mqtt_port, 10, "%d",MQTT_PORT);
					rbusValue_SetString(value, mqtt_port);
					MQTTCM_FREE(mqtt_port);
				}
				else
				{
					MqttCMError("mqtt_port malloc failure\n");
				}
			}
		}
	}
        rbusProperty_SetValue(property, value);
        rbusValue_Release(value);

    }
    return RBUS_ERROR_SUCCESS;
}

int isSubscribeNeeded(char *compname)
{
	comp_topic_name_t* temp = g_head;
	while (temp != NULL)
	{
		if(strcmp(temp->compName, compname) == 0)
		{
			if(temp->subscribeOnFlag == 0)
			{
				MqttCMInfo("%s component needs to be subscribed\n", temp->compName);
				return 1;
			}
			else
			{
				return 0;
			}
		}
		temp = temp->next;
	}
	MqttCMInfo("Component is not in the list\n");
	return 2;
}

char* GetTopicFromSubcribeId(int subscribeId)
{
	comp_topic_name_t* temp = g_head;
	while (temp != NULL)
	{
		if(temp->subscribeId == subscribeId)
		{
			return (temp->compName);
		}
		temp = temp->next;
	}
	return NULL;
}

int UpdateSubscriptionIdToList(char *comp, int subscribeId)
{
	comp_topic_name_t* temp = g_head;
	while (temp != NULL)
	{
		if(strcmp(temp->compName, comp) == 0)
		{
			temp->subscribeId = subscribeId;
			temp->subscribeOnFlag = 1;
			MqttCMInfo("The component %s is subscribed to topic %s with subscribeId %d\n", temp->compName, temp->topic, subscribeId);
			return 1;
		}
		temp = temp->next;
	}
        return 0;
}

int printList()
{
	MqttCMInfo("Inside print function\n");
	comp_topic_name_t* current = g_head;
	while (current != NULL)
	{
		MqttCMInfo("compname is %s and topic is %s\n", current->compName, current->topic);
		current = current->next;
	}
	return 1;
}

int mqtt_subscribe(char *comp, char *topic)
{
	int rc;
	char temp_topic[30]={'\0'};
	if(topic != NULL && comp !=NULL)
	{
		int ret = isSubscribeNeeded(comp);
		//To Avoid resubscribe of the same component again
		if(ret == 0)
		{
			MqttCMInfo("Component is already subscribed, ret: %d\n", ret);
			return 0;
		}
		else if(ret == 2)
		{
			MqttCMInfo("Adding to subscribe list, ret: %d\n", ret);
			AddToSubscriptionList(comp ,topic, 1);
		}
		else
		{
			MqttCMInfo("Proceed to mosquitto_subscribe, ret:%d\n", ret);
		}

		//Adding int pointer subscribId in mosquitto_subscribe function to get the unique subscribeId which will be sent from cloud after subscription of each component
		if(strcmp (comp, SUBSCRIBE_WEBCONFIG) == 0)
		{
			int subscribeId;
			char *tempclient_id = NULL;
			tempclient_id = Get_Mqtt_ClientId();
			if( tempclient_id != NULL && strlen(tempclient_id) !=0 )
			{
				snprintf(temp_topic, sizeof(temp_topic), "%s%s/#", MQTT_SUBSCRIBE_TOPIC, tempclient_id);
				MQTTCM_FREE(tempclient_id);
				tempclient_id = NULL;
			}
			else
			{
				MqttCMError("Client id is NULL so not proceeding to subscribe\n");
				return 1;
			}
			MqttCMInfo("Subscribing to wildcard topic - %s\n", temp_topic);			
			rc = mosquitto_subscribe(mosq, &subscribeId, temp_topic, 1);

			if(rc != MOSQ_ERR_SUCCESS)
			{
				MqttCMError("Error subscribing: %s for %s\n", mosquitto_strerror(rc), comp);
				return 1;
			}

			comp_topic_name_t* temp = g_head;
			while (temp != NULL)
			{
				if(strcmp(temp->compName, SUBSCRIBE_WEBCONFIG) == 0)
				{
					MqttCMInfo("The subscribeId received from broker is %d\n", subscribeId);
					//Add the subscribeId to the list to create a mapping for each component subscribe
					UpdateSubscriptionIdToList(temp->compName, subscribeId);
					MqttCMDebug("Component is subscribed and added to the list\n");
				}
				else
				{
					MqttCMInfo("The subscribeId received from broker is -1\n");
					//Add the subscribeId to the list to create a mapping for each component subscribe
					UpdateSubscriptionIdToList(temp->compName, -1);
					MqttCMDebug("Component is subscribed and added to the list\n");
				}
				temp = temp->next;
			}
		}
		else if(webcfg_subscribed == 1)
		{
			UpdateSubscriptionIdToList(comp, -1); // since subscribeId is unknown using -ve value
			MqttCMDebug("Component is subscribed and added to the list\n");
		}
		else
		{
			MqttCMInfo("Webcfg is not subscribed, so pausing subscription of %s\n", comp);
		}
		return 0;
	}
	else
	{
		MqttCMError("Failed to subscribe as topic is NULL\n");
	}
	return 1;
}

int GetTopicFromFileandUpdateList()
{
	FILE* file;
	char content[256];

	file = fopen(MQTT_SUBSCRIBER_FILE, "r");
	if (file == NULL) {
		MqttCMInfo("%s is not present\n", MQTT_SUBSCRIBER_FILE);
		return 0;
	}

	char *compName = NULL;
	// Read the content from the file
	while (fgets(content, sizeof(content), file) != NULL)
	{
		char* delimiterPos = strchr(content, ':');
		if (delimiterPos != NULL)
		{
			*delimiterPos = '\0';  // Replace ':' with null character

			// Store the first part as compName
			compName = strdup(content);

			// Strip the newline character from topic value
			char* topic = delimiterPos + 1;
			if(topic != NULL)
			{
				size_t topicLength = strlen(topic);
				if (topic[topicLength - 1] == '\n')
				{
					topic[topicLength - 1] = '\0';
				}
			}

			AddToSubscriptionList(compName, topic, 0);

			if(compName != NULL)
			{
				MQTTCM_FREE(compName);
				compName = NULL;
			}
		}
	}

	fclose(file);
	return 1;
}

//Used to create a file for subscribed components with component name and topic, the file format is compName:topic
int AddSubscribeTopicToFile(char *compName, char *topic)
{
	FILE *fp;
	char str[256] = {'\0'};
	int ret = 0;
	fp = fopen(MQTT_SUBSCRIBER_FILE , "a+");
	if (fp == NULL)
	{
		MqttCMError("Could not open file %s\n", MQTT_SUBSCRIBER_FILE );
		return ret;
	}

	if((compName !=NULL) && (topic != NULL))
	{
		snprintf(str, sizeof(str), "%s:%s\n", compName, topic);
		fprintf(fp, "%s", str);
		MqttCMInfo("AddSubscribeTopicToFile: Added compName %s with topic %s\n", compName, topic);
		ret = 1;
	}
	else
	{
		MqttCMError("AddSubscribeTopicToFile failed as Compname or Topic is NULL\n");
		ret = 0;
	}

	if(fp != NULL)
	{
		fclose(fp);
	}

	return ret;
}
//writeflag to avoid duplicate entry in the subscriber local file
int AddToSubscriptionList(char *compName, char *topic, int writeFlag)
{
	if( (compName != NULL) && (topic != NULL) )
	{
		MqttCMInfo("The component name is %s and the topic is %s\n", compName, topic);
	}
	else
	{
		MqttCMError("The compName or topic is NULL\n");
		return 0;
	}

	//if component not present then add it to the list and set subscribe flag as calling function will proceed to subscribe
	comp_topic_name_t* newNode = (comp_topic_name_t*)malloc(sizeof(comp_topic_name_t));
	if(newNode)
	{
		memset(newNode, 0, sizeof(comp_topic_name_t) );
		strncpy(newNode->compName, compName, sizeof(newNode->compName) - 1);
		strncpy(newNode->topic, topic, sizeof(newNode->topic) - 1);
		newNode->subscribeOnFlag = 0;
		newNode->next = NULL;

		if(writeFlag)
		{
			AddSubscribeTopicToFile(newNode->compName, newNode->topic);
		}

		if(g_head == NULL)
		{
			g_head = newNode;
		}
		else
		{
			comp_topic_name_t* current = g_head;
			while (current->next != NULL)
			{
				current = current->next;
			}
			current->next = newNode;
		}
	}
	else
	{
		MqttCMError("Memory allocation failed\n");
		return 0;
	}

	return 1;
}

int regMqttDataModel()
{
	rbusError_t ret = RBUS_ERROR_SUCCESS;
	rbusDataElement_t dataElements[SINGLE_CONN_ELEMENTS] = {
		{MQTT_BROKER_PARAM, RBUS_ELEMENT_TYPE_PROPERTY, {MqttBrokerGetHandler, MqttBrokerSetHandler, NULL, NULL, NULL, NULL}},
		{MQTT_PORT_PARAM, RBUS_ELEMENT_TYPE_PROPERTY, {MqttPortGetHandler, MqttPortSetHandler, NULL, NULL, NULL, NULL}},
		{MQTT_LOCATIONID_PARAM, RBUS_ELEMENT_TYPE_PROPERTY, {MqttLocationIdGetHandler, MqttLocationIdSetHandler, NULL, NULL, NULL, NULL}},
		{MQTT_CONNECTMODE_PARAM, RBUS_ELEMENT_TYPE_PROPERTY, {MqttConnModeGetHandler, MqttConnModeSetHandler, NULL, NULL, NULL, NULL}},
		{MQTT_CONNSTATUS_PARAM, RBUS_ELEMENT_TYPE_PROPERTY, {MqttConnStatusGetHandler, NULL, NULL, NULL, NULL, NULL}},
                {MQTT_SUBSCRIBE_PARAM, RBUS_ELEMENT_TYPE_METHOD, {NULL, NULL, NULL, NULL, NULL, MqttSubscribeMethodHandler}},
		{MQTT_PUBLISH_PARAM, RBUS_ELEMENT_TYPE_METHOD, {NULL, NULL, NULL, NULL, NULL, MqttPublishMethodHandler}}
	};

	ret = rbus_regDataElements(get_global_rbus_handle(), SINGLE_CONN_ELEMENTS, dataElements);
	if(ret == RBUS_ERROR_SUCCESS)
	{
		MqttCMInfo("regMqttDataModel success %s,%s\n", MQTT_BROKER_PARAM, MQTT_PORT_PARAM);
		//Adding mqttcm integration in the initial phase and webconfig integration will be done later, so disabling webconfig tr181s
		rbusRegWebcfgDataElements();
		fetchMqttParamsFromDB();
	}
	else
	{
		MqttCMError("Failed to register rbus data model ret %d\n", ret);
	}
	return ret;
}

int writeToDBFile(char *db_file_path, char *data, size_t size)
{
	FILE *fp;
	fp = fopen(db_file_path , "w+");
	if (fp == NULL)
	{
		MqttCMError("Failed to open file in db %s\n", db_file_path );
		return 0;
	}
	if(data !=NULL)
	{
		fwrite(data, size, 1, fp);
		fclose(fp);
		return 1;
	}
	else
	{
		MqttCMInfo("WriteToJson failed, Data is NULL\n");
		fclose(fp);
		return 0;
	}
}

void get_interface(char **interface)
{
#if ! defined(DEVICE_EXTENDER)
        FILE *fp = fopen(DEVICE_PROPS_FILE, "r");

        if (NULL != fp)
        {
                char str[255] = {'\0'};
                while (fgets(str, sizeof(str), fp) != NULL)
                {
                    char *value = NULL;

                    if(NULL != (value = strstr(str, "WEBCONFIG_INTERFACE=")))
                    {
                        value = value + strlen("WEBCONFIG_INTERFACE=");
                        value[strlen(value)-1] = '\0';
                        *interface = strdup(value);
                        break;
                    }

                }
                fclose(fp);
        }
        else
        {
                MqttCMError("Failed to open device.properties file:%s\n", DEVICE_PROPS_FILE);
        }

        if (NULL == *interface)
        {
                MqttCMError("Interface is not present in device.properties\n");

        }
        else
        {
                MqttCMDebug("interface fetched is %s\n", *interface);
        }
#endif
}
