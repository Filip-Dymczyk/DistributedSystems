#include <ArduinoMqttClient.h>
#include <Arduino_FreeRTOS.h>
#include <WiFiS3.h>
#include "arduino_secrets.h"

#define CONNECTION_DELAY_MS 1000u
#define MAX_DATA_COUNT 100u
#define TEMPERATURE_IDX 0u
#define INSULATION_IDX 1u
#define WIND_IDX 2u
#define THREAD_STACK_SIZE 256

WiFiClient wifi_client {};
MqttClient mqtt_client(wifi_client);

static bool const retained              = false;
static int const publish_Qos            = 1;
static bool const dup                   = false;
static TickType_t const mqtt_poll_delay = 100 / portTICK_PERIOD_MS;  // Convert 100ms to ticks.

static float received_temperature = 0.0f;
static float received_insulation  = 0.0f;
static float received_wind        = 0.0f;
static float sum_temperatures     = 0.0f;
static float sum_insulations      = 0.0f;
static float sum_winds            = 0.0f;
static uint8_t data_count[]       = {0u, 0u, 0u};

static char data_indicator;
static char received_data[MAX_DATA_COUNT];

SemaphoreHandle_t parse_data_semaphore = nullptr;

static void
on_message_received(int message_size);

static void
connect_task(void* pvParameters);

static void
mqtt_poll_task(void* pvParameters);

static void
data_parse_task(void* pvParameters);

static void
data_send_task(void* pvParameters);

void
setup()
{
    Serial.begin(9600);

    parse_data_semaphore = xSemaphoreCreateBinary();
    if(parse_data_semaphore == nullptr)
    {
        Serial.println("Failed to create semaphore!");
        return;
    }

    xTaskCreate(connect_task, "WiFi Connect", THREAD_STACK_SIZE, nullptr, 1, nullptr);
    
    Serial.println("Initialization complete!");
    vTaskStartScheduler();
}

static void
on_message_received(int message_size)
{
    if(message_size <= 0)
    {
        return;
    }

    uint32_t i = 0;

    data_indicator = '\0';
    while(mqtt_client.available())
    {
        if(data_indicator == '\0')
        {
            data_indicator = static_cast<char>(mqtt_client.read());
        }
        if(i < MAX_DATA_COUNT - 1)
        {
            received_data[i] = static_cast<char>(mqtt_client.read());
            i++;
        }
        else
        {
            break;
        }
    }
    received_data[i] = '\0';

    xSemaphoreGive(parse_data_semaphore);  // Notify that parsing can happen.
}

static void
connect_task(void* pvParameters)
{
    Serial.println("Attempting to connect to the Network...");
    while(WiFi.begin(SSID, PASSWORD) != WL_CONNECTED)
    {
        Serial.print(".");
        delay(CONNECTION_DELAY_MS);
    }

    Serial.println("You're connected to the Network!");

    delay(CONNECTION_DELAY_MS);  // Delay to stabilize WiFi connection.

    Serial.println("Attempting to connect the MQTT broker...");
    while(!mqtt_client.connect(BROKER, PORT))
    {
        Serial.print("MQTT connection failed! Error code = ");
        Serial.println(mqtt_client.connectError());
        delay(CONNECTION_DELAY_MS);
        Serial.println("Retrying...");
    }

    Serial.println("Connected to the MQTT broker!");

    int const subscribe_Qos = 1;
    mqtt_client.subscribe(DATA_RECEIVE_TOPIC, subscribe_Qos);
    mqtt_client.onMessage(on_message_received);

    delay(CONNECTION_DELAY_MS);  // Additional delay.

    xTaskCreate(mqtt_poll_task, "MQTT Polling", THREAD_STACK_SIZE, nullptr, 1, nullptr);
    xTaskCreate(data_parse_task, "Data Parsing", THREAD_STACK_SIZE, nullptr, 1, nullptr);
    xTaskCreate(data_send_task, "Data Sending", THREAD_STACK_SIZE, nullptr, 1, nullptr);
    vTaskDelete(NULL);
}

static void
mqtt_poll_task(void* pvParameters)
{
    Serial.println("Starting MQTT polling...");

    for(;;)
    {
        mqtt_client.poll();
        vTaskDelay(mqtt_poll_delay);
    }
}

static void
parse_data();

static void
data_parse_task(void* pvParameters)
{
    Serial.println("Starting data parsing...");

    for(;;)
    {
        if(xSemaphoreTake(parse_data_semaphore, portMAX_DELAY) == pdTRUE)
        {
            parse_data();
        }
        vTaskDelay(mqtt_poll_delay);
    }
}

static void
data_send_task(void* pvParameters)
{
    Serial.println("Starting data sending...");

    for(;;)
    {
        vTaskDelay(mqtt_poll_delay);
    }
}

static void
parse_data()
{
    char message[100];
    switch(data_indicator)
    {
        case 't':
        {
            if(data_count[TEMPERATURE_IDX] < MAX_DATA_COUNT)
            {
                received_temperature = strtof(received_data, nullptr);
                sum_temperatures += received_temperature;
                data_count[TEMPERATURE_IDX]++;

                uint16_t const len = snprintf(message, sizeof(message), "t%f", received_temperature);
                mqtt_client.beginMessage(DATA_PUBLISH, len, retained, publish_Qos, dup);
                mqtt_client.print(message);
                mqtt_client.endMessage();
            }
            else
            {
                float const temperature_mean = sum_temperatures / MAX_DATA_COUNT;
                sum_temperatures             = 0.0f;
                data_count[TEMPERATURE_IDX]  = 0u;

                uint16_t const len = snprintf(message, sizeof(message), "tm%f", temperature_mean);
                mqtt_client.beginMessage(DATA_PUBLISH, len, retained, publish_Qos, dup);
                mqtt_client.print(message);
                mqtt_client.endMessage();
            }
            break;
        }
        case 'i':
        {
            if(data_count[INSULATION_IDX] < MAX_DATA_COUNT)
            {
                received_insulation = strtof(received_data, nullptr);
                sum_insulations += received_insulation;
                data_count[INSULATION_IDX]++;
            }
            else
            {
                float const insulation_mean = sum_insulations / MAX_DATA_COUNT;
                sum_insulations             = 0.0f;
                data_count[INSULATION_IDX]  = 0u;
            }
            break;
        }
        case 'w':
        {
            if(data_count[WIND_IDX] < MAX_DATA_COUNT)
            {
                received_wind = strtof(received_data, nullptr);
                sum_winds += received_wind;
                data_count[WIND_IDX]++;
            }
            else
            {
                float const wind_mean = sum_winds / MAX_DATA_COUNT;
                sum_winds             = 0.0f;
                data_count[WIND_IDX]  = 0u;
            }
            break;
        }
        default:
        {
            break;
        }
    }
}

void
loop()
{
}