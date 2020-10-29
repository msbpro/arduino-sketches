#include "UnoWiFi.h"
#ifdef __AVR__
 #include <avr/pgmspace.h>
#else
 #define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif

long _wifiInterval = 3000;
unsigned long _prevMillisWifi = 0;
bool _debug = false;
char _serverOutput[1000];

#ifdef __ENABLE_WIFI__
int _status = WL_IDLE_STATUS;// the Wifi radio's status
WiFiServer server(80);
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
    Serial.println(F("Communication with WiFi module failed!"));
  }
  else 
  {
    String fv = WiFi.firmwareVersion();
    if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
      Serial.println(F("Please upgrade the firmware"));
    }
  
    // attempt to connect to Wifi network:
    while (_status != WL_CONNECTED) {
      if(_debug)
      {
        Serial.print(F("Attempting to connect to WPA SSID: "));
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
      Serial.println(F("You're connected to the network"));
      printCurrentNet();
      printWifiData();
      Serial.println(F("Starting webserver"));
      // start the web server on port 80
      server.begin();
    }
  }
  #endif
}

void UnoWiFi::loop(void)
{
  checkClient();
  
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
  #ifdef __ENABLE_WIFI__
  if(!_debug)
  {
    return;
  }
  IPAddress ip = WiFi.localIP();
  Serial.print(F("IP Address: "));
  Serial.println(ip);

  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print(F("MAC address: "));
  printMacAddress(mac);
  #endif
}

void UnoWiFi::printCurrentNet() {
  #ifdef __ENABLE_WIFI__
  if(!_debug)
  {
    return;
  }
  Serial.print(F("SSID: "));
  Serial.print(WiFi.SSID());

  byte bssid[6];
  WiFi.BSSID(bssid);
  Serial.print(F(" / BSSID: "));
  printMacAddress(bssid);

  long rssi = WiFi.RSSI();
  Serial.print(F(" / sig:"));
  Serial.print(rssi);

  byte encryption = WiFi.encryptionType();
  Serial.print(F(" / encryption:"));
  Serial.println(encryption, HEX);
  #endif
}

void UnoWiFi::setServerOutput(char* output)
{
  strncpy(_serverOutput, output, sizeof(_serverOutput));
}

void UnoWiFi::printMacAddress(byte mac[]) {
  #ifdef __ENABLE_WIFI__
  if(!_debug)
  {
    return;
  }
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print(F("0"));
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(F(":"));
    }
  }
  #endif
}

void UnoWiFi::checkClient()
{
  #ifdef __ENABLE_WIFI__
  WiFiClient client = server.available();   // listen for incoming clients
  if (client) {                             // if you get a client,
    Serial.println("new client");           // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        if (c == '\n') {                    // if the byte is a newline character
  
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println(F("HTTP/1.1 200 OK"));
            client.println(F("Content-type:text/html"));
            client.println();

            /*
            // the content of the HTTP response follows the header:
            client.print(F("Click <a href=\"/H\">here</a> turn the LED on<br>"));
            client.print(F("Click <a href=\"/L\">here</a> turn the LED off<br>"));
            */
            if ((_serverOutput == NULL) || (_serverOutput[0] == '\0'))
            {
               client.print(F("No output specified"));
            }
            else
            {
              Serial.println(_serverOutput);
              client.print(_serverOutput);
            }
            
            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          }
          else {      // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        }
        else if (c != '\r') {    // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }

        /*
        // Check to see if the client request was "GET /H" or "GET /L":
        if (currentLine.endsWith(F("GET /H"))) {
          digitalWrite(LED_BUILTIN, HIGH);               // GET /H turns the LED on
        }
        if (currentLine.endsWith(F("GET /L"))) {
          digitalWrite(LED_BUILTIN, LOW);                // GET /L turns the LED off
        }
        */
      }
    }
    // close the connection:
    client.stop();
    Serial.println(F("client disconnected"));
  }
  #endif
}
