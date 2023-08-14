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

// Test function for update_mqtt_delay
void test_update_mqtt_delay(void)
{
    mqtt_timer_t timer;
    timer.max_count = 4;
    timer.count = 0;
    timer.delay = 3;

    unsigned result = update_mqtt_delay(&timer);
    CU_ASSERT_EQUAL(result, 7);
    result = update_mqtt_delay(&timer);
    CU_ASSERT_EQUAL(result, 15);
}

// Test function for mqtt_rand_secs
void test_mqtt_rand_secs_success(void)
{
    int random_num = 5;
    unsigned max_secs = 10;
    unsigned result = mqtt_rand_secs(random_num, max_secs);

    CU_ASSERT_EQUAL(result, 3);
}

void test_mqtt_rand_secs_failure(void)
{
    int random_num = 15;
    unsigned max_secs = 10;
    unsigned result = mqtt_rand_secs(random_num, max_secs);

    CU_ASSERT_EQUAL(result, 10);
}

// Test function for mqtt_rand_secs
void test_mqtt_rand_nsecs_input_lessthan_1000000000(void)
{
    int random_num = 999999999;
    unsigned result = mqtt_rand_nsecs(random_num);
    CU_ASSERT_EQUAL(result, 499999999);

    random_num = 0;
    result = mqtt_rand_nsecs(random_num);
    CU_ASSERT_EQUAL(result, 0);

    random_num = 2;
    result = mqtt_rand_nsecs(random_num);
    CU_ASSERT_EQUAL(result, 1);
}

void test_mqtt_rand_nsecs_input_greaterorequal_1000000000(void)
{
    int random_num = 1000000000;
    unsigned result = mqtt_rand_nsecs(random_num);
    CU_ASSERT_EQUAL(result, 500000000);

    random_num = 2000000000;
    result = mqtt_rand_nsecs(random_num);
    CU_ASSERT_EQUAL(result, 0);

    random_num = 2000000002;
    result = mqtt_rand_nsecs(random_num);
    CU_ASSERT_EQUAL(result, 1);
}

// Test function for mqtt_add_timespec
void test_mqtt_add_timespec_positive(void)
{
    struct timespec t1 = {1, 500000000};
    struct timespec t2 = {2, 300000000};
    struct timespec expected_result = {3, 800000000};

    mqtt_add_timespec(&t1, &t2);
    CU_ASSERT_EQUAL(t2.tv_sec, expected_result.tv_sec);
    CU_ASSERT_EQUAL(t2.tv_nsec, expected_result.tv_nsec);
}

void test_mqtt_add_timespec_carryover(void)
{
    struct timespec t1 = {1, 800000000};
    struct timespec t2 = {2, 500000000};
    struct timespec expected_result = {4, 300000000};

    mqtt_add_timespec(&t1, &t2);
    CU_ASSERT_EQUAL(t2.tv_sec, expected_result.tv_sec);
    CU_ASSERT_EQUAL(t2.tv_nsec, expected_result.tv_nsec);
}

void test_mqtt_add_timespec_nanosecond_carryover(void)
{
    struct timespec t1 = {0, 900000000};
    struct timespec t2 = {0, 200000000};
    struct timespec expected_result = {1, 100000000};

    mqtt_add_timespec(&t1, &t2);
    CU_ASSERT_EQUAL(t2.tv_sec, expected_result.tv_sec);
    CU_ASSERT_EQUAL(t2.tv_nsec, expected_result.tv_nsec);
}

// Test function for mqtt_rand_expiration
void test_mqtt_rand_expiration(void)
{
    int random_num1 = 5;
    int random_num2 = 10;
    mqtt_timer_t timer;
    timer.count = 0;
    timer.max_count = 31;
    timer.delay = 3;

    struct timespec ts;
    ts.tv_sec = 100;
    ts.tv_nsec = 500000000;

    mqtt_rand_expiration(random_num1, random_num2, &timer, &ts);
    CU_ASSERT(ts.tv_sec > 100);
    CU_ASSERT(ts.tv_nsec >= 0 && ts.tv_nsec < 1000000000);
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
    CU_add_test( *suite, "test update_mqtt_delay", test_update_mqtt_delay);
    CU_add_test( *suite, "test mqtt_rand_secs_success", test_mqtt_rand_secs_success);
    CU_add_test( *suite, "test mqtt_rand_secs_failure", test_mqtt_rand_secs_failure);
    CU_add_test( *suite, "test mqtt_rand_nsecs_input_lessthan_1000000000", test_mqtt_rand_nsecs_input_lessthan_1000000000);
    CU_add_test( *suite, "test mqtt_rand_nsecs_input_greaterorequal_1000000000", test_mqtt_rand_nsecs_input_greaterorequal_1000000000);
    CU_add_test( *suite, "test mqtt_add_timespec_positive", test_mqtt_add_timespec_positive);
    CU_add_test( *suite, "test mqtt_add_timespec_carryover", test_mqtt_add_timespec_carryover);
    CU_add_test( *suite, "test mqtt_add_timespec_nanosecond_carryover", test_mqtt_add_timespec_nanosecond_carryover);
    CU_add_test( *suite, "test mqtt_rand_expiration", test_mqtt_rand_expiration);
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