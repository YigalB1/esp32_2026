// esp32_eye_main.cpp
//
// Stage 2: adaptive background + self-tuning detection threshold.
// The background and the "what counts as empty" threshold both
// drift slowly toward the current scene, but ONLY during frames
// already classified as empty — so a train sitting in frame (moving
// or stopped) never gets absorbed into the background, while genuine
// lighting drift (dimmer switch, sun moving) gets tracked automatically.
//
// Exposure/gain are locked after boot-time convergence, and a
// watchdog force-recaptures the background if OBJECT persists far
// longer than any real train transit would — both guard against the
// same failure mode: a static scene that doesn't actually stay static
// frame-to-frame, which reads as a permanent false detection.
//
// Serial commands:
//   'c' — dump one raw frame (see capture_frame.py)
//   'b' — recapture the background reference from the current view

#include <Arduino.h>
#include <WiFi.h>
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

// Per-pixel byte-difference cutoff — how far a single pixel has to
// move to count as "changed" at all. Fixed for now; the adaptive
// part is the count-level threshold below.
#define PIXEL_DIFF_THRESHOLD 25

// Detection threshold (in changed-pixel count) is derived as
// noiseFloor * NOISE_MARGIN_FACTOR, clamped to this range.
#define MIN_DETECTION_THRESHOLD 30
#define MAX_DETECTION_THRESHOLD 8000
#define NOISE_MARGIN_FACTOR 4.0f

// How fast the background/noise estimates drift, per empty frame.
// Small values = slow drift (tracks lighting, ignores brief noise).
#define BACKGROUND_EMA_ALPHA 0.02f
#define NOISE_EMA_ALPHA      0.05f

// If the frame stays classified OBJECT for this long with no empty
// frame in between, it's not a real, brief train passage — it's a
// stuck false trigger. Rather than trusting a single frame as the
// new background (which can bake in a still-present object), we
// hold a candidate for VERIFY_FRAMES consecutive frames and only
// commit it if the scene stays stable that whole time.
#define STUCK_OBJECT_MS 5000
#define VERIFY_FRAMES 5

static bool cameraReady = false;
static uint8_t *background = nullptr;
static size_t backgroundLen = 0;

static float noiseFloor = 0;
static bool noiseFloorInit = false;
static float detectionThreshold = MIN_DETECTION_THRESHOLD;
static unsigned long lastEmptyMillis = 0;

static bool verifying = false;
static int verifyFramesLeft = 0;
static uint8_t *verifyBackground = nullptr;

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

  // Grayscale, low-res: fast to frame-diff, plenty for detection.
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

// Captures one frame and writes it raw to Serial, preceded by a
// text header line the PC-side script parses: "FRAME <w> <h> <len>".
static void dumpFrameOverSerial() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Frame capture failed");
    return;
  }
  Serial.printf("FRAME %u %u %u\n", fb->width, fb->height, fb->len);
  Serial.write(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

static bool captureBackground() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Background capture failed");
    return false;
  }
  if (background == nullptr || backgroundLen != fb->len) {
    free(background);
    background = (uint8_t *)malloc(fb->len);
    backgroundLen = fb->len;
  }
  memcpy(background, fb->buf, fb->len);
  esp_camera_fb_return(fb);

  // A fresh background means the old noise/threshold estimate no
  // longer applies — start that back at the conservative floor too.
  noiseFloorInit = false;
  detectionThreshold = MIN_DETECTION_THRESHOLD;

  Serial.println("Background captured");
  return background != nullptr;
}

// Counts pixels that differ from the background by more than
// PIXEL_DIFF_THRESHOLD. Simple and fast — no need for anything
// fancier at QQVGA resolution.
static size_t diffScore(const camera_fb_t *fb) {
  if (background == nullptr || backgroundLen != fb->len) {
    return 0;
  }
  size_t changed = 0;
  for (size_t i = 0; i < fb->len; i++) {
    int diff = (int)fb->buf[i] - (int)background[i];
    if (diff < 0) diff = -diff;
    if (diff > PIXEL_DIFF_THRESHOLD) changed++;
  }
  return changed;
}

// Blends the background toward the current frame. Only ever called
// on frames already judged empty, so an object in view — however
// long it sits there — never gets folded in.
static void driftBackgroundToward(const camera_fb_t *fb) {
  if (background == nullptr || backgroundLen != fb->len) return;
  for (size_t i = 0; i < fb->len; i++) {
    float b = background[i];
    b += BACKGROUND_EMA_ALPHA * ((float)fb->buf[i] - b);
    background[i] = (uint8_t)b;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("esp32_eye: bring-up starting");

  // Needed to register this device by MAC on the bridge side for
  // ESP-NOW. Doesn't join any network — just reads the chip's own
  // factory MAC.
  WiFi.mode(WIFI_STA);
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

  cameraReady = initCamera();
  Serial.println(cameraReady ? "Camera ready" : "Camera init FAILED");

  if (cameraReady) {
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
      // Let auto-exposure/auto-gain converge on the real scene first...
      delay(2000);
      // ...then freeze them. Otherwise every frame's brightness can
      // shift slightly on its own, which frame-diff reads as motion
      // across the whole image — exactly what happened in testing.
      s->set_exposure_ctrl(s, 0);
      s->set_gain_ctrl(s, 0);
    }
    captureBackground();
    lastEmptyMillis = millis();
  }
}

void loop() {
  if (!cameraReady) {
    delay(1000);
    return;
  }

  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 'c') {
      dumpFrameOverSerial();
      return;
    } else if (cmd == 'b') {
      captureBackground();
      lastEmptyMillis = millis();
      return;
    }
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Frame capture failed");
    delay(200);
    return;
  }

  // Mid-verification: check this frame against the candidate
  // background rather than the live one, and only commit once
  // VERIFY_FRAMES in a row all look stable against it.
  if (verifying) {
    size_t vdiff = 0;
    for (size_t i = 0; i < fb->len; i++) {
      int d = (int)fb->buf[i] - (int)verifyBackground[i];
      if (d < 0) d = -d;
      if (d > PIXEL_DIFF_THRESHOLD) vdiff++;
    }

    if ((float)vdiff >= MIN_DETECTION_THRESHOLD) {
      // Not actually stable (still something in frame) — abandon
      // this candidate and let the watchdog try again later.
      Serial.println("Re-baseline candidate unstable, will retry");
      verifying = false;
      lastEmptyMillis = millis();
    } else {
      verifyFramesLeft--;
      if (verifyFramesLeft <= 0) {
        memcpy(background, verifyBackground, backgroundLen);
        noiseFloorInit = false;
        detectionThreshold = MIN_DETECTION_THRESHOLD;
        lastEmptyMillis = millis();
        verifying = false;
        Serial.println("Re-baseline verified and committed");
      }
    }
    esp_camera_fb_return(fb);
    delay(100);
    return;
  }

  size_t changed = diffScore(fb);
  bool isEmpty = (float)changed < detectionThreshold;

  if (isEmpty) {
    lastEmptyMillis = millis();
    if (!noiseFloorInit) {
      noiseFloor = (float)changed;
      noiseFloorInit = true;
    } else {
      noiseFloor += NOISE_EMA_ALPHA * ((float)changed - noiseFloor);
    }
    detectionThreshold = constrain(noiseFloor * NOISE_MARGIN_FACTOR,
                                    (float)MIN_DETECTION_THRESHOLD,
                                    (float)MAX_DETECTION_THRESHOLD);
    driftBackgroundToward(fb);
  } else if (millis() - lastEmptyMillis > STUCK_OBJECT_MS) {
    // No real train passage takes this long — start a verification
    // window instead of trusting this one frame outright.
    Serial.println("Stuck in OBJECT too long — starting re-baseline verification");
    if (verifyBackground == nullptr || backgroundLen != fb->len) {
      free(verifyBackground);
      verifyBackground = (uint8_t *)malloc(fb->len);
    }
    memcpy(verifyBackground, fb->buf, fb->len);
    verifying = true;
    verifyFramesLeft = VERIFY_FRAMES;
    esp_camera_fb_return(fb);
    delay(100);
    return;
  }

  Serial.printf("diff:%u thresh:%.1f noise:%.2f %s t=%lu ms\n",
                (unsigned)changed, detectionThreshold, noiseFloor,
                isEmpty ? "empty " : "OBJECT", millis());

  esp_camera_fb_return(fb);
  delay(100);  // ~10 fps for this stage
}



