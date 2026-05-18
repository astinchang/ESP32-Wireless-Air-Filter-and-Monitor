#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include "Adafruit_PM25AQI.h"
#include <Adafruit_HDC302x.h>
#include <SPI.h>
#include <SD.h>

// PIN DEFINITIONS
// for I2C temp/humid sensor
#define SDA_PIN 23
#define SCL_PIN 22
// for UART PMS sensor
#define RX_PIN 4  
#define TX_PIN 5 
// for fan PWM
#define FAN_PWM_PIN 6
// for manual override button
#define BTN_PIN 0
// SD card SPI 
#define SD_CS_PIN 18
#define SD_MOSI_PIN 19
#define SD_MISO_PIN 20
#define SD_SCK_PIN 21

// PMS5003 uses 9600 baud
#define PMS_BAUD 9600

// PWM settings for fans
#define PWM_FREQ 25000      // 25 kHz common for 4-wire PWM PC fans
#define PWM_RESOLUTION 8    // duty cycle range: 0-255

// WiFi settings:
const char* ssid = "ESP32-AirFilter";
const char* password = "airfilter123";
WiFiServer server(80);
String header;

IPAddress apIP;
unsigned long lastWiFiPrintTime = 0;
const unsigned long WIFI_PRINT_INTERVAL_MS = 10000;  // print every 10 sec

// define sensors
HardwareSerial pmsSerial(1); // UART1 on ESP32
Adafruit_PM25AQI aqi = Adafruit_PM25AQI();
Adafruit_HDC302x hdc = Adafruit_HDC302x();

// timer variables for serial monitor prints
unsigned long lastPrintTime = 0;
const unsigned long PRINT_INTERVAL_MS = 5000;

// timer variables for button debounce / ISR
volatile bool buttonPressed = false;
volatile unsigned long lastInterruptTime = 0;

bool fanEnabled = true;

bool sdReady = false;
void logDataToSD();
// true  = PM2.5 controls PWM
// false = fan forced off

// hold sensor values
uint16_t latestPM1 = 0;
uint16_t latestPM25 = 0;
uint16_t latestPM10 = 0;

uint16_t latestPM25Env = 0;
uint16_t latestPM10Env = 0;
uint16_t latestAQI = 0;

double latestTempC = 0.0;
double latestHumidity = 0.0;

int latestFanDuty = 0;

// button ISR
void IRAM_ATTR buttonISR() {
  unsigned long currentTime = millis();

  // Simple debounce
  if (currentTime - lastInterruptTime > 250) {
    buttonPressed = true;
    lastInterruptTime = currentTime;
  }
}

// helper functions to setup sensors
void setupPMS5003() {
  Serial.println("Starting PMS5003 UART...");

  pmsSerial.begin(PMS_BAUD, SERIAL_8N1, RX_PIN, TX_PIN);

  // PMS5003 needs boot time before first read
  Serial.println("Waiting 30 seconds for PMS5003 fan/laser to stabilize...");
  delay(30000);

  if (!aqi.begin_UART(&pmsSerial)) {
    Serial.println("ERROR: Could not find PMS5003 / PM2.5 sensor.");
    Serial.println("Check: VCC=5V, GND, PMS TX -> ESP32 RX, baud=9600.");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("PMS5003 found.");
}

void setupHDC3022() {
  Serial.println("Starting HDC3022 I2C...");

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!hdc.begin(0x44, &Wire)) {
    Serial.println("ERROR: Could not find HDC3022.");
    Serial.println("Check: VIN=3.3V, GND, SDA, SCL, I2C address 0x44.");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("HDC3022 found.");
}

void setupFanPWM() {
  Serial.println("Starting PWM fan output...");

  ledcAttach(FAN_PWM_PIN, PWM_FREQ, PWM_RESOLUTION);

  ledcWrite(FAN_PWM_PIN, 0);

  Serial.println("PWM fan output ready.");
}

void setupButtonInterrupt() {
  Serial.println("Starting button interrupt input...");

  pinMode(BTN_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BTN_PIN), buttonISR, FALLING);

  Serial.println("Button interrupt ready.");
}

void setupWiFiServer() {
  Serial.println();
  Serial.println("Setting up ESP32 Wi-Fi Access Point...");

  WiFi.softAP(ssid, password);

  apIP = WiFi.softAPIP();

  Serial.print("Connect to Wi-Fi network: ");
  Serial.println(ssid);

  Serial.print("Password: ");
  Serial.println(password);

  Serial.print("Then open this IP address in a browser: ");
  Serial.println(apIP);

  server.begin();

  Serial.println("Web server started.");
}

void setupSDCard() {
  Serial.println("Starting SD card...");

  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  if (!SD.begin(SD_CS_PIN, SPI)) {
    Serial.println("ERROR: SD card failed or not present.");
    Serial.println("check wiring!");
    sdReady = false;
    return;
  }

  sdReady = true;

  Serial.println("SD card ready.");

  if (!SD.exists("/airlog.csv")) {
    File file = SD.open("/airlog.csv", FILE_WRITE);

    if (file) {
      file.println("time_ms,pm1,pm25,pm10,pm25_env,pm10_env,aqi,temp_c,humidity,fan_enabled,pwm_duty");
      file.close();
      Serial.println("Created airlog.csv with header.");
    } else {
      Serial.println("ERROR: Could not create airlog.csv.");
    }
  } else {
    Serial.println("airlog.csv already exists.");
  }
}

// FAN CONTROL
int getPMDrivenFanDuty(uint16_t pm25) {
  /*
    PM2.5 drives fan PWM.

    Cleaner air  -> lower PWM
    Dirtier air  -> higher PWM
  */

  if (pm25 <= 5) {
    return 0;
  } else if (pm25 <= 12) {
    return map(pm25, 5, 12, 60, 100);
  } else if (pm25 <= 35) {
    return map(pm25, 12, 35, 100, 200);
  } else if (pm25 <= 50) {
    return map(pm25, 35, 50, 200, 255);
  } else {
    return 255;
  }
}

void updateFanPWM() {
  int duty = 0;

  if (fanEnabled) {
    duty = getPMDrivenFanDuty(latestPM25);
  } else {
    duty = 0;
  }

  latestFanDuty = duty;
  ledcWrite(FAN_PWM_PIN, duty);

  Serial.print("[FAN] Enabled: ");
  Serial.print(fanEnabled ? "YES" : "NO");

  Serial.print(" | PM2.5: ");
  Serial.print(latestPM25);
  Serial.print(" ug/m3");

  Serial.print(" | PWM Duty: ");
  Serial.print(duty);
  Serial.println(" / 255");
}

void handleButtonPress() {
  if (buttonPressed) {
    buttonPressed = false;

    fanEnabled = !fanEnabled;

    Serial.println();
    Serial.println("Button interrupt detected!");
    Serial.print("Fan enabled set to: ");
    Serial.println(fanEnabled ? "YES" : "NO");

    updateFanPWM();
  }
}

// read and print helper functions
void readAndPrintSensors() {
  PM25_AQI_Data pmData;

  Serial.println();
  Serial.println("AIR FILTER SENSOR READINGS");
  Serial.println("======================================");

  // Read PMS5003
  if (aqi.read(&pmData)) {
    latestPM1 = pmData.pm10_standard;
    latestPM25 = pmData.pm25_standard;
    latestPM10 = pmData.pm100_standard;

    latestPM25Env = pmData.pm25_env;
    latestPM10Env = pmData.pm100_env;
    latestAQI = pmData.aqi_pm25_us;

    Serial.println("[PMS5003: Particulate Matter]");

    Serial.print("PM1.0 standard: ");
    Serial.print(latestPM1);
    Serial.println(" ug/m3");

    Serial.print("PM2.5 standard: ");
    Serial.print(latestPM25);
    Serial.println(" ug/m3");

    Serial.print("PM10 standard: ");
    Serial.print(latestPM10);
    Serial.println(" ug/m3");

    Serial.print("PM2.5 environmental: ");
    Serial.print(latestPM25Env);
    Serial.println(" ug/m3");

    Serial.print("PM10 environmental: ");
    Serial.print(latestPM10Env);
    Serial.println(" ug/m3");

    Serial.print("PM2.5 AQI US: ");
    Serial.println(latestAQI);
  } else {
    Serial.println("[PMS5003] Could not read PM data this cycle.");
  }

  Serial.println();

  // Read HDC3022
  Serial.println("[HDC3022: Temperature / Humidity]");

  bool hdcOK = hdc.readTemperatureHumidityOnDemand(
    latestTempC,
    latestHumidity,
    TRIGGERMODE_LP0
  );

  if (hdcOK) {
    Serial.print("Temperature: ");
    Serial.print(latestTempC);
    Serial.println(" C");

    Serial.print("Humidity: ");
    Serial.print(latestHumidity);
    Serial.println(" %");
  } else {
    Serial.println("Could not read HDC3022 data this cycle.");
  }

  updateFanPWM();
  logDataToSD();

  Serial.println("================================================");
}

void printWiFiInfoPeriodically() {
  if (millis() - lastWiFiPrintTime >= WIFI_PRINT_INTERVAL_MS) {
    lastWiFiPrintTime = millis();

    Serial.println();
    Serial.println("========== WIFI DASHBOARD INFO ==========");
    Serial.print("Connect to Wi-Fi network: ");
    Serial.println(ssid);
    Serial.print("Password: ");
    Serial.println(password);
    Serial.print("Open browser to: http://");
    Serial.println(apIP);
    Serial.println("=========================================");
  }
}

void logDataToSD() {
  if (!sdReady) {
    Serial.println("[SD] SD card not ready. Skipping log.");
    return;
  }

  File file = SD.open("/airlog.csv", FILE_APPEND);

  if (!file) {
    Serial.println("ERROR: Could not open airlog.csv for appending.");
    return;
  }

  file.print(millis());
  file.print(",");
  file.print(latestPM1);
  file.print(",");
  file.print(latestPM25);
  file.print(",");
  file.print(latestPM10);
  file.print(",");
  file.print(latestPM25Env);
  file.print(",");
  file.print(latestPM10Env);
  file.print(",");
  file.print(latestAQI);
  file.print(",");
  file.print(latestTempC, 2);
  file.print(",");
  file.print(latestHumidity, 2);
  file.print(",");
  file.print(fanEnabled ? 1 : 0);
  file.print(",");
  file.println(latestFanDuty);

  file.close();

  Serial.println("[SD] Logged data to airlog.csv");
}

// WiFi page
void handleWebClient() {
  WiFiClient client = server.available();

  if (client) {
    Serial.println("New web client connected.");

    String currentLine = "";
    header = "";

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        header += c;

        if (c == '\n') {
          if (currentLine.length() == 0) {
            // HTTP response header
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // HTML page
            client.println("<!DOCTYPE html><html>");
            client.println("<head>");
            client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<meta http-equiv=\"refresh\" content=\"5\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");

            client.println("<style>");
            client.println("html { font-family: Helvetica; text-align: center; background-color: #f4f4f4; }");
            client.println("body { margin: 0; padding: 20px; }");
            client.println(".card { background-color: white; padding: 20px; margin: 15px auto; border-radius: 12px; max-width: 500px; box-shadow: 0 2px 8px rgba(0,0,0,0.15); }");
            client.println("h1 { color: #222; }");
            client.println("h2 { color: #333; }");
            client.println("p { font-size: 20px; }");
            client.println(".value { font-weight: bold; }");
            client.println("</style>");
            client.println("</head>");

            client.println("<body>");
            client.println("<h1>ESP32 Air Filter Dashboard</h1>");

            client.println("<div class=\"card\">");
            client.println("<h2>What These Values Mean</h2>");
            client.println("<p><strong>PM2.5:</strong> Fine particles in the air that are 2.5 micrometers or smaller. Higher PM2.5 usually means dirtier air.</p>");
            client.println("<p><strong>PM2.5 Environmental:</strong> The sensor-adjusted PM2.5 reading intended to better represent real surrounding air conditions.</p>");
            client.println("<p><strong>AQI:</strong> Air Quality Index. This converts particle pollution into a simpler health-based scale, where lower values mean cleaner air and higher values mean worse air quality.</p>");
            client.println("</div>");

            client.println("<div class=\"card\">");
            client.println("<h2>Particulate Matter</h2>");
            client.println("<p>PM1.0: <span class=\"value\">" + String(latestPM1) + "</span> ug/m3</p>");
            client.println("<p>PM2.5: <span class=\"value\">" + String(latestPM25) + "</span> ug/m3</p>");
            client.println("<p>PM10: <span class=\"value\">" + String(latestPM10) + "</span> ug/m3</p>");
            client.println("<p>PM2.5 Environmental: <span class=\"value\">" + String(latestPM25Env) + "</span> ug/m3</p>");
            client.println("<p>PM10 Environmental: <span class=\"value\">" + String(latestPM10Env) + "</span> ug/m3</p>");
            client.println("<p>PM2.5 AQI US: <span class=\"value\">" + String(latestAQI) + "</span></p>");
            client.println("</div>");

            client.println("<div class=\"card\">");
            client.println("<h2>Temperature / Humidity</h2>");
            client.println("<p>Temperature: <span class=\"value\">" + String(latestTempC, 2) + "</span> C</p>");
            client.println("<p>Humidity: <span class=\"value\">" + String(latestHumidity, 2) + "</span> %</p>");
            client.println("</div>");

            client.println("<div class=\"card\">");
            client.println("<h2>Fan Status</h2>");
            client.println("<p>Fan Enabled: <span class=\"value\">" + String(fanEnabled ? "YES" : "NO") + "</span></p>");
            client.println("<p>PWM Duty: <span class=\"value\">" + String(latestFanDuty) + "</span> / 255</p>");
            client.println("</div>");

            client.println("<p>Page auto-refreshes every 5 seconds.</p>");
            client.println("</body></html>");

            client.println();

            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }

    header = "";
    client.stop();

    Serial.println("Web client disconnected.");
    Serial.println();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("ESP32 Air Filter Sensor + Interrupt PWM + WiFi Dashboard");

  setupHDC3022();
  setupPMS5003();
  setupFanPWM();
  setupButtonInterrupt();
  setupWiFiServer();
  setupSDCard();

  Serial.println("Setup complete. Reading sensors, controlling fans, making webpage...");
}

void loop() {
  handleButtonPress();
  handleWebClient();
  printWiFiInfoPeriodically();

  if (millis() - lastPrintTime >= PRINT_INTERVAL_MS) {
    lastPrintTime = millis();
    readAndPrintSensors();
  }
}
