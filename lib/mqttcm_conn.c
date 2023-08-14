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

#include <stdio.h>
#include <rbus/rbus.h>
#include <rbus/rbus_object.h>
#include <rbus/rbus_property.h>
#include <rbus/rbus_value.h>
#include <zlib.h>

#include "mqttcm_conn.h"

#define MQTT_COMPONENT_NAME "mqttConnManager"

static rbusHandle_t rbus_handle;

static rbusHandle_t get_global_rbus_handle(void);

int mqttcm_conn_init()
{   
	rbusError_t ret = RBUS_ERROR_BUS_ERROR;

	ret = rbus_open(&rbus_handle,"MQTT_COMPONENT_NAME");
	if (ret != RBUS_ERROR_SUCCESS)
	{
		MqttCMError("%s: Rbus open failed for MQTTCM from  MqttCM lib, err=%d\n",__func__,ret);
		return 0;
	}
	return 1;
}

static rbusHandle_t get_global_rbus_handle(void)
{
	return rbus_handle;
}

static bool mqttcm_conn_msg_publish(void *msg, char *topic, char *qos, long msg_len)
{
	bool ret = false;
	rbusObject_t inParams;
	rbusObject_t outParams;
	rbusValue_t  value;
	rbusError_t err = RBUS_ERROR_BUS_ERROR;

	rbusObject_Init(&inParams, NULL);
	rbusObject_Init(&outParams, NULL);

	rbusValue_Init(&value);
	rbusValue_SetBytes(value, msg, msg_len);
	rbusObject_SetValue(inParams, "payload", value);
	rbusValue_Release(value);

	rbusValue_Init(&value);
	rbusValue_SetString(value, topic);
	rbusObject_SetValue(inParams, "topic", value);
	rbusValue_Release(value);

	rbusValue_Init(&value);
	rbusValue_SetString(value, qos);
	rbusObject_SetValue(inParams, "qos", value);
	rbusValue_Release(value);

	err = rbusMethod_Invoke(rbus_handle,MQTT_PUBLISH_PARAM,inParams,&outParams);
	if (err == RBUS_ERROR_SUCCESS)
	{
		MqttCMInfo("%s: Message published to broker successfully\n",__func__);
		ret = true;
	}
	else {
		MqttCMError("%s: Message published to broker failed\n",__func__);
	}

	rbusObject_Release(inParams);
	rbusObject_Release(outParams);

	return ret;
}

//To fetch mqttcm broker connection status
static int mqttcm_conn_status_get()
{
	rbusValue_t value = NULL;
	char *status = NULL;
	int ret = 0;
        rbusError_t  rc = RBUS_ERROR_BUS_ERROR;

	// Get global rbus handle status
	if(get_global_rbus_handle() != NULL)
	{
		rc = rbus_get(get_global_rbus_handle(), MQTT_CONNSTATUS_PARAM, &value);

		if(rc == RBUS_ERROR_SUCCESS)
		{
			status = (char *)rbusValue_GetString(value, NULL);
			if(status !=NULL)
			{
				MqttCMInfo("%s: MqttCM connection status fetched is %s\n",__func__, status);
				if(strncmp(status,  "Up", 2) == 0)
				{
					ret = 1;
				}
			}
			else
			{
				MqttCMError("%s: MqttCM connect status is NULL\n",__func__);
			}
		}
		else
		{
			MqttCMError("%s: MqttCM connect status rbus_get failed, rc %d\n",__func__, rc);
		}
	}
	else
	{
		MqttCMError("%s: Geting global rbus handle status for MqttCM failed\n",__func__);
	}
	return ret;
}

int mqttcm_conn_msg_process(void *msg, long mesg_len, bool do_compress, char *topic, char *qos)
{
	int rc;	
	bool is_published = false;
	static unsigned long buflen = 0;
	static unsigned char *buf = NULL;
	long len;
	int ret;

    // Get Mqttcm connection status, proceed if connection is alive
	rc = mqttcm_conn_status_get();

	if(rc !=0)
	{
		MqttCMInfo("%s: MqttCM connection is Up; Ready for message publishing\n",__func__);
                // Compress the messages received from different components
		if(do_compress)
		{
			len = compressBound(mesg_len);
			if (len > (long)buflen)
			{
				// allocate 1/4 more than needed
				buflen = len + len / 4;
				buf = realloc(buf, buflen);

				if (!buf)
				{
					MqttCMError("%s: Memory allocation for Compressing messages are failed",__func__);
					return is_published;
				}
			}
			len = buflen;
			MqttCMInfo("%s: Compressed buffer length is %ld",__func__,len);
			ret = compress(buf, (unsigned long*)&len, msg, mesg_len);
			if (ret != Z_OK)
			{
				MqttCMError("%s: Compression of messages are failed from MqttCM %d",__func__,ret);
				return is_published;
			}
			else
			{
				MqttCMInfo("%s: Messages are compressed succesfully",__func__);
				MqttCMInfo("%s: Publishing uncompressed: %ld compressed: %ld reduction: %d%%",__func__, mesg_len, len, (int)(100 - 100 * len / mesg_len));
			}
			msg = buf;
			mesg_len = len;
		}
                // Publish the messages to MQTTCM via rbus
		is_published = mqttcm_conn_msg_publish(msg,topic,qos,mesg_len);
	}
	else {
		MqttCMError("%s: MqttCM connection is down; Drop/Wait for connection to up\n",__func__);
	}

	return is_published;
}

int mqttcm_conn_finish()
{
	rbusError_t ret = RBUS_ERROR_BUS_ERROR;

	ret = rbus_close(rbus_handle);
	if (ret != RBUS_ERROR_SUCCESS)
	{
		MqttCMError("%s: Rbus close failed for MQTTCM from  MqttCM lib, err=%d\n",__func__,ret);
		return 0;
	}
	return 1;
}
