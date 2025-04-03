#include "../include/tasks_manager.h"
#include <cctype>
#include "api/Common.h"
#include "arduino_secrets.h"

#define CONNECTION_DELAY_MS 1000u
#define THREAD_STACK_SIZE 256

DataManager TasksManager::m_data_manager {};
WiFiClient TasksManager::m_wifi_client {};
MqttClient TasksManager::m_mqtt_client {m_wifi_client};

SemaphoreHandle_t TasksManager::m_parse_data_semaphore = nullptr;
SemaphoreHandle_t TasksManager::m_data_send_semaphore  = nullptr;
SemaphoreHandle_t TasksManager::m_mqtt_client_mutex  = nullptr;

void
TasksManager::run()
{
    m_parse_data_semaphore = xSemaphoreCreateBinary();
    if(m_parse_data_semaphore == nullptr)
    {
        Serial.println("Failed to create semaphore!");
        return;
    }

    m_data_send_semaphore = xSemaphoreCreateBinary();
    if(m_data_send_semaphore == nullptr)
    {
        Serial.println("Failed to create semaphore!");
        return;
    }

    m_mqtt_client_mutex = xSemaphoreCreateMutex();
    if(m_data_send_semaphore == nullptr)
    {
        Serial.println("Failed to create mutex!");
        return;
    }

    xTaskCreate(connect_task, "WiFi Connect", THREAD_STACK_SIZE, nullptr, 1, nullptr);

    Serial.println("Initialization complete!");
    vTaskStartScheduler();
}

void
TasksManager::connect_task(void* pvParameters)
{
    Serial.println("Attempting to connect to the Network...");
    while(WiFi.begin(NETWORK_SSID, PASSWORD) != WL_CONNECTED)
    {
        Serial.print(".");
        delay(CONNECTION_DELAY_MS);
    }

    Serial.println("You're connected to the Network!");

    delay(CONNECTION_DELAY_MS);  // Delay to stabilize WiFi connection.

    Serial.println("Attempting to connect the MQTT broker...");
    while(!m_mqtt_client.connect(BROKER, BROKER_PORT))
    {
        Serial.print("MQTT connection failed! Error code = ");
        Serial.println(m_mqtt_client.connectError());
        delay(CONNECTION_DELAY_MS);
        Serial.println("Retrying...");
    }

    Serial.println("Connected to the MQTT broker!");

    int const subscribe_Qos = 1;
    m_mqtt_client.subscribe(DATA_RECEIVE_TOPIC, subscribe_Qos);
    m_mqtt_client.onMessage(on_message_received);

    delay(CONNECTION_DELAY_MS);  // Additional delay.

    xTaskCreate(mqtt_poll_task, "MQTT Polling", THREAD_STACK_SIZE, nullptr, 1, nullptr);
    xTaskCreate(data_parse_task, "Data Parsing", THREAD_STACK_SIZE, nullptr, 1, nullptr);
    xTaskCreate(data_send_task, "Data Sending", THREAD_STACK_SIZE, nullptr, 1, nullptr);
    xTaskCreate(check_connections_task, "Connections Check", THREAD_STACK_SIZE, nullptr, 1, nullptr);
    vTaskDelete(NULL);
}

void
TasksManager::mqtt_poll_task(void* pvParameters)
{
    Serial.println("Starting MQTT polling...");

    for(;;)
    {
        if(xSemaphoreTake(m_mqtt_client_mutex, portMAX_DELAY) == pdTRUE)
        {
            m_mqtt_client.poll();
            xSemaphoreGive(m_mqtt_client_mutex);
        }
        vTaskDelay(mqtt_poll_delay);
    }
}

void
TasksManager::data_parse_task(void* pvParameters)
{
    Serial.println("Starting data parsing...");

    for(;;)
    {
        if(xSemaphoreTake(m_parse_data_semaphore, portMAX_DELAY) == pdTRUE)
        {
            m_data_manager.parse_data();
            xSemaphoreGive(m_data_send_semaphore);
        }
        vTaskDelay(mqtt_poll_delay);
    }
}

void
TasksManager::data_send_task(void* pvParameters)
{
    Serial.println("Starting data sending...");

    static bool const retained   = false;
    static bool const dup        = false;
    static int const publish_Qos = 1;

    for(;;)
    {
        if(xSemaphoreTake(m_data_send_semaphore, portMAX_DELAY) == pdTRUE)
        {
            if(xSemaphoreTake(m_mqtt_client_mutex, portMAX_DELAY) == pdTRUE)
            {
                m_mqtt_client.beginMessage(DATA_PUBLISH, m_data_manager.get_message_len(), retained, publish_Qos, dup);
                m_mqtt_client.print(m_data_manager.get_message());
                m_mqtt_client.endMessage();
                xSemaphoreGive(m_mqtt_client_mutex);
            }  
        }
        vTaskDelay(mqtt_poll_delay);
    }
}

void
TasksManager::on_message_received(int message_size)
{
    if(message_size <= 0)
    {
        return;
    }

    size_t region_idx    = 0u;
    size_t values_idx    = 0u;
    bool region_obtained = false;

    while(m_mqtt_client.available())
    {
        char const mqtt_data = static_cast<char>(m_mqtt_client.read());

        if(isDigit(mqtt_data) && !region_obtained)
        {
            bool const success = m_data_manager.receive_idx_data(mqtt_data, region_idx++);
            if(!success)
            {
                return;
            }
        }
        else
        {
            region_obtained = true;

            bool const success = m_data_manager.receive_data(mqtt_data, values_idx++);

            if(!success)
            {
                return;
            }
        }
    }

    bool success = m_data_manager.set_idx_data_terminator(region_idx);
    if(!success)
    {
        return;
    }

    success = m_data_manager.set_data_terminator(values_idx);
    if(!success)
    {
        return;
    }
    xSemaphoreGive(m_parse_data_semaphore);  // Notify that parsing can happen.
}

void
TasksManager::check_connections_task(void* pvParameters)
{
    Serial.println("Starting connections check...");

    for(;;)
    {    
        if(WiFi.status() != WL_CONNECTED || !m_mqtt_client.connected())
        {
            NVIC_SystemReset();
        }
        vTaskDelay(connection_alive_delay);
    }
}