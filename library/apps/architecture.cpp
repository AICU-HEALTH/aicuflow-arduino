#include "architecture.h"

// State variables
int lastHWDiagUpdate = 0;
int hwDiagUpdateInterval = 500; // update every 500ms
bool hwDiagInitialized = false;
bool hwDiagFirstDraw = true;

// Hardware detection results
struct HWDiagInfo {
  // CPU info
  uint32_t cpuFreqMHz;
  uint32_t xtalFreqMHz;
  uint8_t cpuCores;
  String chipModel;
  uint8_t chipRevision;
  uint32_t flashSize;
  uint32_t psramSize;
  
  // Memory
  uint32_t heapSize;
  uint32_t freeHeap;
  uint32_t psramFree;
  
  // Connectivity
  String macBT;
  String macWiFi;
  bool bleAvailable;
  bool wifiAvailable;
  
  // Power
  bool usbConnected;
  float batteryVoltage;
  bool onBattery;
  
  // Peripherals
  bool hasDisplay;
  int displayW;
  int displayH;
  String uartBridge;
  
  // Pins
  int leftButton;
  int rightButton;
  int resetButton;
  bool activePins[40]; // track which pins show activity
};

HWDiagInfo hwInfo;
HWDiagInfo hwInfoPrev; // Previous values for comparison

// Color scheme - minimal white/gray Apple style
#define COL_BG 0x0000
#define COL_LINE 0xFFFF
#define COL_TEXT 0xCE79
#define COL_ACCENT 0x4A49
#define COL_ACTIVE 0x07E0

void detectHardware() {
  // CPU & Chip info
  hwInfo.cpuFreqMHz = ESP.getCpuFreqMHz();
  hwInfo.chipModel = ESP.getChipModel();
  hwInfo.chipRevision = ESP.getChipRevision();
  hwInfo.flashSize = ESP.getFlashChipSize();
  
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S2)
  hwInfo.cpuCores = 2;
  hwInfo.xtalFreqMHz = 40;
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  hwInfo.cpuCores = 1;
  hwInfo.xtalFreqMHz = 40;
#else
  hwInfo.cpuCores = 2;
  hwInfo.xtalFreqMHz = 40;
#endif

  // PSRAM detection
#ifdef BOARD_HAS_PSRAM
  hwInfo.psramSize = ESP.getPsramSize();
  hwInfo.psramFree = ESP.getFreePsram();
#else
  hwInfo.psramSize = 0;
  hwInfo.psramFree = 0;
#endif

  // Memory
  hwInfo.heapSize = ESP.getHeapSize();
  hwInfo.freeHeap = ESP.getFreeHeap();
  
  // MAC addresses - use WiFi class methods
  uint8_t mac[6];
  WiFi.macAddress(mac);
  hwInfo.macWiFi = "";
  for(int i=0; i<6; i++) {
    if(i>0) hwInfo.macWiFi += ":";
    if(mac[i]<16) hwInfo.macWiFi += "0";
    hwInfo.macWiFi += String(mac[i], HEX);
  }
  
  // BT MAC (derive from WiFi MAC)
  hwInfo.macBT = "";
  for(int i=0; i<6; i++) {
    if(i>0) hwInfo.macBT += ":";
    uint8_t btByte = (i == 5) ? mac[i] + 2 : mac[i];
    if(btByte<16) hwInfo.macBT += "0";
    hwInfo.macBT += String(btByte, HEX);
  }
  
  // Connectivity - use DeviceProps correctly
  hwInfo.bleAvailable = device->has_bt;
  hwInfo.wifiAvailable = device->has_wifi;
  
  // Display
  hwInfo.hasDisplay = device->has_display;
  hwInfo.displayW = screenWidth;
  hwInfo.displayH = screenHeight;
  
  // UART Bridge detection
#if defined(ARDUINO_LILYGO_T_DISPLAY_S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
  hwInfo.uartBridge = "USB-OTG";
#else
  hwInfo.uartBridge = "CP2102/CH340";
#endif

  // Power detection
#ifdef CONFIG_IDF_TARGET_ESP32S3
  #ifdef ARDUINO_USB_MODE
    hwInfo.usbConnected = true;
  #else
    hwInfo.usbConnected = false;
  #endif
#else
  hwInfo.usbConnected = true; // assume USB if running
#endif
  
  // Button pins from device config
  hwInfo.leftButton = LEFT_BUTTON;
  hwInfo.rightButton = RIGHT_BUTTON;
  
  // Reset button (common pins)
#if defined(ARDUINO_TTGO_T1)
  hwInfo.resetButton = 35;
#elif defined(ARDUINO_LILYGO_T_DISPLAY_S3)
  hwInfo.resetButton = 14;
#else
  hwInfo.resetButton = 0;
#endif

  // NO PIN SCANNING - just clear the array
  memset(hwInfo.activePins, 0, sizeof(hwInfo.activePins));
}

void drawThinRect(int x, int y, int w, int h, uint16_t col) {
  tft.drawRect(x, y, w, h, col);
}

void drawThinLine(int x1, int y1, int x2, int y2, uint16_t col) {
  tft.drawLine(x1, y1, x2, y2, col);
}

void drawCompactText(int x, int y, const char* text, uint16_t col, uint8_t size=1) {
  tft.setTextSize(size);
  tft.setTextColor(col, COL_BG);
  tft.setCursor(x, y);
  tft.print(text);
}

void clearTextArea(int x, int y, int w, int h) {
  tft.fillRect(x, y, w, h, COL_BG);
}

void drawStaticElements(bool isSmall, int offsetY) {
  int fontSize = 1;
  
  // === DISPLAY SECTION ===
  if(hwInfo.hasDisplay) {
    int dispX = screenWidth/2 - 25;
    int dispY = offsetY + 2;
    int dispW = 50;
    int dispH = 18;
    
    drawThinRect(dispX, dispY, dispW, dispH, COL_LINE);
    
    // Display model info
    char dispInfo[32];
    if(isSmall) {
      tft.print(hwInfo.displayW);
      tft.print('x');
      tft.print(hwInfo.displayH);
    } else {
      tft.print(F("TFT "));
      tft.print(hwInfo.displayW);
      tft.print('x');
      tft.print(hwInfo.displayH);
    }
    drawCompactText(dispX+2, dispY+2, dispInfo, COL_TEXT, fontSize);
    
    // Display type
    const char* dispType = isSmall ? "ST7735" : "ST7789/ILI9341";
    drawCompactText(dispX+2, dispY+10, dispType, COL_ACCENT, fontSize);
    
    // Connection line to ESP32
    drawThinLine(dispX + dispW/2, dispY + dispH, dispX + dispW/2, dispY + dispH + 8, COL_LINE);
  }
  
  // === MAIN ESP32 CHIP ===
  int espX = screenWidth/2 - 40;
  int espY = hwInfo.hasDisplay ? (offsetY + 30) : (offsetY + 10);
  int espW = 80;
  int espH = isSmall ? 55 : 65;
  
  drawThinRect(espX, espY, espW, espH, COL_LINE);
  
  // Chip model (static)
  char chipName[32];
  String chipStr = hwInfo.chipModel + " r" + String(hwInfo.chipRevision);
  chipStr.toCharArray(chipName, sizeof(chipName));
  drawCompactText(espX+2, espY+2, chipName, COL_TEXT, fontSize);
  
  // CPU Cores (static)
  int coreW = (espW - 6) / hwInfo.cpuCores;
  for(uint8_t i=0; i<hwInfo.cpuCores; i++) {
    int coreX = espX + 2 + i*(coreW+2);
    int coreY = espY + 12;
    drawThinRect(coreX, coreY, coreW, 12, COL_ACCENT);
    
    char coreLabel[8];
    String coreLabelStr = "C" + String(i);
    coreLabelStr.toCharArray(coreLabel, sizeof(coreLabel));
    drawCompactText(coreX+2, coreY+3, coreLabel, COL_TEXT, fontSize);
  }
  
  // === CONNECTIVITY CHIPS ===
  int connY = espY + espH + 10;
  
  // WiFi chip
  if(hwInfo.wifiAvailable) {
    int wifiX = espX - 5;
    int wifiW = 38;
    int wifiH = 18;
    
    drawThinRect(wifiX, connY, wifiW, wifiH, COL_LINE);
    drawCompactText(wifiX+2, connY+2, "WiFi", COL_TEXT, fontSize);
    drawCompactText(wifiX+2, connY+10, isSmall ? "2.4G" : "2.4GHz", COL_ACCENT, fontSize);
    
    // Connection line
    drawThinLine(espX, espY+espH-5, wifiX+wifiW, connY+wifiH/2, COL_LINE);
  }
  
  // BT chip
  if(hwInfo.bleAvailable) {
    int btX = espX + espW - 33;
    int btW = 38;
    int btH = 18;
    
    drawThinRect(btX, connY, btW, btH, COL_LINE);
    drawCompactText(btX+2, connY+2, "BLE", COL_TEXT, fontSize);
    drawCompactText(btX+2, connY+10, isSmall ? "5.0" : "BT5.0", COL_ACCENT, fontSize);
    
    // Connection line
    drawThinLine(espX+espW, espY+espH-5, btX, connY+btH/2, COL_LINE);
  }
  
  // === UART BRIDGE / USB ===
  int uartY = connY + 25;
  int uartX = screenWidth/2 - 30;
  int uartW = 60;
  int uartH = 16;
  
  drawThinRect(uartX, uartY, uartW, uartH, COL_LINE);
  drawCompactText(uartX+2, uartY+2, hwInfo.uartBridge.c_str(), COL_TEXT, fontSize);
  
  // Connection to ESP
  drawThinLine(espX+espW/2, espY+espH, uartX+uartW/2, uartY, COL_LINE);
  
  // === LEFT SIDE PINOUT ===
  int pinStartY = espY;
  int pinSpacing = isSmall ? 7 : 8;
  int maxLeftPins = isSmall ? 7 : 8;
  
  // Common left-side pins for ESP32
  int leftPins[] = {36, 39, 34, 35, 32, 33, 25, 26};
  for(int i=0; i<maxLeftPins && i<8; i++) {
    int pin = leftPins[i];
    int py = pinStartY + i*pinSpacing;
    
    // Pin line
    uint16_t pinCol = (pin < 40 && hwInfo.activePins[pin]) ? COL_ACTIVE : COL_LINE;
    drawThinLine(0, py, espX-2, py, pinCol);
    
    // Pin label
    char pinLabel[8];
    String(pin).toCharArray(pinLabel, sizeof(pinLabel));
    drawCompactText(2, py-3, pinLabel, pinCol, fontSize);
    
    // Special annotations
    if(pin == hwInfo.leftButton) {
      drawCompactText(espX-18, py-3, isSmall ? "B" : "BTN", COL_ACTIVE, fontSize);
    }
  }
  
  // === RIGHT SIDE PINOUT ===
  int rightPins[] = {4, 5, 18, 19, 21, 22, 23, 27};
  for(int i=0; i<maxLeftPins && i<8; i++) {
    int pin = rightPins[i];
    int py = pinStartY + i*pinSpacing;
    
    uint16_t pinCol = (pin < 40 && hwInfo.activePins[pin]) ? COL_ACTIVE : COL_LINE;
    drawThinLine(espX+espW+2, py, screenWidth-1, py, pinCol);
    
    char pinLabel[8];
    String(pin).toCharArray(pinLabel, sizeof(pinLabel));
    drawCompactText(screenWidth-14, py-3, pinLabel, pinCol, fontSize);
    
    if(pin == hwInfo.rightButton) {
      drawCompactText(espX+espW+4, py-3, isSmall ? "B" : "BTN", COL_ACTIVE, fontSize);
    }
  }
  
  // === BUTTON INDICATORS ===
  // Left button arrow
  if(hwInfo.leftButton > 0 && !isSmall) {
    drawCompactText(2, screenHeight-10, en_de("<-BTN", "<-TAST"), COL_ACCENT, fontSize);
  }
  
  // Right button arrow
  if(hwInfo.rightButton > 0 && !isSmall) {
    drawCompactText(screenWidth-35, screenHeight-10, en_de("BTN->", "TAST->"), COL_ACCENT, fontSize);
  }
  
  // Reset button indicator (top)
  if(hwInfo.resetButton > 0 && !isSmall) {
    drawCompactText(screenWidth-28, offsetY + 2, en_de("RST^", "RST^"), COL_ACCENT, fontSize);
  }
}

void updateDynamicElements(bool isSmall, int offsetY) {
  int fontSize = 1;
  int espX = screenWidth/2 - 40;
  int espY = hwInfo.hasDisplay ? (offsetY + 30) : (offsetY + 10);
  int espW = 80;
  int espH = isSmall ? 55 : 65;
  
  // Update frequency if changed
  if(hwInfo.cpuFreqMHz != hwInfoPrev.cpuFreqMHz) {
    clearTextArea(espX+2, espY+27, 50, 8);
    char freqInfo[24];
    String freqStr = String(hwInfo.cpuFreqMHz) + "MHz";
    freqStr.toCharArray(freqInfo, sizeof(freqInfo));
    drawCompactText(espX+2, espY+27, freqInfo, COL_ACCENT, fontSize);
  }
  
  // Update memory if changed
  if(hwInfo.freeHeap != hwInfoPrev.freeHeap || hwInfo.heapSize != hwInfoPrev.heapSize) {
    clearTextArea(espX+2, espY+36, espW-4, 8);
    char memInfo[32];
    String memStr;
    if(isSmall) {
      memStr = String(hwInfo.freeHeap/1024) + "K/" + String(hwInfo.heapSize/1024) + "K";
    } else {
      memStr = "RAM:" + String(hwInfo.freeHeap/1024) + "K/" + String(hwInfo.heapSize/1024) + "K";
    }
    memStr.toCharArray(memInfo, sizeof(memInfo));
    drawCompactText(espX+2, espY+36, memInfo, COL_TEXT, fontSize);
  }
  
  // Update PSRAM if available and changed
  if(hwInfo.psramSize > 0 && (hwInfo.psramFree != hwInfoPrev.psramFree || hwDiagFirstDraw)) {
    clearTextArea(espX+2, espY+45, 40, 8);
    char psramInfo[24];
    String psramStr = "PS:" + String(hwInfo.psramSize/(1024*1024)) + "M";
    psramStr.toCharArray(psramInfo, sizeof(psramInfo));
    drawCompactText(espX+2, espY+45, psramInfo, COL_ACCENT, fontSize);
  }
  
  // Flash (static after first draw)
  if(!isSmall && hwDiagFirstDraw) {
    char flashInfo[24];
    String flashStr = "FL:" + String(hwInfo.flashSize/(1024*1024)) + "M";
    flashStr.toCharArray(flashInfo, sizeof(flashInfo));
    drawCompactText(espX+42, espY+45, flashInfo, COL_ACCENT, fontSize);
  }
  
  // Update USB status if changed
  if(hwInfo.usbConnected != hwInfoPrev.usbConnected || hwDiagFirstDraw) {
    int connY = espY + espH + 10;
    int uartY = connY + 25;
    int uartX = screenWidth/2 - 30;
    
    clearTextArea(uartX+2, uartY+9, 56, 7);
    const char* usbStat = hwInfo.usbConnected ? 
      (isSmall ? "USB" : "USB-PWR") : 
      (isSmall ? "BAT" : "BATTERY");
    uint16_t pwrCol = hwInfo.usbConnected ? COL_ACTIVE : COL_ACCENT;
    drawCompactText(uartX+2, uartY+9, usbStat, pwrCol, fontSize);
  }
  
  // Update bottom status line
  if(!isSmall && (hwInfo.usbConnected != hwInfoPrev.usbConnected || hwDiagFirstDraw)) {
    clearTextArea(2, screenHeight-20, screenWidth-4, 8);
    char statusLine[64];
    String statusStr = String(device->kind_short) + " | " + (hwInfo.usbConnected ? "USB" : "BATT");
    statusStr.toCharArray(statusLine, sizeof(statusLine));
    drawCompactText(2, screenHeight-20, statusLine, COL_TEXT, fontSize);
  }
}

void onHWDiagPageOpen() {
  tft.fillScreen(COL_BG);
  tft.setTextColor(COL_TEXT, COL_BG);
  
  // Initialize hardware detection
  detectHardware();
  hwDiagInitialized = true;
  hwDiagFirstDraw = true;
  lastHWDiagUpdate = millis();
  
  // Copy current to previous
  memcpy(&hwInfoPrev, &hwInfo, sizeof(HWDiagInfo));
}

void onHWDiagPageUpdate() {
  if(!hwDiagInitialized) return;
  
  // Refresh detection periodically
  if(millis() - lastHWDiagUpdate > hwDiagUpdateInterval) {
    detectHardware();
    
    bool isSmall = (screenWidth <= 135);
    
    // Calculate vertical centering offset
    int totalHeight = hwInfo.hasDisplay ? 135 : 115;
    int offsetY = (screenHeight - totalHeight) / 2;
    if(offsetY < 0) offsetY = 0;
    
    // First draw: render everything
    if(hwDiagFirstDraw) {
      drawStaticElements(isSmall, offsetY);
      updateDynamicElements(isSmall, offsetY);
      hwDiagFirstDraw = false;
    } else {
      // Subsequent draws: only update what changed
      updateDynamicElements(isSmall, offsetY);
    }
    
    // Copy current to previous for next comparison
    memcpy(&hwInfoPrev, &hwInfo, sizeof(HWDiagInfo));
    
    lastHWDiagUpdate = millis();
  }
  
  closePageIfAnyButtonIsPressed();
}