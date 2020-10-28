#include "UnoWiFi.h"
#ifdef __AVR__
 #include <avr/pgmspace.h>
#else
 #define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif

long _wifiInterval = 3000;
unsigned long _prevMillisWifi = 0;
bool _debug = false;

#ifdef __ENABLE_WIFI__
int _status = WL_IDLE_STATUS;// the Wifi radio's status
#endif

#include "secrets.h" 

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

UnoWiFi::UnoWiFi()
{
  UnoWiFi(false);
}

UnoWiFi::UnoWiFi(bool debug)
{
  _debug = debug;
}

void UnoWiFi::setup()
{
  #ifdef __ENABLE_WIFI__
  if (WiFi.status() == WL_NO_MODULE) 
  {
    Serial.println("Communication with WiFi module failed!");
  }
  else 
  {
    String fv = WiFi.firmwareVersion();
    if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
      Serial.println("Please upgrade the firmware");
    }
  
    // attempt to connect to Wifi network:
    while (_status != WL_CONNECTED) {
      if(_debug)
      {
        Serial.print("Attempting to connect to WPA SSID: ");
        Serial.println(ssid);
      }
      
      // Connect to WPA/WPA2 network:
      _status = WiFi.begin(ssid, pass);
  
      // wait for connection:
      delay(5000);
    }

    if(_debug)
    {
      // you're connected now, so print out the data:
      Serial.print("You're connected to the network");
      printCurrentNet();
      printWifiData();
    }
  }
  #endif
}

void UnoWiFi::loop(void)
{
  if(!_debug)
  {
    return;
  }
  unsigned long curMillis = millis();
  
  if((curMillis - _prevMillisWifi >= _wifiInterval))
  {
    _prevMillisWifi = curMillis;
    #ifdef __ENABLE_WIFI__
    printCurrentNet();
    #endif
  }
}

void UnoWiFi::updateDebug(bool debug)
{
  _debug = debug;
}

void UnoWiFi::printWifiData() 
{
  #ifdef __ENABLE_WIFI__FI
  if(!_debug)
  {
    return;
  }
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);
  #endif
}

void UnoWiFi::printCurrentNet() {
  #ifdef __ENABLE_WIFI__
  if(!_debug)
  {
    return;
  }
  Serial.print("SSID: ");
  Serial.print(WiFi.SSID());

  byte bssid[6];
  WiFi.BSSID(bssid);
  Serial.print(" / BSSID: ");
  printMacAddress(bssid);

  long rssi = WiFi.RSSI();
  Serial.print(" / sig:");
  Serial.print(rssi);

  byte encryption = WiFi.encryptionType();
  Serial.print(" / encryption:");
  Serial.println(encryption, HEX);
  #endif
}

void UnoWiFi::printMacAddress(byte mac[]) {
  #ifdef __ENABLE_WIFI__
  if(!_debug)
  {
    return;
  }
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  #endif
}
