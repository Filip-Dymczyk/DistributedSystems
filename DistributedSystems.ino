#include <ArduinoMqttClient.h>
#include <WiFiS3.h>
#include "arduino_secrets.h"

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

static char const temp_message[] = "test_temperature";
static char const insu_message[] = "test_insu";
static char const wind_message[] = "test_wind";

static uint8_t const temp_message_len = sizeof(temp_message) / sizeof(char);
static uint8_t const insu_message_len = sizeof(insu_message) / sizeof(char);
static uint8_t const wind_message_len = sizeof(wind_message) / sizeof(char);

static bool const retained = false;
static int const publish_Qos = 1;
static bool const dup = false;

static void
subscribe_topics();

void setup() {
  Serial.begin(9600);

  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(SSID);
  while (WiFi.begin(SSID, PASSWORD) != WL_CONNECTED) 
  {
    Serial.print(".");
    delay(1000);
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
    delay(5000);
  }

  Serial.println("You're connected to the MQTT broker!");
  Serial.println();

  subscribe_topics();

}

void loop() {
  mqttClient.poll();

  mqttClient.beginMessage(MEAN_TEMPERATURE_PUBLISH, temp_message_len, retained, publish_Qos, dup);
  mqttClient.print(temp_message);
  mqttClient.endMessage();

  mqttClient.beginMessage(MEAN_INSOLATION_PUBLISH, insu_message_len, retained, publish_Qos, dup);
  mqttClient.print(insu_message);
  mqttClient.endMessage();

  mqttClient.beginMessage(MEAN_WIND_PUBLISH, wind_message_len, retained, publish_Qos, dup);
  mqttClient.print(wind_message);
  mqttClient.endMessage();
}

void on_temperature_message(int message_size)
{
  while (mqttClient.available()) 
  {
    Serial.print((char)mqttClient.read());
  }
}

void on_insolation_message(int message_size)
{
  while (mqttClient.available()) 
  {
    Serial.print((char)mqttClient.read());
  }
}

void on_wind_message(int message_size)
{
  while (mqttClient.available()) 
  {
    Serial.print((char)mqttClient.read());
  }
}

static void
subscribe_topics()
{
  int const subscribe_Qos = 1;
  mqttClient.subscribe(TEMPERATURE_TOPIC, subscribe_Qos);
  mqttClient.onMessage(on_temperature_message);

  mqttClient.subscribe(INSOLATION_TOPIC, subscribe_Qos);
  mqttClient.onMessage(on_insolation_message);

  mqttClient.subscribe(WIND_TOPIC, subscribe_Qos);
  mqttClient.onMessage(on_wind_message);
}