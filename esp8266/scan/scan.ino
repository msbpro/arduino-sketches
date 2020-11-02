#include "ESP8266WiFi.h"

#define BLINK_PERIOD 250
long lastBlinkMillis;
boolean ledState;

#define SCAN_PERIOD 5000
long lastScanMillis;


void setup()
 {
  Serial.begin(115200);
  Serial.println();

  pinMode(LED_BUILTIN, OUTPUT);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
}

void loop()
{
  long currentMillis = millis();

  // blink LED
  if (currentMillis - lastBlinkMillis > BLINK_PERIOD)
  {
    digitalWrite(LED_BUILTIN, ledState);
    ledState = !ledState;
    lastBlinkMillis = currentMillis;
  }

  // trigger Wi-Fi network scan
  if (currentMillis - lastScanMillis > SCAN_PERIOD)
  {
    WiFi.scanNetworks(true);
    Serial.print("\nScan start ... ");
    lastScanMillis = currentMillis;
  }

  // print out Wi-Fi network scan result upon completion
  int n = WiFi.scanComplete();
  if(n >= 0)
  {
    Serial.printf("%d network(s) found\n", n);
    for (int i = 0; i < n; i++)
    {
      Serial.printf("%d: %s, Ch:%d (%ddBm) %s\n", i+1, WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i), WiFi.encryptionType(i) == ENC_TYPE_NONE ? "open" : "");
    }
    WiFi.scanDelete();
  }
}
