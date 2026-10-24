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
#include <LittleFS.h>

#else

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>

#endif

#include <Arduino.h>
#include <WebSocketsServer.h>
#include <stdlib.h>
#include <string.h>
#include <Wire.h>
#include <ACROBOTIC_SSD1306.h>

#include "config.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#ifndef WEBSOCKETS_SERVER_CLIENT_MAX
#define WEBSOCKETS_SERVER_CLIENT_MAX 5
#endif

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
char buffer[128];

#ifdef ESP32
//MDNSResponder mdns;
#define mdns MDNS
WiFiMulti WiFiMulti;
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
#else
MDNSResponder mdns;
ESP8266WiFiMulti WiFiMulti;
ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
#endif

// speed range from -100 to +100
int motor_speed_left = 0;
int motor_speed_right = 0;
bool robot_powered = false;
int controller_id = -1;
unsigned long last_motor_command = 0;
bool authenticated[WEBSOCKETS_SERVER_CLIENT_MAX] = {};

bool parseMotorCommand(const uint8_t *payload, size_t length, int &leftSpeed, int &rightSpeed)
{
    const size_t prefixLength = 6;
    const size_t maxCommandLength = 32;

    if (length < prefixLength || length >= maxCommandLength)
    {
        return false;
    }

    char command[maxCommandLength];
    memcpy(command, payload, length);
    command[length] = '\0';

    if (strncmp(command, "motor:", prefixLength) != 0)
    {
        return false;
    }

    char *end;
    long parsedLeft = strtol(command + prefixLength, &end, 10);
    if (end == command + prefixLength || *end != ':')
    {
        return false;
    }

    char *rightStart = end + 1;
    long parsedRight = strtol(rightStart, &end, 10);
    if (end == rightStart || *end != '\0')
    {
        return false;
    }

    leftSpeed = constrain(parsedLeft, -100L, 100L);
    rightSpeed = constrain(parsedRight, -100L, 100L);
    return true;
}

bool isCommand(const uint8_t *payload, size_t length, const char *command)
{
    size_t commandLength = strlen(command);
    return length == commandLength && memcmp(payload, command, commandLength) == 0;
}

bool hasControl(uint8_t client)
{
    return controller_id == client && authenticated[client];
}

bool tokenMatches(const uint8_t *payload, size_t length)
{
    const size_t prefixLength = 5;
    const size_t tokenLength = strlen(CONTROL_TOKEN);
    return length == prefixLength + tokenLength &&
           memcmp(payload, "auth:", prefixLength) == 0 &&
           memcmp(payload + prefixLength, CONTROL_TOKEN, tokenLength) == 0;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    Serial.printf("webSocketEvent(%d, %d, ...)\r\n", num, type);
    switch (type)
    {
        case WStype_DISCONNECTED:
        {
            Serial.printf("[%u] Disconnected!\r\n", num);
            authenticated[num] = false;
            if (controller_id == num)
            {
                controller_id = -1;
                robot_off();
            }
            oled.clearDisplay();
            oled.putString("Client disconnected");
            break;
        }
        case WStype_CONNECTED:
        {
            authenticated[num] = CONTROL_TOKEN[0] == '\0';
            if (controller_id < 0 && authenticated[num])
            {
                controller_id = num;
            }
            IPAddress ip = webSocket.remoteIP(num);
            snprintf(buffer, sizeof(buffer), "[%u] Connected from %d.%d.%d.%d url: %.*s\r\n", num, ip[0], ip[1], ip[2], ip[3], (int)length, payload);
            Serial.printf("%s", buffer);
            
            // Send the current LED status
            webSocket.sendTXT(num, "HELLO", 5);
    
            oled.clearDisplay();
            oled.setTextXY(0,0);
            oled.putString("Client connected");
            oled.setTextXY(1,0);
            oled.putString(buffer);
            break;
        }
        case WStype_TEXT:
        {
            Serial.printf("[%u] get Text length: %u\r\n", num, length);

            if (isCommand(payload, length, "auth") ||
                (length >= 5 && memcmp(payload, "auth:", 5) == 0))
            {
                if (CONTROL_TOKEN[0] == '\0' || tokenMatches(payload, length))
                {
                    authenticated[num] = true;
                    if (controller_id < 0)
                    {
                        controller_id = num;
                    }
                    webSocket.sendTXT(num, "AUTH_OK");
                }
                else
                {
                    authenticated[num] = false;
                    webSocket.sendTXT(num, "AUTH_FAILED");
                }
                break;
            }

            if (!hasControl(num))
            {
                Serial.println("Command rejected: client is not the controller");
                break;
            }
    
            // motor:<leftspeed>:<rightspeed>
            if (length >= 5 && memcmp(payload, "motor", 5) == 0)
            {
                if (robot_powered && parseMotorCommand(payload, length, motor_speed_left, motor_speed_right))
                {
                    setMotorSpeed(motor_speed_left, motor_speed_right);
                    last_motor_command = millis();
                    Serial.print("Setting motor speed ");
                    Serial.print(motor_speed_left);
                    Serial.print(' ');
                    Serial.println(motor_speed_right);
                }
                else
                {
                    Serial.println(robot_powered ? "Invalid motor command" : "Motor command rejected while powered off");
                }

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
            else if (isCommand(payload, length, "on"))
            {
                robot_on();
                oled.setTextXY(0,0);
                oled.putString("FIRE!        ");
            }
            else if (isCommand(payload, length, "off"))
            {
                robot_off();
            }
    
            else if (isCommand(payload, length, "stop"))
            {
                robot_stop();
            }
            else
            {
                Serial.print("Unknown command: ");
                Serial.write(payload, length);
                Serial.println();
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
    File page = LittleFS.open("/index.html", "r");
    if (!page)
    {
        server.send(503, "text/plain", "Web UI is not installed. Upload the data filesystem first.");
        return;
    }
    server.streamFile(page, "text/html");
    page.close();
}

void handleJoystickJs()
{
    File script = LittleFS.open("/joystick.js", "r");
    if (!script)
    {
        server.send(404, "text/plain", "joystick.js is not installed");
        return;
    }
    server.streamFile(script, "text/javascript");
    script.close();
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

#ifdef ESP32
    analogWriteResolution(PWMA, 10);
    analogWriteResolution(PWMB, 10);
    analogWriteFrequency(PWMA, 1000);
    analogWriteFrequency(PWMB, 1000);
#else
    analogWriteRange(PWMRANGE);
    analogWriteFreq(1000);
#endif

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

    if (!LittleFS.begin())
    {
        Serial.println("LittleFS mount failed");
    }

    // Initialze SSD1306 OLED display
    oled.init();
    oled.clearDisplay();
    oled.setTextXY(0,0);

    oled.putString("Connecting to Wi-Fi...");
  
    if (ssid[0] != '\0')
    {
        WiFiMulti.addAP(ssid, password);
    }

    const unsigned long wifiTimeout = T100_WIFI_CONNECT_TIMEOUT_MS;
    unsigned long wifiStart = millis();
    while (WiFiMulti.run() != WL_CONNECTED && millis() - wifiStart < wifiTimeout)
    {
        //oled.write('.');
        Serial.print(".");
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED)
    {
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

#ifdef ESP32
        if (mdns.begin(DEVICE_HOSTNAME))
#else
        if (mdns.begin(DEVICE_HOSTNAME, WiFi.localIP()))
#endif
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
    }
    else
    {
        Serial.println("");
        Serial.println("Wi-Fi unavailable; continuing without network");
        oled.clearDisplay();
        oled.setTextXY(0, 0);
        oled.putString("Wi-Fi unavailable");
    }

    server.on("/", handleRoot);
    server.on("/virtualjoystick.js", handleJoystickJs);
    server.onNotFound(handleNotFound);

    server.begin();

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    if (CONTROL_TOKEN[0] == '\0')
    {
        Serial.println("WARNING: WebSocket token authentication is disabled");
    }
}

void loop()
{
    WiFiMulti.run();
    webSocket.loop();
    server.handleClient();

    if (robot_powered && millis() - last_motor_command > MOTOR_COMMAND_TIMEOUT_MS)
    {
        robot_stop();
    }
}

/**
 * Speed will be 
 * 
 * @param leftSpeed left motor speed -100 to +100
 * @param rightSpeed right motor speed -100 to +100
 */
void setMotorSpeed(int leftSpeed, int rightSpeed)
{
    const int minSpeedThreshold = MOTOR_MIN_SPEED;
    const int maxSpeedThreshold = MOTOR_MAX_SPEED;

    leftSpeed = constrain(leftSpeed, -maxSpeedThreshold, maxSpeedThreshold);
    rightSpeed = constrain(rightSpeed, -maxSpeedThreshold, maxSpeedThreshold);

    const int minPwmSpeed = PWMRANGE / 4;
    const int maxPwmSpeed = PWMRANGE;

    if (abs(leftSpeed) < minSpeedThreshold)
    {
        leftSpeed = 0;
    }

    if (abs(rightSpeed) < minSpeedThreshold)
    {
        rightSpeed = 0;
    }

    digitalWrite(DIRA, leftSpeed < 0 ? HIGH : LOW);
    digitalWrite(DIRB, rightSpeed > 0 ? HIGH : LOW);

    if (leftSpeed)
    {
        analogWrite(PWMA, map(abs(leftSpeed), 0, maxSpeedThreshold, minPwmSpeed, maxPwmSpeed));
    }
    else
    {
        analogWrite(PWMA, 0);
    }

    if (rightSpeed)
    {
        analogWrite(PWMB, map(abs(rightSpeed), 0, maxSpeedThreshold, minPwmSpeed, maxPwmSpeed));
    }
    else
    {
        analogWrite(PWMB, 0);
    }
}

void robot_stop()
{
    motor_speed_left = 0;
    motor_speed_right = 0;
    setMotorSpeed(0, 0);
}

void robot_on()
{
    robot_stop();
    last_motor_command = millis();
    robot_powered = true;
    digitalWrite(POWER_PIN, HIGH);
}

void robot_off()
{
    robot_stop();
    robot_powered = false;
    digitalWrite(POWER_PIN, LOW);
}
