#include "esp_camera.h"
#include "img_converters.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "secrets.h"   // WIFI_SSID, WIFI_PASSWORD, NTFY_TOPIC (no leading "/")

// XIAO ESP32-S3 Sense camera pins
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39
#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

// Motion settings
#define MOTION_THRESHOLD       12.0
#define PIXEL_DIFF_THRESHOLD   35
#define REQUIRED_MOTION_FRAMES 2
#define COOLDOWN_MS            8000   // wait after a SUCCESSFUL alert
#define RETRY_MS               4000   // wait after a FAILED alert

// Reliability settings
#define MAX_FAILS         5                              // restart after this many failed uploads in a row
#define RESTART_EVERY_MS  (12UL * 60UL * 60UL * 1000UL)  // scheduled restart every 12 hours
#define WIFI_CHECK_MS     30000                          // check Wi-Fi every 30 s

#define SAMPLE_COLS 16
#define SAMPLE_ROWS 12
#define SAMPLE_COUNT (SAMPLE_COLS * SAMPLE_ROWS)

// Public DNS servers (hotspot DNS is often unreliable)
static const IPAddress DNS_PRIMARY(8, 8, 8, 8);
static const IPAddress DNS_SECONDARY(1, 1, 1, 1);

uint8_t previousSamples[SAMPLE_COUNT];
bool baselineReady = false;

uint8_t *rgbBuffer = nullptr;
size_t rgbBufferSize = 0;

int consecutiveMotionFrames = 0;
unsigned long lastAttemptTime = 0;
bool hasAttempted = false;
bool lastAttemptOk = false;

int failCount = 0;
unsigned long lastWifiCheck = 0;

// ------------------------------------------------
// CAMERA
// ------------------------------------------------
void setupCamera() {
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
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;

  config.fb_count = psramFound() ? 2 : 1;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM
                                    : CAMERA_FB_IN_DRAM;

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {
    Serial.printf("Camera initialization failed: 0x%x\n", err);
    Serial.println("Restarting in 5 s...");
    delay(5000);
    ESP.restart();
  }

  sensor_t *sensor = esp_camera_sensor_get();

  if (sensor != nullptr) {
    if (sensor->id.PID == OV3660_PID) {
      sensor->set_vflip(sensor, 1);
      sensor->set_brightness(sensor, 1);
      sensor->set_saturation(sensor, -2);
    }

    sensor->set_whitebal(sensor, 1);
    sensor->set_awb_gain(sensor, 1);
  }

  Serial.println("Camera initialized.");
}

// ------------------------------------------------
// WI-FI + DNS FIX
// ------------------------------------------------
void forcePublicDNS() {
  IPAddress ip      = WiFi.localIP();
  IPAddress gateway = WiFi.gatewayIP();
  IPAddress subnet  = WiFi.subnetMask();

  // Keep the DHCP-given IP, replace DNS with Google + Cloudflare
  WiFi.config(ip, gateway, subnet, DNS_PRIMARY, DNS_SECONDARY);
  delay(500);

  Serial.print("DNS 1: ");
  Serial.println(WiFi.dnsIP(0));
  Serial.print("DNS 2: ");
  Serial.println(WiFi.dnsIP(1));
}

bool waitForWiFi(unsigned long timeoutMs) {
  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < timeoutMs) {
    delay(250);
    Serial.print(".");
  }

  return WiFi.status() == WL_CONNECTED;
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.printf("Connecting to Wi-Fi: %s", WIFI_SSID);

  if (!waitForWiFi(30000)) {
    Serial.println("\nWi-Fi timeout. Restarting...");
    delay(200);
    ESP.restart();
  }

  Serial.println("\nWi-Fi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("Signal strength: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");

  forcePublicDNS();
}

bool ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.println("Wi-Fi disconnected; reconnecting...");

  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  if (!waitForWiFi(15000)) {
    Serial.println("\nWi-Fi reconnection failed.");
    return false;
  }

  Serial.println("\nReconnected.");
  forcePublicDNS();   // WiFi.begin() resets DNS, so set it again
  return true;
}

// ------------------------------------------------
// INTERNET + DNS + HTTPS DIAGNOSTIC (runs once at startup)
// ------------------------------------------------
void testNtfyConnection() {
  Serial.println("\n===== NTFY DIAGNOSTIC =====");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ERROR: Wi-Fi is disconnected.");
    return;
  }

  WiFiClient raw;
  Serial.println("Step 1: raw internet test (8.8.8.8:53)...");

  if (raw.connect(IPAddress(8, 8, 8, 8), 53, 5000)) {
    Serial.println("  -> Internet OK (raw IP reachable).");
    raw.stop();
  } else {
    Serial.println("  -> NO INTERNET through this Wi-Fi network.");
    Serial.println("  Check hotspot/router internet, or use a phone hotspot (2.4 GHz).");
    Serial.println("===== DIAGNOSTIC COMPLETE =====\n");
    return;
  }

  IPAddress serverIP;
  Serial.println("Step 2: resolving ntfy.sh...");

  if (WiFi.hostByName("ntfy.sh", serverIP) != 1) {
    Serial.println("  -> DNS FAILED even with public DNS.");
    Serial.println("===== DIAGNOSTIC COMPLETE =====\n");
    return;
  }

  Serial.print("  -> Resolved IP: ");
  Serial.println(serverIP);

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15);   // seconds

  Serial.println("Step 3: HTTPS connect to ntfy.sh:443...");

  if (client.connect("ntfy.sh", 443)) {
    Serial.println("  -> HTTPS CONNECTION SUCCESSFUL.");
    client.stop();
  } else {
    Serial.println("  -> HTTPS CONNECTION FAILED.");
  }

  Serial.println("===== DIAGNOSTIC COMPLETE =====\n");
}

// ------------------------------------------------
// MOTION DETECTION
// ------------------------------------------------
bool checkMotion(camera_fb_t *fb) {
  if (fb == nullptr || fb->format != PIXFORMAT_JPEG) {
    return false;
  }

  size_t requiredSize = (size_t)fb->width * fb->height * 3;

  if (rgbBuffer == nullptr || rgbBufferSize < requiredSize) {
    if (rgbBuffer != nullptr) {
      free(rgbBuffer);
      rgbBuffer = nullptr;
    }

    rgbBuffer = (uint8_t *)ps_malloc(requiredSize);
    if (rgbBuffer == nullptr) {
      rgbBuffer = (uint8_t *)malloc(requiredSize);   // fallback
    }

    if (rgbBuffer == nullptr) {
      Serial.println("ERROR: RGB buffer allocation failed.");
      return false;
    }

    rgbBufferSize = requiredSize;
    baselineReady = false;
  }

  if (!fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgbBuffer)) {
    Serial.println("JPEG-to-RGB conversion failed.");
    return false;
  }

  int sampleIndex = 0;
  int changedPixels = 0;

  for (int row = 0; row < SAMPLE_ROWS; row++) {
    for (int col = 0; col < SAMPLE_COLS; col++) {
      int x = ((2 * col + 1) * fb->width) / (2 * SAMPLE_COLS);
      int y = ((2 * row + 1) * fb->height) / (2 * SAMPLE_ROWS);

      size_t offset = ((size_t)y * fb->width + x) * 3;

      uint8_t r = rgbBuffer[offset];
      uint8_t g = rgbBuffer[offset + 1];
      uint8_t b = rgbBuffer[offset + 2];

      uint8_t gray = (uint8_t)((77 * r + 150 * g + 29 * b) >> 8);

      if (!baselineReady) {
        previousSamples[sampleIndex] = gray;
      } else {
        int difference = abs((int)gray - (int)previousSamples[sampleIndex]);

        if (difference >= PIXEL_DIFF_THRESHOLD) {
          changedPixels++;
        }

        previousSamples[sampleIndex] =
            (uint8_t)(((int)previousSamples[sampleIndex] * 3 + gray) / 4);
      }

      sampleIndex++;
    }
  }

  if (!baselineReady) {
    baselineReady = true;
    Serial.println("Motion baseline established.");
    return false;
  }

  float motionPercent = changedPixels * 100.0f / SAMPLE_COUNT;

  Serial.printf("Changed sample pixels: %d/%d (%.1f%%)\n",
                changedPixels, SAMPLE_COUNT, motionPercent);

  return motionPercent >= MOTION_THRESHOLD;
}

// ------------------------------------------------
// NTFY IMAGE UPLOAD
// ------------------------------------------------
bool sendAlert(camera_fb_t *fb) {
  if (fb == nullptr || fb->format != PIXFORMAT_JPEG) {
    Serial.println("ERROR: Invalid JPEG frame.");
    return false;
  }

  if (!ensureWiFi()) return false;

  // DNS check; if it fails, re-apply public DNS and try once more
  IPAddress serverIP;
  if (WiFi.hostByName("ntfy.sh", serverIP) != 1) {
    Serial.println("DNS failed; re-applying public DNS...");
    forcePublicDNS();

    if (WiFi.hostByName("ntfy.sh", serverIP) != 1) {
      Serial.println("DNS still failing. Skipping this attempt.");
      return false;
    }
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15);   // seconds

  Serial.println("Connecting to ntfy.sh:443...");

  // Hostname keeps the correct TLS server name (SNI)
  if (!client.connect("ntfy.sh", 443)) {
    Serial.println("HTTPS connection failed.");
    Serial.println("No HTTP request was sent.");
    return false;
  }

  char headers[512];

  int headerLength = snprintf(
      headers, sizeof(headers),
      "PUT /%s HTTP/1.1\r\n"
      "Host: ntfy.sh\r\n"
      "Title: Wildlife Motion Detected\r\n"
      "Priority: high\r\n"
      "Tags: warning,camera\r\n"
      "Filename: motion.jpg\r\n"
      "Content-Type: image/jpeg\r\n"
      "Content-Length: %u\r\n"
      "Connection: close\r\n\r\n",
      NTFY_TOPIC, (unsigned int)fb->len);

  if (headerLength <= 0 || headerLength >= (int)sizeof(headers)) {
    Serial.println("HTTP header construction failed.");
    client.stop();
    return false;
  }

  size_t headerSent = client.write((const uint8_t *)headers,
                                   (size_t)headerLength);

  if (headerSent != (size_t)headerLength) {
    Serial.println("Failed to send HTTP headers.");
    client.stop();
    return false;
  }

  size_t sent = 0;

  while (sent < fb->len) {
    size_t written = client.write(fb->buf + sent, fb->len - sent);

    if (written == 0) {
      Serial.println("Image upload interrupted.");
      client.stop();
      return false;
    }

    sent += written;
  }

  Serial.printf("Uploaded %u image bytes. Waiting for HTTP response...\n",
                (unsigned int)sent);

  unsigned long start = millis();

  while (!client.available() && client.connected() &&
         millis() - start < 15000) {
    delay(20);
  }

  if (!client.available()) {
    Serial.println("No HTTP response received.");
    client.stop();
    return false;
  }

  String statusLine = client.readStringUntil('\n');
  statusLine.trim();

  Serial.print("HTTP status: ");
  Serial.println(statusLine);

  bool success = statusLine.startsWith("HTTP/1.1 2") ||
                 statusLine.startsWith("HTTP/1.0 2");

  if (success) {
    Serial.println("NTFY ACCEPTED THE REQUEST.");
  } else {
    Serial.println("NTFY RETURNED AN HTTP ERROR.");

    unsigned long responseStart = millis();

    while (client.available() && millis() - responseStart < 2000) {
      String line = client.readStringUntil('\n');
      line.trim();

      if (line.length() > 0) {
        Serial.println(line);
      }
    }
  }

  client.stop();
  return success;
}

// ------------------------------------------------
// SETUP
// ------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println("\n=== XIAO ESP32-S3 Wildlife Camera ===");
  Serial.printf("PSRAM: %s\n", psramFound() ? "found" : "NOT found");

  setupCamera();
  connectWiFi();            // also applies public DNS
  testNtfyConnection();     // internet -> DNS -> HTTPS check

  lastWifiCheck = millis();
  Serial.println("Motion monitoring started.");
}

// ------------------------------------------------
// LOOP
// ------------------------------------------------
void loop() {
  // Periodic Wi-Fi watchdog
  if (millis() - lastWifiCheck > WIFI_CHECK_MS) {
    lastWifiCheck = millis();
    ensureWiFi();
  }

  // Scheduled clean restart (keeps heap healthy for long runs)
  if (millis() > RESTART_EVERY_MS && consecutiveMotionFrames == 0) {
    Serial.println("Scheduled restart...");
    delay(200);
    ESP.restart();
  }

  camera_fb_t *fb = esp_camera_fb_get();

  if (fb == nullptr) {
    Serial.println("Camera frame capture failed.");
    delay(200);
    return;
  }

  bool motionDetected = checkMotion(fb);

  if (motionDetected) {
    consecutiveMotionFrames++;
  } else {
    consecutiveMotionFrames = 0;
  }

  if (consecutiveMotionFrames >= REQUIRED_MOTION_FRAMES) {
    unsigned long now = millis();
    unsigned long waitMs = lastAttemptOk ? COOLDOWN_MS : RETRY_MS;

    bool ready = !hasAttempted || (now - lastAttemptTime >= waitMs);

    if (ready) {
      Serial.println("CONFIRMED MOTION: uploading photo...");

      bool success = sendAlert(fb);

      hasAttempted = true;
      lastAttemptOk = success;
      lastAttemptTime = millis();   // cooldown/retry delay starts now

      if (success) {
        failCount = 0;
      } else {
        failCount++;
        Serial.printf("Alert failed (%d/%d).\n", failCount, MAX_FAILS);

        if (failCount >= MAX_FAILS) {
          Serial.println("Too many failures. Restarting...");
          esp_camera_fb_return(fb);
          delay(200);
          ESP.restart();
        }
      }
    }

    consecutiveMotionFrames = 0;
  }

  esp_camera_fb_return(fb);
  delay(150);
}
