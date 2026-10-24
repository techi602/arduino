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
        var lastGamepadButtons = [];
        var reconnectTimer;

        document.addEventListener('mouseup', function () {
          joystickMove(0,0);
        });


        document.addEventListener('DOMContentLoaded', function () {

            debugText = document.getElementById('debug');

            connectWebSocket();

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
                joystickMove(0, 0);
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
                           if (pad.buttons[0].pressed && !lastGamepadButtons[0]) {
                            powerOn = true;
                             switchPower();
                           }

                           if (pad.buttons[1].pressed && !lastGamepadButtons[1]) {
                            powerOn = false;
                             switchPower();
                           }
                           lastGamepadButtons = pad.buttons.map(function(button) { return button.pressed; });
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

        function connectWebSocket() {
            if (!window.location.hostname) return;
            var protocol = window.location.protocol === 'https:' ? 'wss://' : 'ws://';
            websock = new WebSocket(protocol + window.location.hostname + ':81/');
            websock.onopen = function () {
                console.log('websock open');
            };
            websock.onclose = function () {
                joystickMove(0, 0);
                console.log('websock close');
                clearTimeout(reconnectTimer);
                reconnectTimer = setTimeout(connectWebSocket, 1000);
            };
            websock.onerror = function (evt) { console.log(evt); };
            websock.onmessage = function (evt) { console.log(evt); };
        }

        function switchPower()
        {
          websockSend(powerOn ? "on" : "off");
        }

        document.addEventListener('visibilitychange', function () {
            if (document.hidden) {
                joystickMove(0, 0);
                websockSend('stop');
            }
        });


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