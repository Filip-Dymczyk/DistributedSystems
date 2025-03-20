#include <ArduinoMqttClient.h>
#include <WiFiS3.h>
#include "arduino_secrets.h"

#define CONNECTION_DELAY_MS 1000u
#define BUFFER_SIZE 10u
#define TEMPERATUR_IDX 0u
#define INSULATION_IDX 1u
#define WIND_IDX 2u

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

static char const test_message_mean[] = "test_msg_mean";
static char const test_message_raw[] = "test_msg_raw";
static uint8_t const test_message_mean_len = sizeof(test_message_mean) / sizeof(char);
static uint8_t const test_message_raw_len = sizeof(test_message_raw) / sizeof(char);

static bool const retained = false;
static int const publish_Qos = 1;
static bool const dup = false;

static float received_temperature[BUFFER_SIZE];
static float received_insulation[BUFFER_SIZE];
static float received_wind[BUFFER_SIZE];
static uint8_t idx[] = {0u, 0u, 0u};

static void 
on_message_received(int message_size);

void setup() {
  Serial.begin(9600);

  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(SSID);
  while (WiFi.begin(SSID, PASSWORD) != WL_CONNECTED) 
  {
    Serial.print(".");
    delay(CONNECTION_DELAY_MS);
  }

  Serial.println("You're connected to the network");
  Serial.println();

  char const will_message[] = "Client unexpectedly disconnected!";
  bool const will_retain = true;
  int const will_Qos = 1;

  mqttClient.beginWill(WILL_TOPIC, sizeof(will_message) / sizeof(char), will_retain , will_Qos);
  mqttClient.print(will_message);
  mqttClient.endWill();

  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(BROKER);

  while(!mqttClient.connect(BROKER, PORT))
  {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
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
  bool buffers_full = true;
  for(int i = 0; i < 3; i++)
  {
    if(idx[i] < BUFFER_SIZE)
    {
      buffers_full = false;
      break;
    }
  }

  if(buffers_full)
  {
     Serial.println("Buffers full - restart");
     idx[TEMPERATUR_IDX] = 0u;
     idx[INSULATION_IDX] = 0u;
     idx[WIND_IDX] = 0u;
  }

  mqttClient.poll();
  
  // mqttClient.beginMessage(MEAN_DATA_PUBLISH, test_message_mean_len, retained, publish_Qos, dup);
  // mqttClient.print(test_message_mean);
  // mqttClient.endMessage();

  // mqttClient.beginMessage(RAW_DATA_PUBLISH, test_message_raw_len, retained, publish_Qos, dup);
  // mqttClient.print(test_message_raw);
  // mqttClient.endMessage();
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
      if(idx[TEMPERATUR_IDX] < BUFFER_SIZE)
      {
        received_temperature[idx[TEMPERATUR_IDX]] = atof(buff_temp);
        idx[TEMPERATUR_IDX]++;
        snprintf(output, sizeof(output), "Temperature %s", buff_temp);
      }
      else
      {
        snprintf(output, sizeof(output), "Temperature full");
      }
      break;
    }
    case 'i':
    {
      if(idx[INSULATION_IDX] < BUFFER_SIZE)
      {
        received_insulation[idx[INSULATION_IDX]] = atof(buff_temp);
        idx[INSULATION_IDX]++;
        snprintf(output, sizeof(output), "Insulation %s", buff_temp);
      }
      else
      {
        snprintf(output, sizeof(output), "Insulation full");
      }
      break;
    }
    case 'w':
    {
      if(idx[WIND_IDX] < BUFFER_SIZE)
      {
        received_wind[idx[WIND_IDX]] = atof(buff_temp);
        idx[WIND_IDX]++;
        snprintf(output, sizeof(output), "Wind %s", buff_temp);
      }
      else
      {
        snprintf(output, sizeof(output), "Wind full");
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