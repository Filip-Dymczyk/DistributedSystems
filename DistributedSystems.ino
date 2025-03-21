#include <ArduinoMqttClient.h>
#include <WiFiS3.h>
#include <Arduino_FreeRTOS.h>
#include "arduino_secrets.h"

#define CONNECTION_DELAY_MS 1000u
#define MAX_DATA_COUNT 100u
#define TEMPERATURE_IDX 0u
#define INSULATION_IDX 1u
#define WIND_IDX 2u
#define THREAD_STACK_SIZE 512  

WiFiClient wifi_client {};
MqttClient mqttClient(wifi_client);

static bool const retained = false;
static int const publish_Qos = 1;
static bool const dup = false;
static TickType_t const mqtt_poll_delay = 100 / portTICK_PERIOD_MS;  // Convert 100ms to ticks.

static float received_temperature = 0.0f;
static float received_insulation = 0.0f;
static float received_wind  = 0.0f;
static float sum_temperatures = 0.0f;
static float sum_insulations = 0.0f;
static float sum_winds  = 0.0f;
static uint8_t data_count[] = {0u, 0u, 0u};

SemaphoreHandle_t connection_semaphore;

static void 
on_message_received(int message_size);

static void 
mqtt_poll_task(void *pvParameters);

static void 
connect_task(void *pvParameters);

void setup() 
{
  Serial.begin(9600);

  connection_semaphore = xSemaphoreCreateBinary();
  
  if (connection_semaphore == NULL) 
  {
    Serial.println("Failed to create semaphore!");
    return;
  }

  xTaskCreate(connect_task, "WiFi Connect", THREAD_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(mqtt_poll_task, "MQTT Polling", THREAD_STACK_SIZE, NULL, 1, NULL);

  vTaskStartScheduler();
}

void loop() 
{}

static void 
on_message_received(int message_size)
{
  if (message_size <= 0) 
  {
    return;
  }

  char buff_temp[message_size];
  uint32_t i = 0;

  char data_indicator = '\0';
  while (mqttClient.available()) 
  {
    if(data_indicator == '\0')
    {
      data_indicator = (char)mqttClient.read();
    }
    
    if(i < message_size - 1)
    {
      buff_temp[i] = (char)mqttClient.read();
      i++;
    }
    else
    {
      break;
    }
  }
  
  buff_temp[i] = '\0';

  char message[100];

  switch(data_indicator)
  {
    case 't':
    {
      if(data_count[TEMPERATURE_IDX] < MAX_DATA_COUNT)
      {
        received_temperature = strtof(buff_temp, nullptr);
        sum_temperatures += received_temperature;
        data_count[TEMPERATURE_IDX]++;

        uint16_t const len = snprintf(message, sizeof(message), "t%f", received_temperature);
        mqttClient.beginMessage(DATA_PUBLISH, len, retained, publish_Qos, dup);
        mqttClient.print(message);
        mqttClient.endMessage();
      }
      else
      {
        float const temperature_mean = sum_temperatures / MAX_DATA_COUNT;
        sum_temperatures = 0.0f;
        data_count[TEMPERATURE_IDX] = 0u;
        
        uint16_t const len = snprintf(message, sizeof(message), "tm%f", temperature_mean);
        mqttClient.beginMessage(DATA_PUBLISH, len, retained, publish_Qos, dup);
        mqttClient.print(message);
        mqttClient.endMessage();
      }
      break;
    }
    case 'i':
    {
      if(data_count[INSULATION_IDX] < MAX_DATA_COUNT)
      {
        received_insulation = strtof(buff_temp, nullptr);
        sum_insulations += received_insulation;
        data_count[INSULATION_IDX]++;
      }
      else
      {
        float const insulation_mean = sum_insulations / MAX_DATA_COUNT;
        sum_insulations = 0.0f;
        data_count[INSULATION_IDX] = 0u;
      }
      break;
    }
    case 'w':
    {
      if(data_count[WIND_IDX] < MAX_DATA_COUNT)
      {
        received_wind = strtof(buff_temp, nullptr);
        sum_winds += received_wind;
        data_count[WIND_IDX]++;
      }
      else
      {
        float const wind_mean = sum_winds / MAX_DATA_COUNT;
        sum_winds = 0.0f;
        data_count[WIND_IDX] = 0u;
      }
      break;
    }
    default:
    {
      break;
    }
  }
}


static void 
connect_task(void *pvParameters)
{
  Serial.println("Attempting to connect to the Network...");
  while (WiFi.begin(SSID, PASSWORD) != WL_CONNECTED) 
  {
    Serial.print(".");
    delay(CONNECTION_DELAY_MS);
  }

  Serial.println("You're connected to the Network!");
  Serial.println();

  delay(CONNECTION_DELAY_MS); // Delay to stabilize WiFi connection.

  Serial.println("Attempting to connect the MQTT broker...");
  while (!mqttClient.connect(BROKER, PORT)) 
  {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
    delay(CONNECTION_DELAY_MS);
    Serial.println("Retrying...");
  }

  Serial.println("Connected to the MQTT broker!");

  int const subscribe_Qos = 1;
  mqttClient.subscribe(DATA_RECEIVE_TOPIC, subscribe_Qos);
  mqttClient.onMessage(on_message_received);

  xSemaphoreGive(connection_semaphore);
  vTaskDelete(NULL);
}

static void 
mqtt_poll_task(void *pvParameters)
{
  if(xSemaphoreTake(connection_semaphore, portMAX_DELAY) != pdTRUE)
  {
    return;
  }

  Serial.println("WiFi and MQTT connection complete. Starting MQTT polling...");

  for(;;)
  {
    mqttClient.poll();
    vTaskDelay(mqtt_poll_delay);
  }
}