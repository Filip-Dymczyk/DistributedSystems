#include "../include/tasks_manager.h"
#include <cctype>
#include "api/Common.h"
#include "arduino_secrets.h"

#define CONNECTION_DELAY_MS 1000u
#define THREAD_STACK_SIZE 256

DataManager TasksManager::m_data_manager {};
WiFiClient TasksManager::m_wifi_client_mqtt {};
MqttClient TasksManager::m_mqtt_client {m_wifi_client_mqtt};
WiFiClient TasksManager::m_wifi_client_mysql {};
MySQLManager TasksManager::m_my_sql_manager {m_wifi_client_mysql};

SemaphoreHandle_t TasksManager::m_parse_data_semaphore {nullptr};
SemaphoreHandle_t TasksManager::m_data_send_semaphore {nullptr};
SemaphoreHandle_t TasksManager::m_mqtt_client_mutex {nullptr};

TaskHandle_t TasksManager::m_connect_task_handle {nullptr};
TaskHandle_t TasksManager::m_mqtt_poll_task_handle {nullptr};
TaskHandle_t TasksManager::m_data_parse_task_handle {nullptr};
TaskHandle_t TasksManager::m_data_send_task_handle {nullptr};
TaskHandle_t TasksManager::m_check_connections_task_handle {nullptr};

void
TasksManager::run()
{
    initialize_synchronization_items();

    xTaskCreate(connect_task, "WiFi Connect", THREAD_STACK_SIZE, nullptr, 1, &m_connect_task_handle);

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

    delay(CONNECTION_DELAY_MS);

    m_my_sql_manager.connect();

    delay(CONNECTION_DELAY_MS);

    xTaskCreate(mqtt_poll_task, "MQTT Polling", THREAD_STACK_SIZE, nullptr, 1, &m_mqtt_poll_task_handle);
    xTaskCreate(data_parse_task, "Data Parsing", THREAD_STACK_SIZE, nullptr, 1, &m_data_parse_task_handle);
    xTaskCreate(data_send_task, "Data Sending", THREAD_STACK_SIZE, nullptr, 1, &m_data_send_task_handle);
    xTaskCreate(
        check_connections_task, "Connections Check", THREAD_STACK_SIZE, nullptr, 1, &m_check_connections_task_handle);
    vTaskDelete(m_connect_task_handle);
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
            Serial.println("Recovery initialized\n");
            xTaskCreate(recovery_task, "Connection Recovery Task", THREAD_STACK_SIZE, nullptr, 1, nullptr);
        }
        if(xSemaphoreTake(m_mqtt_client_mutex, portMAX_DELAY) == pdTRUE)
        {
            send_alive_msg();
            m_my_sql_manager.insert_data(static_cast<int>(m_data_manager.get_debug_counter()));
            xSemaphoreGive(m_mqtt_client_mutex);
        }
        vTaskDelay(connection_alive_delay);
    }
}

void
TasksManager::send_alive_msg()
{
    static char const alive_msg[] = "Connections alive\0";
    static size_t alive_msg_size  = strlen(alive_msg);

    m_mqtt_client.beginMessage(CONNECTION_ALIVE, alive_msg_size, retained, publish_Qos, dup);
    m_mqtt_client.print(alive_msg);
    m_mqtt_client.endMessage();
}

void
TasksManager::reset_connections()
{
    if(m_mqtt_poll_task_handle != nullptr)
    {
        vTaskDelete(m_mqtt_poll_task_handle);
        m_mqtt_poll_task_handle = nullptr;
    }

    if(m_data_parse_task_handle != nullptr)
    {
        vTaskDelete(m_data_parse_task_handle);
        m_data_parse_task_handle = nullptr;
    }

    if(m_data_send_task_handle != nullptr)
    {
        vTaskDelete(m_data_send_task_handle);
        m_data_send_task_handle = nullptr;
    }

    if(m_parse_data_semaphore != nullptr)
    {
        vSemaphoreDelete(m_parse_data_semaphore);
        m_parse_data_semaphore = nullptr;
    }

    if(m_data_send_semaphore != nullptr)
    {
        vSemaphoreDelete(m_data_send_semaphore);
        m_data_send_semaphore = nullptr;
    }

    if(m_mqtt_client_mutex != nullptr)
    {
        vSemaphoreDelete(m_mqtt_client_mutex);
        m_mqtt_client_mutex = nullptr;
    }

    if(m_check_connections_task_handle != nullptr)
    {
        vTaskDelete(m_check_connections_task_handle);
        m_check_connections_task_handle = nullptr;
    }
}

void
TasksManager::initialize_synchronization_items()
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
}

void
TasksManager::recovery_task(void* pvParameters)
{
    Serial.println("In recovery...");

    vTaskDelay(100 / portTICK_PERIOD_MS);

    reset_connections();

    vTaskDelay(100 / portTICK_PERIOD_MS);

    // By recreating this we are avoiding another call to vTaskScheduler().
    initialize_synchronization_items();
    xTaskCreate(connect_task, "WiFi Connect", THREAD_STACK_SIZE, nullptr, 1, &m_connect_task_handle);

    Serial.println("Recovery finished!");

    vTaskDelete(nullptr);
}
