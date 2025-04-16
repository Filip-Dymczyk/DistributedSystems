#pragma once
#include <ArduinoMqttClient.h>
#include <Arduino_FreeRTOS.h>
#include <WiFiS3.h>
#include "data_manager.h"
#include "my_sql_manager.h"

class TasksManager
{
    static constexpr TickType_t mqtt_poll_delay        = pdMS_TO_TICKS(100u);               // Convert ms to ticks.
    static constexpr TickType_t connection_alive_delay = pdMS_TO_TICKS(2u * 60u * 1000u);  // Convert ms to ticks.
public:
    static void
    run();

private:
    static DataManager m_data_manager;
    static WiFiClient m_wifi_client_mqtt;
    static MqttClient m_mqtt_client;
    static WiFiClient m_wifi_client_mysql;
    static MySQLManager m_my_sql_manager;

    static SemaphoreHandle_t m_parse_data_semaphore;
    static SemaphoreHandle_t m_data_send_semaphore;
    static SemaphoreHandle_t m_mqtt_client_mutex;

    static TaskHandle_t m_connect_task_handle;
    static TaskHandle_t m_mqtt_poll_task_handle;
    static TaskHandle_t m_data_parse_task_handle;
    static TaskHandle_t m_data_send_task_handle;
    static TaskHandle_t m_check_connections_task_handle;

    static bool const retained   = false;
    static bool const dup        = false;
    static int const publish_Qos = 1;

    static void
    connect_task(void* pvParameters);

    static void
    mqtt_poll_task(void* pvParameters);

    static void
    data_parse_task(void* pvParameters);

    static void
    data_send_task(void* pvParameters);

    static void
    on_message_received(int message_size);

    static void
    check_connections_task(void* pvParameters);

    static void
    send_alive_msg();

    static void
    suspend_tasks();

    static void
    reset_connections();

    static void
    initialize_synchronization_items();

    static void
    recovery_task(void* pvParameters);

    static bool
    give_semaphore_safe(SemaphoreHandle_t semaphore);

    static bool
    take_semaphore_safe(SemaphoreHandle_t semaphore);
};