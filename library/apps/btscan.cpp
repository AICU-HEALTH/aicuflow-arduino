#include "btscan.h"

int lastBTScanTime = 0;
int btScanInterval = 5000; // scan every 5 seconds
BLEScan* pBLEScan;

void onBTPageOpen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextSize(1);
  tft.println("Scanning Bluetooth...");

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true); // active scan for more data
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
}

void onBTPageUpdate() {
  if (millis() - lastBTScanTime > btScanInterval) {
    BLEScanResults* foundDevices = pBLEScan->start(3, false); // scan for 3 seconds
    int n = foundDevices->getCount();
    
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);

    if (n == 0) {
      tft.println("No devices found.");
    } else {
      for (int i = 0; i < n && i < 10; i++) {
        BLEAdvertisedDevice device = foundDevices->getDevice(i);
        tft.print(i + 1);
        tft.print(F(": "));
        tft.print(device.getName().c_str()); // .c_str() schadet hier nicht
        tft.print(F(" ("));
        tft.print(device.getRSSI());
        tft.println(F(" dBm)"));
      }
    }

    pBLEScan->clearResults(); // free memory
    lastBTScanTime = millis();
  }
  closePageIfAnyButtonIsPressed();
}
