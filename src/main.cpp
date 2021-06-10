// Platform
#include <Arduino.h>
#include <ArduinoJson.h>
// Temp sensor
#include "DHT.h"
#define DHTPIN 17
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
float temperature;
float humidity;
// Initialize Temp sensor
const char *temp_sensor_id = "8c6f6174-ffa8-444b-8851-943abded59d1";
const char *humid_sensor_id = "9d437e0a-d50a-404a-bf8d-bde0c10799ce";

void initDHT()
{
  Serial.println(F("DHT22 start!"));
  dht.begin();
}

void getSensorReadings()
{
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  while (isnan(t) || isnan(t))
  {
    t = dht.readTemperature();
    h = dht.readHumidity();
  }

  temperature = t;
  humidity = h;
}

// Wi-Fi
#include <WiFi.h>
const char *ssid = "kivill";
const char *password = "26656402665640";
// Initialize WiFi
void initWiFi()
{
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

//WebServer
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
AsyncWebServer server(80);
AsyncEventSource events("/events");

String processor(const String &var)
{
  getSensorReadings();
  if (var == "TEMPERATURE")
  {
    return String(temperature);
  }
  else if (var == "HUMIDITY")
  {
    return String(humidity);
  }
  return String();
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p { font-size: 1.2rem;}
    body {  margin: 0;}
    .topnav { overflow: hidden; background-color: #50B8B4; color: white; font-size: 1rem; }
    .content { padding: 20px; }
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
    .cards { max-width: 800px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); }
    .reading { font-size: 1.4rem; }
  </style>
</head>
<body>
  <div class="topnav">
    <h1>DHT22 WEB SERVER (SSE)</h1>
  </div>
  <div class="content">
    <div class="cards">
      <div class="card">
        <p><i class="fas fa-thermometer-half" style="color:#059e8a;"></i> TEMPERATURE</p><p><span class="reading"><span id="temp">%TEMPERATURE%</span> &deg;C</span></p>
      </div>
      <div class="card">
        <p><i class="fas fa-tint" style="color:#00add6;"></i> HUMIDITY</p><p><span class="reading"><span id="hum">%HUMIDITY%</span> &percnt;</span></p>
      </div>
    </div>
  </div>
<script>
if (!!window.EventSource) {
 var source = new EventSource('/events');
 
 source.addEventListener('open', function(e) {
  console.log("Events Connected");
 }, false);
 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);
 
 source.addEventListener('temperature', function(e) {
  console.log("temperature", e.data);
  document.getElementById("temp").innerHTML = e.data;
 }, false);
 
 source.addEventListener('humidity', function(e) {
  console.log("humidity", e.data);
  document.getElementById("hum").innerHTML = e.data;
 }, false);

}
</script>
</body>
</html>)rawliteral";

// Screen Setup
#include <SPI.h>
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();

// LED Pin
const int LED_PIN = 4;
const int BUTTON_PIN = 5;

// Variables will change:
int buttonPushCounter = 0; // counter for the number of button presses
int buttonState = 0;       // current state of the button
int lastButtonState = 0;   // previous state of the button

#include <PubSubClient.h>
const char *mqtt_server = "192.168.100.4";
const char *deviceId = "ESP32_1";
long lastMsg = 0;
WiFiClient espClient;
PubSubClient client(espClient);

void callback(char *topic, byte *message, unsigned int length)
{
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++)
  {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  StaticJsonDocument<200> doc;
  deserializeJson(doc, messageTemp);

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off".
  // Changes the output state according to the message
  if (String(topic) == "esp32/output" && doc["deviceId"] == deviceId)
  {
    if (doc["sensorId"] == "button")
    {
      if (doc["value"] == 1)
      {
        Serial.println("on");
        digitalWrite(LED_PIN, HIGH);
      }
      else if (doc["value"] == 0)
      {
        Serial.println("off");
        digitalWrite(LED_PIN, LOW);
      }
    }
  }
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(deviceId))
    {
      Serial.println("connected");
      // Subscribe
      client.subscribe("esp32/output");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void connectmqtt()
{
  client.connect(deviceId); // ESP will connect to mqtt broker with clientID
  {
    Serial.println("connected to MQTT");
    // Once connected, publish an announcement...

    // ... and resubscribe
    client.subscribe("esp32/output"); //topic=Demo
    client.publish("esp32/output", "connected to MQTT");

    if (!client.connected())
    {
      reconnect();
    }
  }
}

void setup()
{
  Serial.begin(9600);
  initWiFi();
  initDHT();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  reconnect();
  // // Handle Web Server
  // server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
  //           { request->send_P(200, "text/html", index_html, processor); });

  // // Handle Web Server Events
  // events.onConnect([](AsyncEventSourceClient *client)
  //                  {
  //                    if (client->lastId())
  //                    {
  //                      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
  //                    }
  //                    // send event with message "hello!", id current millis
  //                    // and set reconnect delay to 1 second
  //                    client->send("hello!", NULL, millis(), 10000);
  //                  });
  // server.addHandler(&events);
  // server.begin();
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }

  long now = millis();
  buttonState = digitalRead(BUTTON_PIN);
  if (buttonState != lastButtonState)
  {
    // if the state has changed, increment the counter
    if (buttonState == HIGH)
    {
      char tempButton[200];
      StaticJsonDocument<200> doc;
      doc["deviceId"] = deviceId;
      doc["sensorId"] = "button";
      doc["value"] = 1;
      serializeJson(doc, tempButton);
      client.publish("esp32/output", tempButton);
    }
    else
    {
      char tempButton[200];
      StaticJsonDocument<200> doc;
      doc["deviceId"] = deviceId;
      doc["sensorId"] = "button";
      doc["value"] = 0;
      serializeJson(doc, tempButton);
      client.publish("esp32/output", tempButton);
    }
    delay(50);
  }
  // save the current state as the last state,
  //for next time through the loop
  lastButtonState = buttonState;
  if (now - lastMsg > 60000)
  {
    lastMsg = now;
    getSensorReadings();
    Serial.printf("IP: ");
    Serial.print(WiFi.localIP());
    Serial.println();
    Serial.printf("Temperature = %.2f C \n", temperature);
    Serial.printf("Humidity = %.2f \n", humidity);
    Serial.println();

    char tempString[200];
    char humString[200];
    // dtostrf(temperature, 1, 2, tempString);
    // dtostrf(humidity, 1, 2, humString);
    StaticJsonDocument<200> doc;
    doc["deviceId"] = deviceId;
    doc["sensorId"] = "temperature";
    doc["value"] = temperature;
    serializeJson(doc, tempString);

    // StaticJsonDocument<200> doc1;
    doc["sensorId"] = "humidity";
    doc["value"] = humidity;
    serializeJson(doc, humString);
    client.publish("esp32/output", tempString);
    client.publish("esp32/output", humString);

    // events.send("ping", NULL, millis());
    // events.send(String(temperature).c_str(), "temperature", millis());
    // events.send(String(humidity).c_str(), "humidity", millis());
  }
  client.loop();
}