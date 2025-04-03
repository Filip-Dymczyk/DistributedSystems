#pragma once
#include <ArduinoMqttClient.h>
#include <Arduino_FreeRTOS.h>
#include <WiFiS3.h>
#include "data_manager.h"

class TasksManager
{
    static constexpr TickType_t mqtt_poll_delay = pdMS_TO_TICKS(100u); // Convert ms to ticks.
    static constexpr TickType_t connection_alive_delay = pdMS_TO_TICKS(10u * 60u * 1000u); // Convert ms to ticks.
public:
    void
    run();

private:
    static DataManager m_data_manager;
    static WiFiClient m_wifi_client;
    static MqttClient m_mqtt_client;

    static SemaphoreHandle_t m_parse_data_semaphore;
    static SemaphoreHandle_t m_data_send_semaphore;
    static SemaphoreHandle_t m_mqtt_client_mutex;

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
};