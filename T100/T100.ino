#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// WiFi parameters
#include "arduino_secrets.h"
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

MDNSResponder mdns;

ESP8266WiFiMulti WiFiMulti;

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

#define PWMA D6
#define PWMB D4
#define DIRA D7
#define DIRB D5

int dirA, dirB, pwmA, pwmB;
#define SPEED_MAX 500
#define SPEED_LOW 400

int xspeed = PWMRANGE;


static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=0">
    <title>T100 control center</title>
    <link rel="icon" href="data:;base64,iVBORw0KGgo=">
    <style type="text/css">
        body {
            background-color: #808080;
            Color: #000000;
        }
    </style>
    <script>
        var websock;

        document.addEventListener('DOMContentLoaded', function() {
            websock = new WebSocket('ws://' + window.location.hostname + ':81/');
            if (!websock) {
                console.log('Unable to connect to websocket server!');
            }
            websock.onopen = function (evt) {
                console.log('websock open');
            };
            websock.onclose = function (evt) {
                console.log('websock close');
            };
            websock.onerror = function (evt) {
                console.log(evt);
            };
            websock.onmessage = function (evt) {
                console.log(evt);
            };
        }, false);

        document.onkeydown = function (e) {
            switch (e.key) {
                case ' ':
                    e.preventDefault();
                    websock.send("stop");
                    break;

                case 'ArrowLeft':
                    e.preventDefault();
                    websock.send("left");
                    break;

                case 'ArrowUp':
                    e.preventDefault();
                    websock.send("go");
                    break;

                case 'ArrowRight':
                    e.preventDefault();
                    websock.send("right");
                    break;

                case 'ArrowDown':
                    e.preventDefault();
                    websock.send("back");
                    break;
            }
        };

        function buttonclick(e) {
            websock.send(e.id);
        }
    </script>
</head>
<body>
<h1>T100 control center</h1>
<button id="left" type="button" onclick="buttonclick(this);">LEFT</button>
<button id="right" type="button" onclick="buttonclick(this);">RIGHT</button>
<button id="go" type="button" onclick="buttonclick(this);">GO</button>
<button id="back" type="button" onclick="buttonclick(this);">BACK</button>
<button id="stop" type="button" onclick="buttonclick(this);">STOP</button>
</body>
</html>

)rawliteral";


void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
  Serial.printf("webSocketEvent(%d, %d, ...)\r\n", num, type);
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\r\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        // Send the current LED status
        webSocket.sendTXT(0, "HELLO", 5);
      }
      break;
    case WStype_TEXT:
      Serial.printf("[%u] get Text: %s\r\n", num, payload);

      if (strcmp("go", (const char *)payload) == 0) {
        robot_go();
      }
      else if (strcmp("stop", (const char *)payload) == 0) {
        robot_stop();
      }
      else if (strcmp("left", (const char *)payload) == 0) {
        robot_left();
      }
      else if (strcmp("right", (const char *)payload) == 0) {
        robot_right();
      }
      else if (strcmp("back", (const char *)payload) == 0) {
        robot_back();
      }
      else {
        Serial.print("Unknown command");
        Serial.println((const char *)payload);
      }
      // send data to all connected clients
      webSocket.broadcastTXT(payload, length);
      break;
    case WStype_BIN:
      Serial.printf("[%u] get binary length: %u\r\n", num, length);
      hexdump(payload, length);

      // echo data back to browser
      webSocket.sendBIN(num, payload, length);
      break;
    default:
      Serial.printf("Invalid WStype [%d]\r\n", type);
      break;
  }
}


void handleRoot()
{
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}


void setup(void)
{
  // Start Serial
  Serial.begin(115200);

  pinMode(DIRA, OUTPUT);
  pinMode(DIRB, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(PWMB, OUTPUT);
  // pinMode(HEADLIGHT, OUTPUT);


  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);

  digitalWrite(DIRA, HIGH);
  digitalWrite(DIRB, LOW);
  //  digitalWrite(HEADLIGHT, HIGH);

Serial.println();
  Serial.println();
  Serial.println();
  Serial.println("HELLO");

  WiFiMulti.addAP("AndroidAP", "uche7888");
  WiFiMulti.addAP(ssid, password);

  while(WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (mdns.begin("t100", WiFi.localIP())) {
    Serial.println("MDNS responder started");
    mdns.addService("http", "tcp", 80);
    mdns.addService("ws", "tcp", 81);
  }
  else {
    Serial.println("MDNS.begin failed");
  }
  Serial.print("Connect to http://t100.local");
  Serial.println("");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);

  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

}

void loop() {
  webSocket.loop();
  server.handleClient();

/*  
  if (Serial.available() > 0) {
    // read the incoming byte:
    char incomingByte = Serial.read();

    switch (incomingByte) {
      case 'A':
        Serial.println("left");
        robot_left();
        break;
      case 'D':
        Serial.println("right");
        robot_right();
        break;
      case 'W':
        Serial.println("go");
        robot_go();
        break;
      case 'S':
        Serial.println("back");
        robot_back();
        break;
      case 'X':
        Serial.println("stop");
        xspeed = 0;
        robot_stop();
        break;
      case '0':
        xspeed = 0;
        break;
      case '1':
        xspeed = 500;
        break;
      case '2':
        xspeed = 750;
        break;
      case '3':
        xspeed = 1023;
        break;
    }
  }
  */

}


void motor()
{
  digitalWrite(DIRA, dirA);
  digitalWrite(DIRB, dirB);
  analogWrite(PWMA, xspeed);
  analogWrite(PWMB, xspeed);
}

int robot_left() {
  dirA = LOW;
  dirB = LOW;


  motor();

  return 1;
}

int robot_right() {
  dirA = HIGH;
  dirB = HIGH;


  motor();
  return 1;
}

int robot_go() {
  dirA = HIGH;
  dirB = LOW;

  if (xspeed == 0)  {
    xspeed = PWMRANGE;
  }

  motor();
  return 1;
}


int robot_back() {
  dirA = LOW;
  dirB = HIGH;

  motor();
  return 1;
}

int robot_stop() {
  dirA = HIGH;
  dirB = LOW;
  xspeed = 0;
  motor();

  return 1;
}
