#include <stdio.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>
#include <unity.h>
#include "fifo.h"

#define TEST_TASK_PRIORITY      ( tskIDLE_PRIORITY + 1UL )
#define TEST_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define TEST_RUNNER_PRIORITY      ( tskIDLE_PRIORITY + 2UL )
#define TEST_RUNNER_STACK_SIZE configMINIMAL_STACK_SIZE

int setup_pool;

TaskHandle_t supervisor_thread;
TaskHandle_t worker_threads[THREAD_COUNT];
struct task_args worker_args[THREAD_COUNT];
QueueHandle_t request;
QueueHandle_t response;

void test_full(void)
{
    printf("Sending messages\n");
    for (int i = 0; i < 32; i++) {
        struct request_msg data = {};
        data.input = i;
        xQueueSendToBack(request, &data, portMAX_DELAY);
    }

    for (int i = 0; i < 32; i++) {
        struct request_msg data = {};
        BaseType_t res = xQueueReceive(response, &data, 1000);
        TEST_ASSERT_EQUAL_INT(pdTRUE, res);
        printf("Got result %d for %d, handled by thread %d\n",
               data.input, data.output, data.handled_by);
        TEST_ASSERT_EQUAL_INT(data.input + 5, data.output);
    }

    printf("Done\n");
}

void test_single(void)
{
    struct request_msg data = {42, -1, 0};
    xQueueSendToBack(request, &data, portMAX_DELAY);
    TEST_ASSERT_EQUAL_INT(pdTRUE, xQueueReceive(response, &data, portMAX_DELAY));
    TEST_ASSERT_EQUAL_INT(data.input, 42);
    TEST_ASSERT_EQUAL_INT(data.input + 5, data.output);

    TEST_ASSERT_EQUAL_INT(pdFALSE, xQueueReceive(response, &data, 1000));
}

void test_nothing(void)
{
    struct request_msg data = {42, -1, 0};
    xQueueSendToBack(request, &data, portMAX_DELAY);
    xQueueReceive(response, &data, portMAX_DELAY);
    TEST_ASSERT_EQUAL_INT(data.input, 42);
    TEST_ASSERT_EQUAL_INT(data.input + 5, data.output);
    TEST_ASSERT_EQUAL_INT(pdFALSE, xQueueReceive(response, &data, 1000));
}

void test_all_alone(void)
{
    // Note this test should run with no thread pool.
    struct request_msg data = {42, -1, 0};
    xQueueSendToBack(request, &data, portMAX_DELAY);
    TEST_ASSERT_EQUAL_INT(pdFALSE, xQueueReceive(response, &data, 1000));
}


void runner_thread (__unused void *args)
{
    for (;;) {
        printf("Starting test run\n");
        setup_pool = 1;
        UNITY_BEGIN();
        RUN_TEST(test_full);
        RUN_TEST(test_nothing);
        RUN_TEST(test_single);
        setup_pool = 0;
        RUN_TEST(test_all_alone);
        UNITY_END();
        sleep_ms(10000);
    }
}

int main (void)
{
    stdio_init_all();
    hard_assert(cyw43_arch_init() == PICO_OK);
    sleep_ms(10000);
    printf("Launching runner\n");
    xTaskCreate(runner_thread, "TestRunner",
                TEST_RUNNER_STACK_SIZE, NULL, TEST_RUNNER_PRIORITY, NULL);
    vTaskStartScheduler();
	return 0;
}