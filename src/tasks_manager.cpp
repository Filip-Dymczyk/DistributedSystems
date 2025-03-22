#include "../include/tasks_manager.h"
#include "arduino_secrets.h"

#define CONNECTION_DELAY_MS 1000u
#define THREAD_STACK_SIZE 256

WiFiClient TasksManager::m_wifi_client {};
MqttClient TasksManager::m_mqtt_client {m_wifi_client};
DataManager TasksManager::m_data_manager {m_mqtt_client};
SemaphoreHandle_t TasksManager::m_parse_data_semaphore = nullptr;

void
TasksManager::run()
{
    m_parse_data_semaphore = xSemaphoreCreateBinary();
    if(m_parse_data_semaphore == nullptr)
    {
        Serial.println("Failed to create semaphore!");
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
    vTaskDelete(NULL);
}

void
TasksManager::mqtt_poll_task(void* pvParameters)
{
    Serial.println("Starting MQTT polling...");

    for(;;)
    {
        m_mqtt_client.poll();
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
        }
        vTaskDelay(mqtt_poll_delay);
    }
}

void
TasksManager::data_send_task(void* pvParameters)
{
    Serial.println("Starting data sending...");

    for(;;)
    {
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

    size_t idx = 0;

    static const char init_indicator = '\0';
    m_data_manager.set_data_indicator(init_indicator);

    while(m_mqtt_client.available())
    {
        if(m_data_manager.get_data_indicator() == '\0')
        {
            m_data_manager.set_data_indicator(m_mqtt_client.read());
            continue;
        }
        bool const success = m_data_manager.receive_data(m_mqtt_client.read(), idx);
        idx++;

        if(!success)
        {
            break;
        }
    }

    bool const success = m_data_manager.set_data_terminator(idx);
    if(!success)
    {
        return;
    }

    xSemaphoreGive(m_parse_data_semaphore);  // Notify that parsing can happen.
}