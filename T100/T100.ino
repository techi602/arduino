/*
 * T100 robot control
 * 
 * Wemos D1 R2 + DOIT.AM shield https://github.com/SmartArduino/SZDOITWiKi/wiki/Arduino---2-way%26amp%3B16-way-motor%26amp%3Bservo-shield
 * SSD1306 oled siplay https://github.com/acrobotic/Ai_Ardulib_SSD1306
 * 
 * joystick coordinates math http://home.kendra.com/mauser/Joystick.html
 * 
 * browser gamepad demo: 
 * http://luser.github.io/gamepadtest/
 * http://internetexplorer.github.io/Gamepad-Sample/
 */

#ifdef ESP32

#define PWMRANGE 1023

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#else

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#endif

#include <Arduino.h>
#include <WebSocketsServer.h>
#include <string.h>
#include <Wire.h>
#include <ACROBOTIC_SSD1306.h>

#include "joystick.js.h"
#include "control.html.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// WiFi parameters
#include "secrets.h"

#ifndef WIFI_SSID
#define WIFI_SSID "wifi"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
char buffer[40];

#ifdef ESP32
//MDNSResponder mdns;
WiFiMulti WiFiMulti;
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
#else
MDNSResponder mdns;
ESP8266WiFiMulti WiFiMulti;
ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
#endif

#define PWMA D6
#define PWMB D4
#define DIRA D7
#define DIRB D5

#define POWER_PIN D8

// speed range from -100 to +100
int motor_speed_left = 0;
int motor_speed_right = 0;

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    Serial.printf("webSocketEvent(%d, %d, ...)\r\n", num, type);
    switch (type)
    {
        case WStype_DISCONNECTED:
        {
            Serial.printf("[%u] Disconnected!\r\n", num);
            oled.clearDisplay();
            oled.putString("Client disconnected");
            break;
        }
        case WStype_CONNECTED:
        {
            IPAddress ip = webSocket.remoteIP(num);
            sprintf(buffer, "[%u] Connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
            Serial.printf(buffer);
            
            // Send the current LED status
            webSocket.sendTXT(0, "HELLO", 5);
    
            oled.clearDisplay();
            oled.setTextXY(0,0);
            oled.putString("Client connected");
            oled.setTextXY(1,0);
            oled.putString(buffer);
            break;
        }
        case WStype_TEXT:
        {
            Serial.printf("[%u] get Text: %s\r\n", num, payload);
    
            // motor:<leftspeed>:<rightspeed>
            if (strncmp("motor", (const char *)payload, 5) == 0)
            {
                const char delim[2] = ":";
                char *token;
    
                // expect "motor"
                token = strtok((char *)payload, delim);
    
                // expect left speed as string
                token = strtok(NULL, delim);
    
                motor_speed_left = atoi(token);
    
                // expect right speed as string
                token = strtok(NULL, delim);
    
                motor_speed_right = atoi(token);
    
                setMotorSpeed(motor_speed_left, motor_speed_right);
                Serial.print("Setting motor speed ");
                Serial.print(motor_speed_left);
                Serial.print(' ');
                Serial.println(motor_speed_right);

                // display delay too slow blocking socket IO
                /*
                oled.setTextXY(0,0);
                oled.putString("Motor speed");
                oled.setTextXY(1,0);
                oled.putString("       ");
                oled.putNumber(motor_speed_left);
                oled.setTextXY(2,0);
                oled.putString("       ");
                oled.putNumber(motor_speed_right);
                */
            }
            else if (strcmp("on", (const char *)payload) == 0)
            {
                robot_on();
                oled.setTextXY(0,0);
                oled.putString("FIRE!        ");
            }
            else if (strcmp("off", (const char *)payload) == 0)
            {
                robot_off();
            }
    
            else if (strcmp("stop", (const char *)payload) == 0)
            {
                robot_stop();
            }
            else
            {
                Serial.print("Unknown command: ");
                Serial.println((const char *)payload);
            }
            // send data to all connected clients
            // too slow - blocking IO
            //webSocket.broadcastTXT(payload, length);
            break;
        }
        case WStype_BIN:
        {
            Serial.printf("[%u] get binary length: %u\r\n", num, length);
            //hexdump(payload, length);
    
            // echo data back to browser
            //webSocket.sendBIN(num, payload, length);
            break;
        }
        default:
        {
            Serial.printf("Invalid WStype [%d]\r\n", type);
            break;
        }
    }
}

void handleRoot()
{
    server.send_P(200, "text/html", INDEX_HTML);
}

void handleJoystickJs()
{
    server.send_P(200, "text/javascript", JOYSTICK_JS);
}

void handleNotFound()
{
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i = 0; i < server.args(); i++)
    {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
}

String IPAddress2String(IPAddress address)
{
    return
        String(address[0]) + "." + 
        String(address[1]) + "." + 
        String(address[2]) + "." + 
        String(address[3]);
}

void setup(void)
{
    pinMode(DIRA, OUTPUT);
    pinMode(DIRB, OUTPUT);
    pinMode(PWMA, OUTPUT);
    pinMode(PWMB, OUTPUT);
    pinMode(POWER_PIN, OUTPUT);

    analogWrite(PWMA, 0);
    analogWrite(PWMB, 0);

    digitalWrite(DIRA, LOW);
    digitalWrite(DIRB, LOW);

    digitalWrite(POWER_PIN, LOW);

    Serial.begin(115200);
    Serial.println();
    Serial.println();
    Serial.println();
    Serial.println("T100 booting");

    Wire.begin();  

    // Initialze SSD1306 OLED display
    oled.init();
    oled.clearDisplay();
    oled.setTextXY(0,0);

    oled.putString("Connecting to Wi-Fi...");
  
    // my mobile hotspot
    WiFiMulti.addAP("AndroidAP", "uche7888");
    WiFiMulti.addAP(ssid, password);

    while (WiFiMulti.run() != WL_CONNECTED)
    {
        //oled.write('.');
        Serial.print(".");
        delay(100);
    }

    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.println(WiFi.localIP());

    oled.clearDisplay();
    oled.setTextXY(0, 0);
    oled.putString("Connected");
    oled.setTextXY(1, 0);
    oled.putString(ssid);
    oled.setTextXY(2, 0);
    oled.putString(IPAddress2String(WiFi.localIP()));
    
    if (mdns.begin("t100", WiFi.localIP()))
    {
        Serial.println("MDNS responder started");
        mdns.addService("http", "tcp", 80);
        mdns.addService("ws", "tcp", 81);
    }
    else
    {
        Serial.println("MDNS.begin failed");
    }
    Serial.print("Connect to http://t100.local");
    Serial.println("");
    Serial.println(WiFi.localIP());

    server.on("/", handleRoot);
    server.on("/virtualjoystick.js", handleJoystickJs);
    server.onNotFound(handleNotFound);

    server.begin();

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
}

void loop()
{
    webSocket.loop();
    server.handleClient();
}

/**
 * Speed will be 
 * 
 * @param leftSpeed left motor speed -100 to +100
 * @param rightSpeed right motor speed -100 to +100
 */
void setMotorSpeed(int leftSpeed, int rightSpeed)
{
    const int minSpeedTreshhold = 10;
    const int maxSpeedTreshhold = 100;

    const int minPwmSpeed = PWMRANGE / 4;
    const int maxPwmSpeed = PWMRANGE;

    if (abs(leftSpeed) < minSpeedTreshhold)
    {
        leftSpeed = 0;
    }

    if (abs(rightSpeed) < minSpeedTreshhold)
    {
        rightSpeed = 0;
    }

    digitalWrite(DIRA, leftSpeed < 0 ? HIGH : LOW);
    digitalWrite(DIRB, rightSpeed > 0 ? HIGH : LOW);

    if (leftSpeed)
    {
        analogWrite(PWMA, map(abs(leftSpeed), 0, maxSpeedTreshhold, minPwmSpeed, maxPwmSpeed));
    }
    else
    {
        analogWrite(PWMA, 0);
    }

    if (rightSpeed)
    {
        analogWrite(PWMB, map(abs(rightSpeed), 0, maxSpeedTreshhold, minPwmSpeed, maxPwmSpeed));
    }
    else
    {
        analogWrite(PWMB, 0);
    }
}

void robot_stop()
{
    setMotorSpeed(0, 0);
}

void robot_on()
{
    digitalWrite(POWER_PIN, HIGH);
}

void robot_off()
{
    digitalWrite(POWER_PIN, LOW);
}
