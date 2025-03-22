#pragma once
#include <ArduinoMqttClient.h>
#include <Arduino_FreeRTOS.h>
#include <WiFiS3.h>
#include "data_manager.h"

class TasksManager
{
    static TickType_t const mqtt_poll_delay = 100 / portTICK_PERIOD_MS;  // Convert 100ms to ticks.
public:
    void
    run();

private:
    static DataManager m_data_manager;
    static WiFiClient m_wifi_client;
    static MqttClient m_mqtt_client;

    static SemaphoreHandle_t m_parse_data_semaphore;
    static SemaphoreHandle_t m_data_send_semaphore;

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
};