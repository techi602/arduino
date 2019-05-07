/*
 * http://home.kendra.com/mauser/Joystick.html
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

int dirA, dirB, pwmA, pwmB;
#define SPEED_MAX 500
#define SPEED_LOW 400

int xspeed = PWMRANGE;

// speed range from -100 to +100
int motor_speed_left = 0;
int motor_speed_right = 0;

static const char PROGMEM JOYSTICK_JS[] = R"rawliteral(
var VirtualJoystick = function (opts) {
            opts = opts || {};
            this._container = opts.container || document.body;
            this._strokeStyle = opts.strokeStyle || 'cyan';
            this._stickEl = opts.stickElement || this._buildJoystickStick();
            this._baseEl = opts.baseElement || this._buildJoystickBase();
            this._mouseSupport = opts.mouseSupport !== undefined ? opts.mouseSupport : false;
            this._stationaryBase = opts.stationaryBase || false;
            this._baseX = this._stickX = opts.baseX || 0
            this._baseY = this._stickY = opts.baseY || 0
            this._limitStickTravel = opts.limitStickTravel || false
            this._stickRadius = opts.stickRadius !== undefined ? opts.stickRadius : 100
            this._useCssTransform = opts.useCssTransform !== undefined ? opts.useCssTransform : false

            this._container.style.position = "relative"

            this._container.appendChild(this._baseEl)
            this._baseEl.style.position = "absolute"
            this._baseEl.style.display = "none"
            this._container.appendChild(this._stickEl)
            this._stickEl.style.position = "absolute"
            this._stickEl.style.display = "none"

            this._pressed = false;
            this._touchIdx = null;

            if (this._stationaryBase === true) {
                this._baseEl.style.display = "";
                this._baseEl.style.left = (this._baseX - this._baseEl.width / 2) + "px";
                this._baseEl.style.top = (this._baseY - this._baseEl.height / 2) + "px";
            }

            this._transform = this._useCssTransform ? this._getTransformProperty() : false;
            this._has3d = this._check3D();

            var __bind = function (fn, me) {
                return function () {
                    return fn.apply(me, arguments);
                };
            };
            this._$onTouchStart = __bind(this._onTouchStart, this);
            this._$onTouchEnd = __bind(this._onTouchEnd, this);
            this._$onTouchMove = __bind(this._onTouchMove, this);
            this._container.addEventListener('touchstart', this._$onTouchStart, false);
            this._container.addEventListener('touchend', this._$onTouchEnd, false);
            this._container.addEventListener('touchmove', this._$onTouchMove, false);
            if (this._mouseSupport) {
                this._$onMouseDown = __bind(this._onMouseDown, this);
                this._$onMouseUp = __bind(this._onMouseUp, this);
                this._$onMouseMove = __bind(this._onMouseMove, this);
                this._container.addEventListener('mousedown', this._$onMouseDown, false);
                this._container.addEventListener('mouseup', this._$onMouseUp, false);
                this._container.addEventListener('mousemove', this._$onMouseMove, false);
            }
        }

        VirtualJoystick.prototype.destroy = function () {
            this._container.removeChild(this._baseEl);
            this._container.removeChild(this._stickEl);

            this._container.removeEventListener('touchstart', this._$onTouchStart, false);
            this._container.removeEventListener('touchend', this._$onTouchEnd, false);
            this._container.removeEventListener('touchmove', this._$onTouchMove, false);
            if (this._mouseSupport) {
                this._container.removeEventListener('mouseup', this._$onMouseUp, false);
                this._container.removeEventListener('mousedown', this._$onMouseDown, false);
                this._container.removeEventListener('mousemove', this._$onMouseMove, false);
            }
        }

        /**
         * @returns {Boolean} true if touchscreen is currently available, false otherwise
         */
        VirtualJoystick.touchScreenAvailable = function () {
            return 'createTouch' in document ? true : false;
        }

        /**
         * microevents.js - https://github.com/jeromeetienne/microevent.js
         */
        ;(function (destObj) {
            destObj.addEventListener = function (event, fct) {
                if (this._events === undefined) this._events = {};
                this._events[event] = this._events[event] || [];
                this._events[event].push(fct);
                return fct;
            };
            destObj.removeEventListener = function (event, fct) {
                if (this._events === undefined) this._events = {};
                if (event in this._events === false) return;
                this._events[event].splice(this._events[event].indexOf(fct), 1);
            };
            destObj.dispatchEvent = function (event /* , args... */) {
                if (this._events === undefined) this._events = {};
                if (this._events[event] === undefined) return;
                var tmpArray = this._events[event].slice();
                for (var i = 0; i < tmpArray.length; i++) {
                    var result = tmpArray[i].apply(this, Array.prototype.slice.call(arguments, 1))
                    if (result !== undefined) return result;
                }
                return undefined
            };
        })(VirtualJoystick.prototype);

        //////////////////////////////////////////////////////////////////////////////////
        //                    //
        //////////////////////////////////////////////////////////////////////////////////

        VirtualJoystick.prototype.deltaX = function () {
            return (this._pressed ? this._stickX - this._baseX : 0)
        }
        VirtualJoystick.prototype.deltaY = function () {
            return (this._pressed ? this._stickY - this._baseY : 0)
        }

        VirtualJoystick.prototype.up = function () {
            if (this._pressed === false) return false;
            var deltaX = this.deltaX();
            var deltaY = this.deltaY();
            if (deltaY >= 0) return false;
            if (Math.abs(deltaX) > 2 * Math.abs(deltaY)) return false;
            return true;
        }
        VirtualJoystick.prototype.down = function () {
            if (this._pressed === false) return false;
            var deltaX = this.deltaX();
            var deltaY = this.deltaY();
            if (deltaY <= 0) return false;
            if (Math.abs(deltaX) > 2 * Math.abs(deltaY)) return false;
            return true;
        }
        VirtualJoystick.prototype.right = function () {
            if (this._pressed === false) return false;
            var deltaX = this.deltaX();
            var deltaY = this.deltaY();
            if (deltaX <= 0) return false;
            if (Math.abs(deltaY) > 2 * Math.abs(deltaX)) return false;
            return true;
        }
        VirtualJoystick.prototype.left = function () {
            if (this._pressed === false) return false;
            var deltaX = this.deltaX();
            var deltaY = this.deltaY();
            if (deltaX >= 0) return false;
            if (Math.abs(deltaY) > 2 * Math.abs(deltaX)) return false;
            return true;
        }

        //////////////////////////////////////////////////////////////////////////////////
        //                    //
        //////////////////////////////////////////////////////////////////////////////////

        VirtualJoystick.prototype._onUp = function () {
            this._pressed = false;
            this._stickEl.style.display = "none";

            if (this._stationaryBase == false) {
                this._baseEl.style.display = "none";

                this._baseX = this._baseY = 0;
                this._stickX = this._stickY = 0;
            }
        }

        VirtualJoystick.prototype._onDown = function (x, y) {
            this._pressed = true;
            if (this._stationaryBase == false) {
                this._baseX = x;
                this._baseY = y;
                this._baseEl.style.display = "";
                this._move(this._baseEl.style, (this._baseX - this._baseEl.width / 2), (this._baseY - this._baseEl.height / 2));
            }

            this._stickX = x;
            this._stickY = y;

            if (this._limitStickTravel === true) {
                var deltaX = this.deltaX();
                var deltaY = this.deltaY();
                var stickDistance = Math.sqrt((deltaX * deltaX) + (deltaY * deltaY));
                if (stickDistance > this._stickRadius) {
                    var stickNormalizedX = deltaX / stickDistance;
                    var stickNormalizedY = deltaY / stickDistance;

                    this._stickX = stickNormalizedX * this._stickRadius + this._baseX;
                    this._stickY = stickNormalizedY * this._stickRadius + this._baseY;
                }
            }

            this._stickEl.style.display = "";
            this._move(this._stickEl.style, (this._stickX - this._stickEl.width / 2), (this._stickY - this._stickEl.height / 2));
        }

        VirtualJoystick.prototype._onMove = function (x, y) {
            if (this._pressed === true) {
                this._stickX = x;
                this._stickY = y;

                if (this._limitStickTravel === true) {
                    var deltaX = this.deltaX();
                    var deltaY = this.deltaY();
                    var stickDistance = Math.sqrt((deltaX * deltaX) + (deltaY * deltaY));
                    if (stickDistance > this._stickRadius) {
                        var stickNormalizedX = deltaX / stickDistance;
                        var stickNormalizedY = deltaY / stickDistance;

                        this._stickX = stickNormalizedX * this._stickRadius + this._baseX;
                        this._stickY = stickNormalizedY * this._stickRadius + this._baseY;
                    }
                }

                this._move(this._stickEl.style, (this._stickX - this._stickEl.width / 2), (this._stickY - this._stickEl.height / 2));
            }
        }


        //////////////////////////////////////////////////////////////////////////////////
        //    bind touch events (and mouse events for debug)      //
        //////////////////////////////////////////////////////////////////////////////////

        VirtualJoystick.prototype._onMouseUp = function (event) {
            return this._onUp();
        }

        VirtualJoystick.prototype._onMouseDown = function (event) {
            event.preventDefault();
            var x = event.clientX;
            var y = event.clientY;
            return this._onDown(x, y);
        }

        VirtualJoystick.prototype._onMouseMove = function (event) {
            var x = event.clientX;
            var y = event.clientY;
            return this._onMove(x, y);
        }

        //////////////////////////////////////////////////////////////////////////////////
        //    comment               //
        //////////////////////////////////////////////////////////////////////////////////

        VirtualJoystick.prototype._onTouchStart = function (event) {
            // if there is already a touch inprogress do nothing
            if (this._touchIdx !== null) return;

            // notify event for validation
            var isValid = this.dispatchEvent('touchStartValidation', event);
            if (isValid === false) return;

            // dispatch touchStart
            this.dispatchEvent('touchStart', event);

            event.preventDefault();
            // get the first who changed
            var touch = event.changedTouches[0];
            // set the touchIdx of this joystick
            this._touchIdx = touch.identifier;

            // forward the action
            var x = touch.pageX;
            var y = touch.pageY;
            return this._onDown(x, y)
        }

        VirtualJoystick.prototype._onTouchEnd = function (event) {
            // if there is no touch in progress, do nothing
            if (this._touchIdx === null) return;

            // dispatch touchEnd
            this.dispatchEvent('touchEnd', event);

            // try to find our touch event
            var touchList = event.changedTouches;
            for (var i = 0; i < touchList.length && touchList[i].identifier !== this._touchIdx; i++) ;
            // if touch event isnt found,
            if (i === touchList.length) return;

            // reset touchIdx - mark it as no-touch-in-progress
            this._touchIdx = null;

//??????
// no preventDefault to get click event on ios
            event.preventDefault();

            return this._onUp()
        }

        VirtualJoystick.prototype._onTouchMove = function (event) {
            // if there is no touch in progress, do nothing
            if (this._touchIdx === null) return;

            // try to find our touch event
            var touchList = event.changedTouches;
            for (var i = 0; i < touchList.length && touchList[i].identifier !== this._touchIdx; i++) ;
            // if touch event with the proper identifier isnt found, do nothing
            if (i === touchList.length) return;
            var touch = touchList[i];

            event.preventDefault();

            var x = touch.pageX;
            var y = touch.pageY;
            return this._onMove(x, y)
        }


        //////////////////////////////////////////////////////////////////////////////////
        //    build default stickEl and baseEl        //
        //////////////////////////////////////////////////////////////////////////////////

        /**
         * build the canvas for joystick base
         */
        VirtualJoystick.prototype._buildJoystickBase = function () {
            var canvas = document.createElement('canvas');
            canvas.width = 126;
            canvas.height = 126;

            var ctx = canvas.getContext('2d');
            ctx.beginPath();
            ctx.strokeStyle = this._strokeStyle;
            ctx.lineWidth = 6;
            ctx.arc(canvas.width / 2, canvas.width / 2, 40, 0, Math.PI * 2, true);
            ctx.stroke();

            ctx.beginPath();
            ctx.strokeStyle = this._strokeStyle;
            ctx.lineWidth = 2;
            ctx.arc(canvas.width / 2, canvas.width / 2, 60, 0, Math.PI * 2, true);
            ctx.stroke();

            return canvas;
        }

        /**
         * build the canvas for joystick stick
         */
        VirtualJoystick.prototype._buildJoystickStick = function () {
            var canvas = document.createElement('canvas');
            canvas.width = 86;
            canvas.height = 86;
            var ctx = canvas.getContext('2d');
            ctx.beginPath();
            ctx.strokeStyle = this._strokeStyle;
            ctx.lineWidth = 6;
            ctx.arc(canvas.width / 2, canvas.width / 2, 40, 0, Math.PI * 2, true);
            ctx.stroke();
            return canvas;
        }

        //////////////////////////////////////////////////////////////////////////////////
        //    move using translate3d method with fallback to translate > 'top' and 'left'
        //      modified from https://github.com/component/translate and dependents
        //////////////////////////////////////////////////////////////////////////////////

        VirtualJoystick.prototype._move = function (style, x, y) {
            if (this._transform) {
                if (this._has3d) {
                    style[this._transform] = 'translate3d(' + x + 'px,' + y + 'px, 0)';
                } else {
                    style[this._transform] = 'translate(' + x + 'px,' + y + 'px)';
                }
            } else {
                style.left = x + 'px';
                style.top = y + 'px';
            }
        }

        VirtualJoystick.prototype._getTransformProperty = function () {
            var styles = [
                'webkitTransform',
                'MozTransform',
                'msTransform',
                'OTransform',
                'transform'
            ];

            var el = document.createElement('p');
            var style;

            for (var i = 0; i < styles.length; i++) {
                style = styles[i];
                if (null != el.style[style]) {
                    return style;
                }
            }
        }

        VirtualJoystick.prototype._check3D = function () {
            var prop = this._getTransformProperty();
            // IE8<= doesn't have `getComputedStyle`
            if (!prop || !window.getComputedStyle) return module.exports = false;

            var map = {
                webkitTransform: '-webkit-transform',
                OTransform: '-o-transform',
                msTransform: '-ms-transform',
                MozTransform: '-moz-transform',
                transform: 'transform'
            };

            // from: https://gist.github.com/lorenzopolidori/3794226
            var el = document.createElement('div');
            el.style[prop] = 'translate3d(1px,1px,1px)';
            document.body.insertBefore(el, null);
            var val = getComputedStyle(el).getPropertyValue(map[prop]);
            document.body.removeChild(el);
            var exports = null != val && val.length && 'none' != val;
            return exports;
        }

)rawliteral";

static const char PROGMEM INDEX_HTML[] = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=0">
    <title>T100 control center</title>
    <link rel="icon" href="data:;base64,iVBORw0KGgo=">
    <script src="/virtualjoystick.js"></script>
    <script>

        var joystickX = 0;
        var joystickY = 0;
        var padIndex = 0;

        var websock;
        var joystick;

        var virtualJoystickPressed = false;

        var debugText;

        var powerOn = false;

        document.addEventListener('mouseup', function () {
          joystickMove(0,0);
        });


        document.addEventListener('DOMContentLoaded', function () {

            debugText = document.getElementById('debug');

            if (window.location.hostname) {
                websock = new WebSocket('ws://' + window.location.hostname + ':81/');
            }
            if (!websock) {
                console.log('Unable to connect to websocket server!');
            } else {
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
            }

            joystick = new VirtualJoystick({
                mouseSupport: true,
                stationaryBase: true,
                baseX: 200,
                baseY: 200,
                limitStickTravel: true,
                stickRadius: 100
            });
            joystick.addEventListener('touchStart', function(){
                virtualJoystickPressed = true;
            });
            joystick.addEventListener('touchEnd', function(){
                virtualJoystickPressed = false;
            });

            
            window.requestAnimationFrame(readGamepad);

        }, false);


        function readGamepad(timestamp) {
            var pads = navigator.getGamepads();
            var fireMoveEvent = false;
            var x, y;

            if (joystick.deltaX() || joystick.deltaY()) {
                fireMoveEvent = false;

                x = Math.round(joystick.deltaX());
                y = Math.round(joystick.deltaY());
                if (x !== joystickX) {
                    fireMoveEvent = true;
                    joystickX = x;
                }

                if (y !== joystickY) {
                    fireMoveEvent = true;
                    joystickY = y;
                }
           } else {
                if (pads.length > 0) {
                    // get first pad
                    var pad = pads[padIndex];

                    if (pad) {
                        if (pad.buttons.length >= 2) {
                           if (pad.buttons[0].pressed) {
                            powerOn = true;
                             switchPower();
                           }

                           if (pad.buttons[1].pressed) {
                            powerOn = false;
                             switchPower();
                           }
                        }

                      
                        if (pad.axes.length >= 2) {
                          x = Math.round(pad.axes[0] * 100);
                          y = Math.round(pad.axes[1] * 100);
  
                          if (x !== joystickX) {
                              fireMoveEvent = true;
                              joystickX = x;
                          }
  
                          if (y !== joystickY) {
                              fireMoveEvent = true;
                              joystickY = y;
                          }
                       }
                    }
                }
            }

            if (fireMoveEvent) {
                joystickMove(x, y);
            }

            window.requestAnimationFrame(readGamepad);
        }

        function joystickMove(x, y) {

            var
                v = (100 - Math.abs(x)) * (y / 100) + y,
                w = (100 - Math.abs(y)) * (x / 100) + x,
                r = Math.round((v + w) / 2),
                l = Math.round((v - w) / 2);

            var text = 'X:' + x + '<br>Y:' + y + '<br>V:' + Math.round(v) + '<br>W:' + Math.round(w) + '<br>R:' + r + '<br>L:' + l;
            console.log(text);
            debugText.innerHTML = text;

            if (websock && websock.readyState == 1) {
                websock.send('motor:' + l + ':' + r);
            }
        }

        function websockSend(str_command)
        {
          if (websock && websock.readyState == 1) {
                websock.send(str_command);
            }
        }

        function switchPower()
        {
          websockSend(powerOn ? "on" : "off");
        }


    </script>
</head>
<body>

<form>
<input type="button" id="on" value="on" onclick="websockSend(this.id)">
<input type="button" id="off" value="off" onclick="websockSend(this.id)">
</form>

<pre id="debug"></pre>

</body>
</html>


)HTML";

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    Serial.printf("webSocketEvent(%d, %d, ...)\r\n", num, type);
    switch (type)
    {
    case WStype_DISCONNECTED:
        Serial.printf("[%u] Disconnected!\r\n", num);
        oled.clearDisplay();
        oled.putString("Client disconnected");
        break;
    case WStype_CONNECTED:
    {
      char buffer[40];
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
    }
    break;
    case WStype_TEXT:
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
        else if (strcmp("go", (const char *)payload) == 0)
        {
            robot_go();
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
        else if (strcmp("left", (const char *)payload) == 0)
        {
            robot_left();
        }
        else if (strcmp("right", (const char *)payload) == 0)
        {
            robot_right();
        }
        else if (strcmp("back", (const char *)payload) == 0)
        {
            robot_back();
        }
        else
        {
            Serial.print("Unknown command: ");
            Serial.println((const char *)payload);
        }
        // send data to all connected clients
        //webSocket.broadcastTXT(payload, length);
        break;
    case WStype_BIN:
        Serial.printf("[%u] get binary length: %u\r\n", num, length);
        //hexdump(payload, length);

        // echo data back to browser
        //webSocket.sendBIN(num, payload, length);
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
 return String(address[0]) + "." + 
        String(address[1]) + "." + 
        String(address[2]) + "." + 
        String(address[3]);
}

void setup(void)
{
    // Start Serial
    Serial.begin(115200);

    pinMode(DIRA, OUTPUT);
    pinMode(DIRB, OUTPUT);
    pinMode(PWMA, OUTPUT);
    pinMode(PWMB, OUTPUT);
    pinMode(POWER_PIN, OUTPUT);
    // pinMode(HEADLIGHT, OUTPUT);

    analogWrite(PWMA, 0);
    analogWrite(PWMB, 0);

    digitalWrite(DIRA, HIGH);
    digitalWrite(DIRB, LOW);

    digitalWrite(POWER_PIN, LOW);
    //  digitalWrite(HEADLIGHT, HIGH);

    Serial.println();
    Serial.println();
    Serial.println();
    Serial.println("HELLO");

Wire.begin();  
  oled.init();                      // Initialze SSD1306 OLED display
  oled.clearDisplay();              // Clear screen
  oled.setTextXY(0,0);
    // Use full 256 char 'Code Page 437' font


oled.putString("Connecting to Wi-Fi...");
  

// my mobile hotspot
    WiFiMulti.addAP("AndroidAP", "uche7888");
    WiFiMulti.addAP(ssid, password);

    while (WiFiMulti.run() != WL_CONNECTED)
    {
        //oled.write('.');
        //oled.display();
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

void motor()
{
    digitalWrite(DIRA, dirA);
    digitalWrite(DIRB, dirB);
    analogWrite(PWMA, xspeed);
    analogWrite(PWMB, xspeed);
}

int robot_left()
{
    dirA = LOW;
    dirB = LOW;

    motor();

    return 1;
}

int robot_right()
{
    dirA = HIGH;
    dirB = HIGH;

    motor();
    return 1;
}

int robot_go()
{
    dirA = HIGH;
    dirB = LOW;

    if (xspeed == 0)
    {
        xspeed = PWMRANGE;
    }

    motor();
    return 1;
}

int robot_back()
{
    dirA = LOW;
    dirB = HIGH;

    motor();
    return 1;
}

int robot_stop()
{
    dirA = HIGH;
    dirB = LOW;
    xspeed = 0;
    motor();

    return 1;
}

void robot_on()
{
    digitalWrite(POWER_PIN, HIGH);
}

void robot_off()
{
    digitalWrite(POWER_PIN, LOW);
}
