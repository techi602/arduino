#include <Arduino.h>
#include <WiFi.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include <cstring>
#include "wifi_config.h"

static bool camera_ready = false;

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

static esp_err_t index_handler(httpd_req_t *request) {
  static const char page[] =
      "<!doctype html><html><head><meta name=viewport content='width=device-width'>"
      "<title>ESP32-CAM Live</title><style>body{font-family:Arial;background:#111;color:#eee;"
      "text-align:center;margin:1em}img{width:min(100%,960px);border-radius:8px}"
      "input{width:5em}button{margin:.3em;padding:.5em}pre{max-width:960px;margin:auto;text-align:left;"
      "background:#222;padding:1em;overflow:auto}</style></head>"
      "<body><h1>ESP32-CAM Live</h1><img src='/stream' alt='Live camera stream'>"
      "<p>Quality <input id=q type=number min=10 max=63 value=12>"
      " Brightness <input id=b type=number min=-2 max=2 value=0>"
      " Contrast <input id=c type=number min=-2 max=2 value=0>"
      " <button onclick=save()>Apply</button></p><pre id=s>Loading...</pre>"
      "<script>async function refresh(){let r=await fetch('/api/status');let j=await r.json();"
      "s.textContent=JSON.stringify(j,null,2)}async function save(){await fetch('/api/settings',"
      "{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({jpeg_quality:+q.value,"
      "brightness:+b.value,contrast:+c.value})});refresh()}refresh();setInterval(refresh,3000)</script>"
      "</body></html>";
  httpd_resp_set_type(request, "text/html");
  return httpd_resp_send(request, page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t capture_handler(httpd_req_t *request) {
  camera_fb_t *frame = esp_camera_fb_get();
  if (frame == nullptr) {
    httpd_resp_send_500(request);
    return ESP_FAIL;
  }

  httpd_resp_set_type(request, "image/jpeg");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  esp_err_t result = httpd_resp_send(request, reinterpret_cast<const char *>(frame->buf), frame->len);
  esp_camera_fb_return(frame);
  return result;
}

static esp_err_t stream_handler(httpd_req_t *request) {
  static const char *CONTENT_TYPE = "multipart/x-mixed-replace;boundary=frame";
  static const char *BOUNDARY = "\r\n--frame\r\n";
  char header[64];

  httpd_resp_set_type(request, CONTENT_TYPE);
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");

  while (true) {
    camera_fb_t *frame = esp_camera_fb_get();
    if (frame == nullptr) {
      return ESP_FAIL;
    }

    const int header_length = snprintf(header, sizeof(header),
                                       "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                                       static_cast<unsigned>(frame->len));
    esp_err_t result = httpd_resp_send_chunk(request, BOUNDARY, strlen(BOUNDARY));
    if (result == ESP_OK) {
      result = httpd_resp_send_chunk(request, header, header_length);
    }
    if (result == ESP_OK) {
      result = httpd_resp_send_chunk(request, reinterpret_cast<const char *>(frame->buf), frame->len);
    }
    esp_camera_fb_return(frame);
    if (result != ESP_OK) {
      return result;
    }
    vTaskDelay(pdMS_TO_TICKS(30));
  }
}

static int json_int(const char *body, const char *name, int fallback) {
  const char *key = strstr(body, name);
  if (key == nullptr) {
    return fallback;
  }
  key = strchr(key, ':');
  return key == nullptr ? fallback : atoi(key + 1);
}

static sensor_t *get_camera_sensor() {
  return esp_camera_sensor_get();
}

static esp_err_t health_handler(httpd_req_t *request) {
  char response[192];
  snprintf(response, sizeof(response),
           "{\"wifi\":%s,\"camera\":%s,\"stream\":%s,\"ip\":\"%s\"}",
           WiFi.status() == WL_CONNECTED ? "true" : "false",
           camera_ready ? "true" : "false", camera_ready ? "true" : "false",
           WiFi.localIP().toString().c_str());
  httpd_resp_set_type(request, "application/json");
  return httpd_resp_send(request, response, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_handler(httpd_req_t *request) {
  sensor_t *sensor = get_camera_sensor();
  camera_status_t status = sensor == nullptr ? camera_status_t{} : sensor->status;
  char response[512];
  snprintf(response, sizeof(response),
           "{\"uptime_ms\":%lu,\"wifi\":{\"connected\":%s,\"rssi\":%d,\"ip\":\"%s\"},"
           "\"heap_free\":%lu,\"psram_free\":%lu,\"camera\":%s,"
           "\"settings\":{\"frame_size\":%d,\"jpeg_quality\":%d,\"brightness\":%d,"
           "\"contrast\":%d,\"saturation\":%d,\"hmirror\":%s,\"vflip\":%s}}",
           millis(), WiFi.status() == WL_CONNECTED ? "true" : "false", WiFi.RSSI(),
           WiFi.localIP().toString().c_str(), ESP.getFreeHeap(), ESP.getFreePsram(),
           camera_ready ? "true" : "false", status.framesize, status.quality,
           status.brightness, status.contrast, status.saturation,
           status.hmirror ? "true" : "false", status.vflip ? "true" : "false");
  httpd_resp_set_type(request, "application/json");
  return httpd_resp_send(request, response, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t settings_handler(httpd_req_t *request) {
  sensor_t *sensor = get_camera_sensor();
  if (sensor == nullptr) {
    httpd_resp_send_500(request);
    return ESP_FAIL;
  }

  if (request->method == HTTP_POST) {
    const size_t length = request->content_len;
    char *body = new char[length + 1];
    if (body == nullptr || httpd_req_recv(request, body, length) != static_cast<int>(length)) {
      delete[] body;
      httpd_resp_send_500(request);
      return ESP_FAIL;
    }
    body[length] = '\0';
    if (strstr(body, "frame_size")) sensor->set_framesize(sensor, static_cast<framesize_t>(json_int(body, "frame_size", sensor->status.framesize)));
    if (strstr(body, "jpeg_quality")) sensor->set_quality(sensor, json_int(body, "jpeg_quality", sensor->status.quality));
    if (strstr(body, "brightness")) sensor->set_brightness(sensor, json_int(body, "brightness", sensor->status.brightness));
    if (strstr(body, "contrast")) sensor->set_contrast(sensor, json_int(body, "contrast", sensor->status.contrast));
    if (strstr(body, "saturation")) sensor->set_saturation(sensor, json_int(body, "saturation", sensor->status.saturation));
    if (strstr(body, "hmirror")) sensor->set_hmirror(sensor, json_int(body, "hmirror", sensor->status.hmirror ? 1 : 0) != 0);
    if (strstr(body, "vflip")) sensor->set_vflip(sensor, json_int(body, "vflip", sensor->status.vflip ? 1 : 0) != 0);
    delete[] body;
  }

  return status_handler(request);
}

static void start_web_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  httpd_handle_t server = nullptr;
  if (httpd_start(&server, &config) != ESP_OK) {
    Serial.println("Web server start failed");
    return;
  }

  httpd_uri_t index_uri = {.uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = nullptr};
  httpd_uri_t capture_uri = {.uri = "/capture", .method = HTTP_GET, .handler = capture_handler, .user_ctx = nullptr};
  httpd_uri_t stream_uri = {.uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = nullptr};
  httpd_uri_t health_uri = {.uri = "/api/health", .method = HTTP_GET, .handler = health_handler, .user_ctx = nullptr};
  httpd_uri_t status_uri = {.uri = "/api/status", .method = HTTP_GET, .handler = status_handler, .user_ctx = nullptr};
  httpd_uri_t settings_get_uri = {.uri = "/api/settings", .method = HTTP_GET, .handler = settings_handler, .user_ctx = nullptr};
  httpd_uri_t settings_post_uri = {.uri = "/api/settings", .method = HTTP_POST, .handler = settings_handler, .user_ctx = nullptr};
  httpd_register_uri_handler(server, &index_uri);
  httpd_register_uri_handler(server, &capture_uri);
  httpd_register_uri_handler(server, &stream_uri);
  httpd_register_uri_handler(server, &health_uri);
  httpd_register_uri_handler(server, &status_uri);
  httpd_register_uri_handler(server, &settings_get_uri);
  httpd_register_uri_handler(server, &settings_post_uri);
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  const bool has_psram = psramFound();
  Serial.print("PSRAM: ");
  Serial.println(has_psram ? "available" : "not available");

  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = has_psram ? FRAMESIZE_VGA : FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  config.fb_location = has_psram ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera initialization failed");
    return;
  }
  camera_ready = true;

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  Serial.print("Camera URL: http://");
  Serial.println(WiFi.localIP());
  start_web_server();
}

void loop() {
  delay(1000);
}