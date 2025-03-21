#include <ArduinoMqttClient.h>
#include <WiFiS3.h>
#include "arduino_secrets.h"

#define CONNECTION_DELAY_MS 1000u
#define MAX_DATA_COUNT 100u
#define TEMPERATURE_IDX 0u
#define INSULATION_IDX 1u
#define WIND_IDX 2u

WiFiClient wifi_client {};
MqttClient mqttClient(wifi_client);

static bool const retained = false;
static int const publish_Qos = 1;
static bool const dup = false;

static float received_temperature = 0.0f;
static float received_insulation = 0.0f;
static float received_wind  = 0.0f;
static float sum_temperatures = 0.0f;
static float sum_insulations = 0.0f;
static float sum_winds  = 0.0f;

static uint8_t data_count[] = {0u, 0u, 0u};

static void 
on_message_received(int message_size);

void setup() 
{
  Serial.begin(9600);

  Serial.println("Attempting to connect to the Network...");
  while (WiFi.begin(SSID, PASSWORD) != WL_CONNECTED) 
  {
    Serial.print(".");
    delay(CONNECTION_DELAY_MS);
  }

  Serial.println("You're connected to the Network!");
  Serial.println();

  char const will_message[] = "Client unexpectedly disconnected!";
  bool const will_retain = true;
  int const will_Qos = 1;

  mqttClient.beginWill(WILL_TOPIC, sizeof(will_message) / sizeof(char), will_retain , will_Qos);
  mqttClient.print(will_message);
  mqttClient.endWill();

  Serial.println("Attempting to connect to the MQTT broker...");

  while(!mqttClient.connect(BROKER, PORT))
  {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
    Serial.println("Retrying...");
    delay(CONNECTION_DELAY_MS);
  }

  Serial.println("You're connected to the MQTT broker!");
  Serial.println();

  int const subscribe_Qos = 1;
  mqttClient.subscribe(DATA_RECEIVE_TOPIC, subscribe_Qos);
  mqttClient.onMessage(on_message_received);
}

void loop() 
{
  mqttClient.poll();
}

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

  char output[100];
  switch(data_indicator)
  {
    case 't':
    {
      if(data_count[TEMPERATURE_IDX] < MAX_DATA_COUNT)
      {
        received_temperature = atof(buff_temp);
        sum_temperatures += received_temperature;
        data_count[TEMPERATURE_IDX]++;
        snprintf(output, sizeof(output), "Temperature: %f", received_temperature);

        std::string message = "t" + std::to_string(received_temperature);
        mqttClient.beginMessage(DATA_PUBLISH, message.length(), retained, publish_Qos, dup);
        mqttClient.print(message.c_str());
        mqttClient.endMessage();
      }
      else
      {
        float const temperature_mean = sum_temperatures / MAX_DATA_COUNT;
        sum_temperatures = 0.0f;
        data_count[TEMPERATURE_IDX] = 0u;
        snprintf(output, sizeof(output), "Mean Temperature: %f", temperature_mean);
        
        std::string message = "tm" + std::to_string(temperature_mean);
        mqttClient.beginMessage(DATA_PUBLISH, message.length(), retained, publish_Qos, dup);
        mqttClient.print(message.c_str());
        mqttClient.endMessage();
      }
      break;
    }
    case 'i':
    {
      if(data_count[INSULATION_IDX] < MAX_DATA_COUNT)
      {
        received_insulation = atof(buff_temp);
        sum_insulations += received_insulation;
        data_count[INSULATION_IDX]++;
        snprintf(output, sizeof(output), "Insulation: %f", received_insulation);
      }
      else
      {
        float const insulation_mean = sum_insulations / MAX_DATA_COUNT;
        sum_insulations = 0.0f;
        data_count[INSULATION_IDX] = 0u;
        snprintf(output, sizeof(output), "Mean Insulation: %f", insulation_mean);
      }
      break;
    }
    case 'w':
    {
      if(data_count[WIND_IDX] < MAX_DATA_COUNT)
      {
        received_wind = atof(buff_temp);
        sum_winds += received_wind;
        data_count[WIND_IDX]++;
        snprintf(output, sizeof(output), "Wind: %f", received_wind);
      }
      else
      {
        float const wind_mean = sum_winds / MAX_DATA_COUNT;
        sum_winds = 0.0f;
        data_count[WIND_IDX] = 0u;
        snprintf(output, sizeof(output), "Mean Wind: %f", wind_mean);
      }
      break;
    }
    default:
    {
      break;
    }
  }
  Serial.println(output);
}