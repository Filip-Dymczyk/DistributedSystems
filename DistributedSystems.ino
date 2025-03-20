#include <ArduinoMqttClient.h>
#include <WiFiS3.h>
#include "arduino_secrets.h"

#define CONNECTION_DELAY_MS 1000

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

static char const test_message_mean[] = "test_msg_mean";
static char const test_message_raw[] = "test_msg_raw";
static uint8_t const test_message_mean_len = sizeof(test_message_mean) / sizeof(char);
static uint8_t const test_message_raw_len = sizeof(test_message_raw) / sizeof(char);

static bool const retained = false;
static int const publish_Qos = 1;
static bool const dup = false;

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
      snprintf(output, sizeof(output), "Temperature %s", buff_temp);
      break;
    }
    case 'i':
    {
      snprintf(output, sizeof(output), "Insulation %s", buff_temp);
      break;
    }
    case 'w':
    {
      snprintf(output, sizeof(output), "Wind %s", buff_temp);
      break;
    }
    default:
    {
      break;
    }
  }
  Serial.println(output);
}