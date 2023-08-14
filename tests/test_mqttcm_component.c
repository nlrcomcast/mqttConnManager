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
#include <string.h>
#include <pthread.h>
#include <rbus/rbus.h>
#include <stdlib.h>
#include <ifaddrs.h>
#include <CUnit/Basic.h>
#include <sys/stat.h>
#include "../src/mqttcm_connect.h"
#include "../src/mqttcm_generic.h"
#include "../src/mqttcm_generic.h"
#include "../src/mqttcm_log.h"
#include "../src/mqttcm_privilege.h"
#include "../src/mqttcm_webcfg.h"



/*----------------------------------------------------------------------------*/
/*                             Test Functions                             */
/*----------------------------------------------------------------------------*/


// Test function for getComponentFromTopicName
void test_getComponentFromTopicName_Success(void)
{
    AddToSubscriptionList("Component1", "Topic1",1);
    AddToSubscriptionList("Component2", "Topic2",1);

    const char* compName1 = getComponentFromTopicName("Topic1");
    const char* compName2 = getComponentFromTopicName("Topic2");

    CU_ASSERT_STRING_EQUAL(compName1, "Component1");
    CU_ASSERT_STRING_EQUAL(compName2, "Component2");
}

void test_getComponentFromTopicName_Failure(void)
{
    AddToSubscriptionList("Component1", "Topic1",1);

    const char* compName4 = getComponentFromTopicName("Topic4");
    CU_ASSERT_PTR_NULL(compName4);
}

// Test function for AddToSubscriptionList
void test_AddToSubscriptionList_Success(void)
{
    int result = AddToSubscriptionList("Component1", "Topic1",1);
    CU_ASSERT_EQUAL(result, 1);
}

void test_AddToSubscriptionList_MultipleAdditions(void)
{
    int result1 = AddToSubscriptionList("Component1", "Topic1",1);
    int result2 = AddToSubscriptionList("Component2", "Topic2",1);
    int result3 = AddToSubscriptionList("Component3", "Topic3",1);

    CU_ASSERT_EQUAL(result1, 1);
    CU_ASSERT_EQUAL(result2, 1);
    CU_ASSERT_EQUAL(result3, 1);
}

void test_AddToSubscriptionList_ComponentAlreadyExists(void)
{
    int result1 = AddToSubscriptionList("Component1", "Topic1",1);
    int result2 = AddToSubscriptionList("Component1", "Topic2",1);

    CU_ASSERT_EQUAL(result1, 1);
    CU_ASSERT_EQUAL(result2, 1);
}

// Test function for isSubscribeNeeded
void test_isSubscribeNeeded_Success(void)
{
    UpdateSubscriptionIdToList("Component1",-1);
    int result = isSubscribeNeeded("Component1");
    CU_ASSERT_EQUAL(result, 0);
}

void test_isSubscribeNeeded_Failure(void)
{
    int result = isSubscribeNeeded("Component2");
    CU_ASSERT_EQUAL(result, 1);
}

// Test function for GetTopicFromSubcribeId
void test_GetTopicFromSubcribeId_Success(void)
{
    UpdateSubscriptionIdToList("Component1", 1);
    UpdateSubscriptionIdToList("Component2", 2);
    char* result = GetTopicFromSubcribeId(1);
    CU_ASSERT_STRING_EQUAL(result, "Component1");
    result = GetTopicFromSubcribeId(2);
    CU_ASSERT_STRING_EQUAL(result, "Component2");
}

void test_GetTopicFromSubcribeId_Failure(void)
{
    char* result = GetTopicFromSubcribeId(3);
    CU_ASSERT_PTR_NULL(result);
}

// Test function for UpdateSubscriptionIdToList
void test_UpdateSubscriptionIdToList_Success(void)
{
    int result = UpdateSubscriptionIdToList("Component1", 1);
    CU_ASSERT_EQUAL(result, 1);
}

void test_UpdateSubscriptionIdToList_Failure(void)
{
    int result = UpdateSubscriptionIdToList("NoComponent", 1);
    CU_ASSERT_EQUAL(result, 0);
}


int init_suite(void)
{
    // Initialize any necessary resources or setups
    return 0;
}

// Test suite cleanup function

int clean_suite(void)
{
    // Clean up any allocated resources or memory
    return 0;
}

void add_suites( CU_pSuite *suite )
{
    *suite = CU_add_suite( "tests", NULL, NULL );
    CU_add_test( *suite, "test getComponentFromTopicName_Success", test_getComponentFromTopicName_Success);
    CU_add_test( *suite, "test getComponentFromTopicName_Failure", test_getComponentFromTopicName_Failure);
    CU_add_test( *suite, "test AddToSubscriptionList_Success", test_AddToSubscriptionList_Success);
    CU_add_test( *suite, "test AddToSubscriptionList_MultipleAdditions", test_AddToSubscriptionList_MultipleAdditions);
    CU_add_test( *suite, "test AddToSubscriptionList_ComponentAlreadyExists", test_AddToSubscriptionList_ComponentAlreadyExists);
    CU_add_test( *suite, "test isSubscribeNeeded_Success", test_isSubscribeNeeded_Success);
    CU_add_test( *suite, "test isSubscribeNeeded_Failure", test_isSubscribeNeeded_Failure);
    CU_add_test( *suite, "test GetTopicFromSubcribeId_Success", test_GetTopicFromSubcribeId_Success);
    CU_add_test( *suite, "test GetTopicFromSubcribeId_Failure", test_GetTopicFromSubcribeId_Failure);
    CU_add_test( *suite, "test UpdateSubscriptionIdToList_Success", test_UpdateSubscriptionIdToList_Success);
    CU_add_test( *suite, "test UpdateSubscriptionIdToList_Failure", test_UpdateSubscriptionIdToList_Failure);
}

int main( int argc, char *argv[] )
{
    unsigned rv = 1;
    CU_pSuite suite = NULL;

    (void ) argc;
    (void ) argv;

    if( CUE_SUCCESS == CU_initialize_registry() ) {
        add_suites( &suite );

        if( NULL != suite ) {
            CU_basic_set_mode( CU_BRM_VERBOSE );
            CU_basic_run_tests();
            printf( "\n" );
            CU_basic_show_failures( CU_get_failure_list() );
            printf( "\n\n" );
            rv = CU_get_number_of_tests_failed();
        }

        CU_cleanup_registry();

    }

    return rv;
}
