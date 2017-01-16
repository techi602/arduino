/**
 * Battery powered weather sensor running on esp8266 based board
 * wakes up, connects to wi-fi, measure temperature, humidity and pressure and send them to MQTT server
 *
 * Requires BME280 sensor
 */

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// Update these with values suitable for your network.

const char* ssid = "your ssid";
const char* password = "your wifi password";
const char* mqtt_server = "mqtt ip address";
const char* mqtt_user = "";
const char* mqtt_password = "";
const char* mqtt_client = "ESP8266Client_";
const char* mdns_name = "esp8266-your-place";
const char* place = "your place";

#define SLEEP_INTERVAL 600e6

Adafruit_BME280 bme; // I2C
WiFiClient espClient;
PubSubClient client(espClient);
char payload[50];
char topic[50];
int value = 0;


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void setup_wifi() {
  bool ledState = true;
  byte connectionAttempts = 0;

  delay(10);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.mode(WIFI_STA);

  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(BUILTIN_LED, ledState ? HIGH : LOW);
    delay(500);
    Serial.print(".");
    ledState = !ledState;
    connectionAttempts++;

    if (connectionAttempts > 60) {
      Serial.println();
      Serial.println("Giving up connecting to Wi-Fi.");
      Serial.print("Wi-Fi status: ");
      Serial.println(WiFi.status());
      Serial.println("Going to sleep...");
      ESP.deepSleep(SLEEP_INTERVAL);
    }
  }

  digitalWrite(BUILTIN_LED, LOW);

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println(WiFi.SSID());
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  byte connectionAttempts = 0;
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = mqtt_client;
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str()), mqtt_user, mqtt_password) {
      Serial.println("connected");
      delay(10);
      // Once connected, publish an announcement...
      Serial.println(client.publish("/home", clientId.c_str()));
      Serial.println(client.publish("/home", "hello from balcony!"));
      
    } else {
      if (connectionAttempts > 5) {
        Serial.println();
        Serial.println("Giving up connecting to MQTT.");
        Serial.println("Going to sleep...");
        ESP.deepSleep(SLEEP_INTERVAL, WAKE_RF_DEFAULT);
      }

      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
      connectionAttempts++;
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUILTIN_LED, OUTPUT);

  bool status = bme.begin(0x76);
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    ESP.deepSleep(SLEEP_INTERVAL, WAKE_RF_DEFAULT);
    return;
  }
 
  setup_wifi();

  if (!MDNS.begin(mdns_name)) {
    Serial.println("Error setting up MDNS responder!");
  }

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  reconnect();

  float homeTemp = bme.readTemperature();
  float homeHumidity = bme.readHumidity();
  float homePressure = bme.readPressure()  / 100.0F;

  strcpy(topic, "/home/temperature/");
  strcat(topic, place);
  dtostrf(homeTemp, 0, 1, payload);
  Serial.print("Temperature: ");
  Serial.print(payload);
  Serial.println("*C");
  Serial.println("Publishing payload");
  Serial.println(topic);
  client.publish(topic, payload, true);

  strcpy(topic, "/home/humidity/");
  strcat(topic, place);
  dtostrf(homeHumidity, 0, 1, payload);
  Serial.print("Humidity: ");
  Serial.print(payload);
  Serial.println("%");
  Serial.println("Publishing payload");
  Serial.println(topic);
  client.publish(topic, payload, true);

  strcpy(topic, "/home/pressure/");
  strcat(topic, place);
  dtostrf(homePressure, 0, 1, payload);
  Serial.print("Pressure: ");
  Serial.print(payload);
  Serial.println(" hPa");
  Serial.println("Publishing payload");
  Serial.println(topic);
  client.publish(topic, payload, true);

  Serial.println("Sending data to MQTT...");
  client.loop();
  delay(100); // this has to be here otherwise topics are not send
  // possible workaround is to subscribe to published topics and actually wait and check until callback with topic is called
  // this is faster :)

  Serial.println("Disconnecting...");
  client.disconnect();
  //MDNS.close();
  //WiFi.disconnect();

  Serial.println("Going to sleep...");
  ESP.deepSleepInstant(SLEEP_INTERVAL, WAKE_RF_DEFAULT);
}

void loop()
{

}
