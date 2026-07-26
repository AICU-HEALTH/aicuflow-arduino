/**
 *  Stream Arduino data to aicuflow AI cloud easily.
 *
 *  Docs:  https://aicuflow.com/docs/library/arduino
 *  Code:  https://github.com/AICU-HEALTH/aicuflow-arduino
 *  From:  20260121-26, by Finn Glas @ AICU GmbH
 *  Board: ESP32 recommended, others work too
 *  Start:
 *        1. Log in and all sensor data is sent to your flow.
 *        2. Device, wifi chip and display is autodetected.
 *        3. Explore cool menus and tools on esp32 devices,
 *           if you use a low-end device, it just measures.
 *  Edit:
 *        1. Find `registerSensors` and add new sensors!
 *        2. Use VS-Code for better code editing!
 *        3. Search `add more` to find customisations!
 */

// === Compiler Config ===
#define USE_BLUETOOTH false  // BT-LE is ca 1mb large, thus optional
#define VERBOSE false        // Debugging recommended for developers
#define BYTES_PER_POINT 240  // Reserved memory (increase if more sensors)
#define ALL_PINS true       // (experimental) Register all pins as inputs

// === Imports ===
#include "library/device/DeviceProps.cpp"        // device detection
#if IS_ESP
#include <WiFiClientSecure.h>                    // WPA2/3? alt (insecure): #include <WiFiClient.h>
#include "imports/TFT_eSPI/TFT_eSPI.cpp"         // LIB NEED TO INSTALL 
#include "imports/ArduinoJson/ArduinoJson.h"     // LIB NEED TO INSTALL 
#include "library/device/Settings.cpp"           // persistent settings
#include "library/aicuflow/AicuClient.cpp"       // --> client library you can use <--
#include "library/graphics/TFTKeyboard.cpp"      // enter strings using buttons (cool)
#include "library/graphics/PageManager.cpp"      // multi-page app on screens
#include "library/graphics/closeFunctions.cpp"   // for app close timing & recognition
#include "library/apps/core.cpp"                 // import all apps indirectly
#endif
#include "library/sensors/SensorMeasurement.cpp" // sensor registry for measuring, plots and saving
#include "library/device/all_other_pins.cpp"     // (experimental) for the ALL_PINS feature

// === Setup ===
// TODO CHANGE THESE FACTORY SETTINGS
// or do so later, manually on device
// needed to work
const char* WLAN_SSID = ""; // connect to a stable WPA2 Wifi
const char* WLAN_PASS = "";
const char* AICU_USER = ""; // register at https://aicuflow.com/signup
const char* AICU_PASS = "";
const char* PROJ_FLOW = ""; // create / select at https://aicuflow.com/flows

// optional options
const char* PROJ_FILE = "esp";     // will be auto created with .arrow extension
const char* API_URL = "https://prod-backend.aicuflow.com"; // dev or prod
const char* DEVICE_ID_SUFFIX = ""; // 0,1,2,3 appended to id if you have multiple of same kind
const int POINTS_BATCH_SIZE = 64;  // 64 always works, 256 sometimes did, but may be too large.
const int MEASURE_DELAY_MS = 100;  // millisec interval 100ms = 10 per second
const int SCREEN_IDLE_MS = 60000;  // also needs TFT_BL eg 38
const int WIFI_TIMEOUT = 10000;    // 10s, 0 -> blocking till wifi

// === Globals ===
// Do not change these, app needs em
#if IS_ESP
WiFiClientSecure client;             // secure wifi conn
AicuClient aicu(API_URL);            // aicu api client
TFT_eSPI tft = TFT_eSPI();           // screen stuff
int screenWidth, screenHeight;       // are auto detected
#endif
bool wifiAvailable = false;          // true once wifi connected
SensorMeasurement sensors("");       // sensor registry (->graphs,measure)
int LEFT_BUTTON, RIGHT_BUTTON;       // are auto detected
int VOLTAGE_PIN;                     // is auto detected
const DeviceProps* device = nullptr; // auto device info here

// === Sensors ===
// Measurement functions (add more yourself!)
uint8_t measure_left() { return !digitalRead(LEFT_BUTTON); }
uint8_t measure_right() { return !digitalRead(RIGHT_BUTTON); }
float measure_cpu_voltage() {
  return (analogRead(VOLTAGE_PIN)/4095.0)*2.0*3.3*1.1; // 2=ResDiv;3.3RefV;4095-12-bitADCres
}
float measure_loops_per_ms () { // old (cpu freq): return (double)getCpuFrequencyMhz();
  const uint32_t start = micros();
  const uint32_t duration = 10000; // 10 ms in µs
  uint32_t cycles = 0;
  while ((micros() - start) < duration) cycles++;
  return (float) cycles / 10; // avg cycles in 1 ms
}
#if IS_ESP
float measure_wifi() { return (float) WiFi.RSSI(); }
int measure_freeheap() { return ESP.getFreeHeap(); }
#else
#endif

// Register sensors for data collection,
// and sending to the aicuflow cloud.
// the json keys are short on purpose to reduce load.
void registerSensors() {
  // (if wifi) signal strength = analog sensor
  #if IS_ESP // ESP boards only:
    // Left button = binary sensor
    sensors.registerSensor("left", measure_left, TFT_PURPLE, true, true);
    // Right button = binary sensor
    sensors.registerSensor("right", measure_right, TFT_BLUE, true, true);
    if (device->has_wifi && wifiAvailable)
      sensors.registerSensor("rssi", measure_wifi, TFT_GREEN, true, true);
    // Voltage = analog sensor
    sensors.registerSensor("volt", measure_cpu_voltage, TFT_RED, true, true);
  #endif
  
  // Temperature = analog sensor
  sensors.registerSensor("temp", temperatureRead, TFT_ORANGE, true, true);
  //#if CONFIG_IDF_TARGET_ESP32
    // Hall-Sensor (only on classic esp32)
    // sensors.registerSensor("hall", hallRead, TFT_MAGENTA, true, true);
  //#endif
  #if ALL_PINS // (experimental) sense all analog and digital pins
    registerAllOtherPins();
  #endif

  // Operational speed (~cpu frequency) = discrete number "sensor" (not graphed)
  sensors.registerSensor("speed", measure_loops_per_ms, TFT_YELLOW, true, false);  
  #if IS_ESP // ESP boards only:
    // Free Memory = discrete number "sensor"
    sensors.registerSensor("free", measure_freeheap, TFT_CYAN, true, true);
  #endif

  // Want to add more sensors?
  //   Parameters: (key, readFunc, color, enabled, showGraph)
  
  //   Ex1) Custom analog sensor
  //      sensors.registerSensor("light", []() { return analogRead(35) / 4095.0 * 100; }, TFT_WHITE, true, true);
  
  //   Ex2) Not graphed, deactivated sensor
  //      sensors.registerSensor("uptime", []() { return millis() / 1000.0; }, TFT_MAGENTA, false, false);
}

// === Program Sequence ===
// Only change this if you're sure
void setup() {
  device = &getDeviceProps();
  #if IS_ESP      // complex app start uses modules
    trackForAutoReset(); // 3x reboot before start = reset
    initPoints();
    initDeviceGPIOPins();
    initSerial();
    loadSettings();
    applySettings();
    
    if (device->has_display){
      initTFTScreen();
      bootScreen(1000);
      setupMenus();
    } else delay(1100);

    setupPages();
    clearAutoReset();
  #else /* AVR */ // manual startup
    Serial.begin(115200);
    sensors.deviceId = device->kind_short;
    registerSensors();
  #endif
}

void loop() {
  #if IS_ESP      // render apps like measurement
    pageManager->update();

  #else /* AVR */ // primitive measurement to serial
    // time precisely
    static uint32_t next_us = 0;
    static const uint32_t period_us = MEASURE_DELAY_MS * 1000UL;
    uint32_t now = micros();
    if (next_us == 0) next_us = now + period_us;
    if ((int32_t)(now - next_us) < 0) return;
    next_us += period_us;

    // print measurement json to serial
    StaticJsonDocument<512> doc;
    JsonObject obj = doc.to<JsonObject>();
    sensors.measure();
    sensors.toJson(obj);
    serializeJson(obj, Serial);
    Serial.println();
  #endif
}
