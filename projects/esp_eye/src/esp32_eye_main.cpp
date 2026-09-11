// esp32_eye_main.cpp
//
// Stage 0: camera bring-up for the ESP-EYE board.
// Confirms the camera initializes and streams frames.
// No detection logic yet — that comes once this is verified working.

#include <Arduino.h>
#include "esp_camera.h"

// ESP-EYE camera pin mapping
// (verified against Espressif's own CameraWebServer example)
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM     4
#define SIOD_GPIO_NUM    18
#define SIOC_GPIO_NUM    23
#define Y9_GPIO_NUM      36
#define Y8_GPIO_NUM      37
#define Y7_GPIO_NUM      38
#define Y6_GPIO_NUM      39
#define Y5_GPIO_NUM      35
#define Y4_GPIO_NUM      14
#define Y3_GPIO_NUM      13
#define Y2_GPIO_NUM      34
#define VSYNC_GPIO_NUM    5
#define HREF_GPIO_NUM    27
#define PCLK_GPIO_NUM    25

// Onboard status LED on the ESP-EYE
#define LED_PIN          21

static bool cameraReady = false;

static bool initCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;

  // Grayscale, low-res: this is the format the detection stage will
  // want anyway (fast frame-diff), so start bring-up with it.
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size   = FRAMESIZE_QQVGA;  // 160x120
  config.fb_count      = 2;
  config.fb_location   = CAMERA_FB_IN_PSRAM;
  config.grab_mode     = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed, error 0x%x\n", err);
    return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("esp32_eye: bring-up starting");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  cameraReady = initCamera();
  Serial.println(cameraReady ? "Camera ready" : "Camera init FAILED");
}

void loop() {
  if (!cameraReady) {
    delay(1000);
    return;
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Frame capture failed");
    delay(200);
    return;
  }

  Serial.printf("Frame: %ux%u, %u bytes, t=%lu ms\n",
                fb->width, fb->height, fb->len, millis());

  // Blink to show a frame was captured
  digitalWrite(LED_PIN, HIGH);
  esp_camera_fb_return(fb);
  digitalWrite(LED_PIN, LOW);

  delay(100);  // ~10 fps for this bring-up stage
}
