#include "../include/tasks_manager.h"
#include <cctype>
#include "Arduino_FreeRTOS.h"
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
    while(WiFi.begin(NETWORK_SSID, PASSWORD) != WL_CONNECTED)
    {
        Serial.print(".");
        delay(CONNECTION_DELAY_MS);
    }

    delay(CONNECTION_DELAY_MS);  // Delay to stabilize WiFi connection.

    while(!m_mqtt_client.connect(BROKER, BROKER_PORT))
    {
        Serial.print("MQTT connection failed! Error code = ");
        Serial.println(m_mqtt_client.connectError());
        delay(CONNECTION_DELAY_MS);
        Serial.println("Retrying...");
    }

    Serial.println("Connections successful!");

    int const subscribe_Qos = 1;
    m_mqtt_client.subscribe(DATA_RECEIVE_TOPIC, subscribe_Qos);
    m_mqtt_client.onMessage(on_message_received);

    delay(CONNECTION_DELAY_MS);

    // m_my_sql_manager.connect();

    // delay(CONNECTION_DELAY_MS);

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
        if(take_semaphore_safe(m_mqtt_client_mutex) == pdTRUE)
        {
            m_mqtt_client.poll();
            give_semaphore_safe(m_mqtt_client_mutex);
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
        if(take_semaphore_safe(m_parse_data_semaphore) == pdTRUE)
        {
            m_data_manager.parse_data();
            give_semaphore_safe(m_data_send_semaphore);
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
        if(take_semaphore_safe(m_data_send_semaphore) == pdTRUE)
        {
            if(take_semaphore_safe(m_mqtt_client_mutex) == pdTRUE)
            {
                m_mqtt_client.beginMessage(DATA_PUBLISH, m_data_manager.get_message_len(), retained, publish_Qos, dup);
                m_mqtt_client.print(m_data_manager.get_message());
                m_mqtt_client.endMessage();
                give_semaphore_safe(m_mqtt_client_mutex);
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
    give_semaphore_safe(m_parse_data_semaphore);  // Notify that parsing can happen.
}

void
TasksManager::check_connections_task(void* pvParameters)
{
    Serial.println("Starting connections check...");
    static bool recovery_started = false;

    for(;;)
    {
        if(WiFi.status() != WL_CONNECTED || !m_mqtt_client.connected())
        {
            if(!recovery_started)
            {
                recovery_started = true;

                // Suspending and then deleting all tasks apart from the one we are in.
                vTaskDelay(100 / portTICK_PERIOD_MS);
                suspend_tasks();
                vTaskDelay(100 / portTICK_PERIOD_MS);
                reset_connections();

                xTaskCreate(
                       recovery_task, "Connection Recovery Task", THREAD_STACK_SIZE, &recovery_started, 1, nullptr);
            }
        }
        else
        {
            if(take_semaphore_safe(m_mqtt_client_mutex) == pdTRUE)
            {
                send_alive_msg();
                // m_my_sql_manager.insert_data(static_cast<int>(m_data_manager.get_debug_counter()));
                give_semaphore_safe(m_mqtt_client_mutex);
            }
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
TasksManager::suspend_tasks()
{
    if(m_mqtt_poll_task_handle != nullptr)
    {
        vTaskSuspend(m_mqtt_poll_task_handle);
    }
    if(m_data_parse_task_handle != nullptr)
    {
        vTaskSuspend(m_data_parse_task_handle);
    }

    if(m_data_send_task_handle != nullptr)
    {
        vTaskSuspend(m_data_send_task_handle);
    }
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
}

void
TasksManager::initialize_synchronization_items()
{
    m_parse_data_semaphore = xSemaphoreCreateBinary();
    m_data_send_semaphore  = xSemaphoreCreateBinary();
    m_mqtt_client_mutex    = xSemaphoreCreateMutex();

    if(m_parse_data_semaphore == nullptr || m_data_send_semaphore == nullptr || m_mqtt_client_mutex == nullptr)
    {
        Serial.println("Failed to create synchs!");
        return;
    }
}

void
TasksManager::recovery_task(void* pvParameters)
{
    Serial.println("In recovery...");
    bool* recovery_started = static_cast<bool*>(pvParameters);
    
    // Closing check_connections_task here since when done before it could cause undefined behavior.
    if(m_check_connections_task_handle != nullptr)
    {
        vTaskSuspend(m_check_connections_task_handle);
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    if(m_check_connections_task_handle != nullptr)
    {
        vTaskDelete(m_check_connections_task_handle);
        m_check_connections_task_handle = nullptr;
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);

    // By recreating this we are avoiding another call to vTaskScheduler() which is unacceptable.
    initialize_synchronization_items();
    xTaskCreate(connect_task, "WiFi Connect", THREAD_STACK_SIZE, nullptr, 1, &m_connect_task_handle);

    Serial.println("Recovery finished!");
    *recovery_started = false;

    vTaskDelete(nullptr);
}

bool
TasksManager::give_semaphore_safe(SemaphoreHandle_t semaphore)
{
    return (semaphore != nullptr) && (xSemaphoreGive(semaphore) == pdTRUE);
}

bool
TasksManager::take_semaphore_safe(SemaphoreHandle_t semaphore)
{
    return (semaphore != nullptr) && (xSemaphoreTake(semaphore, portMAX_DELAY) == pdTRUE);
}