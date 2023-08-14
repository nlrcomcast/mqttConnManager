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

#include <rbus.h>
#include <cJSON.h>
#include "mqttcm_webcfg.h"
#include "mqttcm_log.h"
#include "mqttcm_connect.h"
#include "mqttcm_generic.h"

int webcfg_subscribe = 0;
int webcfg_onmessage = 0;
int webcfg_onpublish = 0;

void sendRbusEventWebcfgOnSubscribe()
{
	if(webcfg_subscribe)
	{
		rbusEvent_t event = {0};
		rbusObject_t data;
		rbusValue_t value;

		MqttCMInfo("publishing Event\n");

		rbusValue_Init(&value);
		rbusValue_SetString(value, "success");

		rbusObject_Init(&data, NULL);
		rbusObject_SetValue(data, "value", value);

		event.name = WEBCFG_MQTT_SUBSCRIBE_CALLBACK;
		event.data = data;
		event.type = RBUS_EVENT_GENERAL;

		rbusError_t rc = rbusEvent_Publish(get_global_rbus_handle(), &event);

		rbusValue_Release(value);
		rbusObject_Release(data);

		if(rc != RBUS_ERROR_SUCCESS)
			MqttCMError("provider: rbusEvent_Publish onsubscribe event failed: %d\n", rc);
	}
}

void sendRbusEventWebcfgOnMessage(char *mqttdata, int dataSize, char *topic_name)
{
		rbusEvent_t event = {0};
		rbusObject_t dataIn;
		rbusValue_t value;

		MqttCMInfo("publishing onmessafe event1\n");

		rbusValue_Init(&value);
		rbusValue_SetBytes(value, (uint8_t*)mqttdata, dataSize);

		rbusObject_Init(&dataIn, NULL);
		rbusObject_SetValue(dataIn, "value", value);

		writeToDBFile("/tmp/mqtt_poke.bin",(char *)mqttdata, dataSize);

		event.name = WEBCFG_MQTT_ONMESSAGE_CALLBACK;
		event.data = dataIn;
		event.type = RBUS_EVENT_GENERAL;

		rbusError_t rc = rbusEvent_Publish(get_global_rbus_handle(), &event);

		rbusValue_Release(value);
		rbusObject_Release(dataIn);

		if(rc != RBUS_ERROR_SUCCESS)
		{
			MqttCMError("provider: rbusEvent_Publish onmessage event failed: %d\n", rc);
			sendRbusErrorToMqtt(rc,topic_name);
		}
}

void sendRbusEventWebcfgOnPublish(int mid)
{
	if(webcfg_onpublish)
	{
		char msg[256] = { 0 };
		rbusEvent_t event = {0};
		rbusObject_t data;
		rbusValue_t value;

		snprintf(msg, MAX_MQTT_LEN, "Message with mid %d has been published.", mid);

		MqttCMInfo("publishing Event\n");

		rbusValue_Init(&value);
		rbusValue_SetString(value, msg);

		rbusObject_Init(&data, NULL);
		rbusObject_SetValue(data, "value", value);

		event.name = WEBCFG_MQTT_ONPUBLISH_CALLBACK;
		event.data = data;
		event.type = RBUS_EVENT_GENERAL;

		rbusError_t rc = rbusEvent_Publish(get_global_rbus_handle(), &event);

		rbusValue_Release(value);
		rbusObject_Release(data);

		if(rc != RBUS_ERROR_SUCCESS)
			MqttCMError("provider: rbusEvent_Publish onpublish event failed: %d\n", rc);
	}
}

void sendRbusErrorToMqtt(rbusError_t rc, char *topic_name)
{
	char topic_str[256] = { 0 };
	char locationID[64] = { 0 };
	static char g_ClientID[64] = { 0 };
	char *payload = NULL;
	ssize_t payload_len = 0;
	if(topic_name == NULL)
	{
		MqttCMError("topic name is NULL for sendRbusErrorToMqtt\n");
		return;	
	}
	const char *module = getComponentFromTopicName(topic_name);
	Get_Mqtt_LocationId(locationID);
	if( Get_Mqtt_ClientId() != NULL && strlen(Get_Mqtt_ClientId()) !=0 )
	{
	      strncpy(g_ClientID, Get_Mqtt_ClientId(), sizeof(g_ClientID)-1);
	      MqttCMDebug("g_ClientID fetched from Get_Mqtt_ClientId is %s\n", g_ClientID);
	}
	snprintf(topic_str, MAX_MQTT_LEN, "%s%s/%s/%s/poke", MQTT_PUBLISH_NOTIFY_TOPIC_PREFIX, g_ClientID,locationID,module);
	
	payload = createcJsonSchema(rc,topic_name);
	char * pub_payload = createMqttPubHeader(payload, &payload_len);
	MqttCMInfo("topic_str is:%s,payload_len:%zu\n", topic_str,payload_len);
	MqttCMInfo("publish_error_payload is:%s\n", pub_payload);	
	publish_notify_mqtt(topic_str, pub_payload, payload_len);
}

char * createMqttPubHeader(char * payload, ssize_t * payload_len)
{
	char * content_type = NULL;
	char * content_length = NULL;
	char *pub_headerlist = NULL;

	pub_headerlist = (char *) malloc(sizeof(char) * 1024);

	if(pub_headerlist != NULL)
	{
		if(payload != NULL)
		{
			content_type = (char *) malloc(sizeof(char)*MAX_BUF_SIZE);
			if(content_type !=NULL)
			{
				snprintf(content_type, MAX_BUF_SIZE, "Content-type: application/json");
				MqttCMDebug("content_type formed %s\n", content_type);
			}

			content_length = (char *) malloc(sizeof(char)*MAX_BUF_SIZE);
			if(content_length !=NULL)
			{
				snprintf(content_length, MAX_BUF_SIZE, "\r\nContent-length: %zu", strlen(payload));
				MqttCMDebug("content_length formed %s\n", content_length);
			}

			MqttCMInfo("Framing publish notification header\n");
			snprintf(pub_headerlist, 1024, "%s%s\r\n\r\n%s\r\n", (content_type!=NULL)?content_type:"", (content_length!=NULL)?content_length:"",(payload!=NULL)?payload:"");
	    }
	}
	MqttCMInfo("mqtt pub_headerlist is \n%s", pub_headerlist);
	*payload_len = strlen(pub_headerlist);
	return pub_headerlist;
}

char * createcJsonSchema(rbusError_t rc, char *topic_name)
{
	char device_id[32] = { '\0' };	
	cJSON *notifyPayload = NULL;
	char  * stringifiedNotifyPayload = NULL;	
	notifyPayload = cJSON_CreateObject();
	if(notifyPayload != NULL)
	{
		snprintf(device_id, sizeof(device_id), "mac:%s", Get_Mqtt_ClientId());
		MqttCMDebug("webconfig Device_id %s\n", device_id);
		cJSON_AddStringToObject(notifyPayload,"device_id", device_id);
		cJSON_AddStringToObject(notifyPayload,"topicname", topic_name);
		cJSON_AddNumberToObject(notifyPayload,"error_code", rc);
		if(rc == 23)
			cJSON_AddStringToObject(notifyPayload,"error_details", "component is not found");
		else
			cJSON_AddStringToObject(notifyPayload,"error_details", "unknown error");	
		stringifiedNotifyPayload = cJSON_PrintUnformatted(notifyPayload);
		cJSON_Delete(notifyPayload);
		MqttCMDebug("stringifiedNotifyPayload is %s\n", stringifiedNotifyPayload);
		return stringifiedNotifyPayload;
	}
	return NULL;
}

rbusError_t webcfgMqttSubscribeHandler(rbusHandle_t handle, rbusEventSubAction_t action, const char* eventName, rbusFilter_t filter, int32_t interval, bool* autoPublish)
{
    (void)handle;
    (void)filter;
    (void)autoPublish;
    (void)interval;

    MqttCMInfo(
        "webcfgMqttSubscribeHandler called:\n" \
        "\taction=%s\n" \
        "\teventName=%s\n",
        action == RBUS_EVENT_ACTION_SUBSCRIBE ? "subscribe" : "unsubscribe",
        eventName);

    if(!strcmp(WEBCFG_MQTT_SUBSCRIBE_CALLBACK, eventName))
    {
        webcfg_subscribe = action == RBUS_EVENT_ACTION_SUBSCRIBE ? 1 : 0;
    }
    else
    {
        MqttCMError("provider: webcfgMqttSubscribeHandler unexpected eventName %s\n", eventName);
    }

    return RBUS_ERROR_SUCCESS;
}

rbusError_t webcfgMqttOnMessageHandler(rbusHandle_t handle, rbusEventSubAction_t action, const char* eventName, rbusFilter_t filter, int32_t interval, bool* autoPublish)
{
    (void)handle;
    (void)filter;
    (void)autoPublish;
    (void)interval;

    MqttCMInfo(
        "webcfgMqttOnMessageHandler called:\n" \
        "\taction=%s\n" \
        "\teventName=%s\n",
        action == RBUS_EVENT_ACTION_SUBSCRIBE ? "subscribe" : "unsubscribe",
        eventName);

    if(!strcmp(WEBCFG_MQTT_ONMESSAGE_CALLBACK, eventName))
    {
        webcfg_onmessage = action == RBUS_EVENT_ACTION_SUBSCRIBE ? 1 : 0;
    }
    else
    {
        MqttCMError("provider: webcfgMqttOnMessageHandler unexpected eventName %s\n", eventName);
    }

    return RBUS_ERROR_SUCCESS;
}

rbusError_t webcfgMqttOnPublishHandler(rbusHandle_t handle, rbusEventSubAction_t action, const char* eventName, rbusFilter_t filter, int32_t interval, bool* autoPublish)
{
    (void)handle;
    (void)filter;
    (void)autoPublish;
    (void)interval;

    MqttCMInfo(
        "webcfgMqttOnPublishHandler called:\n" \
        "\taction=%s\n" \
        "\teventName=%s\n",
        action == RBUS_EVENT_ACTION_SUBSCRIBE ? "subscribe" : "unsubscribe",
        eventName);

    if(!strcmp(WEBCFG_MQTT_ONPUBLISH_CALLBACK, eventName))
    {
        webcfg_onpublish = action == RBUS_EVENT_ACTION_SUBSCRIBE ? 1 : 0;
    }
    else
    {
        MqttCMError("provider: webcfgMqttOnPublishHandler unexpected eventName %s\n", eventName);
    }

    return RBUS_ERROR_SUCCESS;
}

int rbusRegWebcfgDataElements()
{
	rbusError_t ret = RBUS_ERROR_SUCCESS;
	rbusDataElement_t webcfgDataElements[WEBCFG_ELEMENTS] = {
		{WEBCFG_MQTT_SUBSCRIBE_CALLBACK, RBUS_ELEMENT_TYPE_EVENT, {NULL, NULL, NULL, NULL, webcfgMqttSubscribeHandler, NULL}},
		{WEBCFG_MQTT_ONMESSAGE_CALLBACK, RBUS_ELEMENT_TYPE_EVENT, {NULL, NULL, NULL, NULL, webcfgMqttOnMessageHandler, NULL}},
		{WEBCFG_MQTT_ONPUBLISH_CALLBACK, RBUS_ELEMENT_TYPE_EVENT, {NULL, NULL, NULL, NULL, webcfgMqttOnPublishHandler, NULL}},
	};

	ret = rbus_regDataElements(get_global_rbus_handle(), WEBCFG_ELEMENTS, webcfgDataElements);
	if(ret == RBUS_ERROR_SUCCESS)
	{
		MqttCMInfo("Registered webcfg rbus callback events\n");
	}
	else
	{
		MqttCMError("Failed to register webcfg rbus callback events %d\n", ret);
	}
	return ret;
}
