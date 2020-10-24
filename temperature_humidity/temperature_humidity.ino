//#define ENABLE_WIFI
#include "DHT.h"
#include <hd44780.h>
#include <hd44780ioClass/hd44780_pinIO.h>
#ifdef ENABLE_WIFI
  #include <SPI.h>
  #include <WiFiNINA.h>
  int status = WL_IDLE_STATUS;// the Wifi radio's status
#endif
#include "secrets.h" 

#define DHTPIN 7
#define DHTTYPE DHT11   // DHT 11
#define NONDHTSENORPIN A0

DHT dht(DHTPIN, DHTTYPE);
hd44780_pinIO lcd(12, 11, 5, 4, 3, 2);
bool hasCleared = false;
bool shouldLog = false;

unsigned long prevMillisCalcValues = 0;
unsigned long prevMillisWifi = 0;
long calcValuesInterval = 1000;
long wifiInterval = 1000;

String inputString = "";
bool stringComplete = false;
bool forceUpdate = false;

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

void setup() {
  Serial.begin(9600);

  while (!Serial) 
  {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  
  lcd.begin(16, 2);
  lcd.print("Initializing...");
  dht.begin();
  inputString.reserve(200);

  #ifdef WIFI
  if (WiFi.status() == WL_NO_MODULE) 
  {
    Serial.println("Communication with WiFi module failed!");
  }
  else 
  {
    lcd.setCursor(0,1);
    lcd.print("Wifi: init");
    wifiEnabled = true;
    String fv = WiFi.firmwareVersion();
    if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
      Serial.println("Please upgrade the firmware");
    }
  
    // attempt to connect to Wifi network:
    while (status != WL_CONNECTED) {
      Serial.print("Attempting to connect to WPA SSID: ");
      Serial.println(ssid);
      // Connect to WPA/WPA2 network:
      status = WiFi.begin(ssid, pass);
  
      // wait for connection:
      delay(5000);
    }
  
    // you're connected now, so print out the data:
    Serial.print("You're connected to the network");
    printCurrentNet();
    printWifiData();
  }
  #endif
  
}

void loop() {
  if (Serial.available() > 0) 
  {
    checkSerial();
  }
   
  checkCLI();
  
  unsigned long curMillis = millis();
  if((curMillis - prevMillisCalcValues >= calcValuesInterval) || forceUpdate)
  {
    forceUpdate = false;
    prevMillisCalcValues = curMillis;
    int sensorVal = analogRead(NONDHTSENORPIN);

    // convert the ADC reading to voltage
    float voltage = (sensorVal / 1024.0) * 5.0;
  
    // convert the voltage to temperature in degrees C
    // the sensor changes 10 mV per degree
    // the datasheet says there's a 500 mV offset
    // ((voltage - 500 mV) times 100)  
    float tempC = (voltage - .5) * 100;
    float tempF = (tempC * 9 / 5) + 32;
    
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    float humidity = dht.readHumidity();
    // Read temperature as Celsius
    float tempC2 = dht.readTemperature();
    // Read temperature as Fahrenheit
    float tempF2 = dht.readTemperature(true);
  
    logValues(sensorVal, voltage, tempC, tempF, humidity, tempC2, tempF2);
    float avgTemp = (tempF + tempF + tempF2)/3;
    updateLcd(avgTemp, humidity);  
  }
#ifdef ENABLE_WIFI
  if((curMillis - prevMillisWifi >= wifiInterval))
  {
    prevMillisWifi = curMillis;
    printCurrentNet();
  }
#endif
}

void checkSerial()
{
  while(Serial.available())
  {
    char inChar = (char)Serial.read();
    
    if(inChar == '\n')
    {
      stringComplete = true;
    }
    else
    {
      inputString += inChar;
    }
  }
}

void checkCLI()
{
  if(stringComplete) 
  {
    if(shouldLog)
    {
      Serial.println(inputString);
    }
    if(inputString.equalsIgnoreCase("debug"))
    {
      shouldLog = !shouldLog;
    }
    else if(inputString.equalsIgnoreCase("update"))
    {
      forceUpdate = true;
    }
    else if(inputString.startsWith("interval"))
    {
      String interval = inputString.substring(8);
      interval.trim();
      long intInterval = interval.toInt();
      if(intInterval > 0)
      {
        calcValuesInterval = intInterval;
      }
    }
    else if(inputString.startsWith("lcd-write"))
    {
      String outStr = inputString.substring(9);
      outStr.trim();
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(outStr);
    }
    else if(inputString.equalsIgnoreCase("lcd-clear"))
    {
      lcd.clear();
      lcd.setCursor(0,0);
    }
    #ifdef ENABLE_WIFI
    else if(inputString.equalsIgnoreCase("net"))
    {
      printCurrentNet();
    }
    #endif
    else
    {
      Serial.println("supported commands debug, update, interval, lcd-write, lcd-clear, net");
    }
    inputString = "";
    stringComplete = false;
  }
}

void logValues(int sensorVal, float voltage, float tempC, float tempF, float humidity, float tempC2, float tempF2)
{
  if(!shouldLog)
  {
    return;
  }
  Serial.print("sensor Value: ");
  Serial.print(sensorVal);
  Serial.print(", Volts: ");
  Serial.print(voltage);
  Serial.print(", degrees C: ");
  Serial.print(tempC);
  Serial.print(", degrees F: ");
  Serial.println(tempF);
  if (isnan(humidity) || isnan(tempC2) || isnan(tempF2)) 
  {
    Serial.println("Failed to read from DHT sensor!");
  }
  else
  {
    Serial.print("dht11: ");
    Serial.print(tempC2);
    Serial.print(" C / ");
    Serial.print(tempF2);
    Serial.print("F / ");
    Serial.print(humidity, 0);
    Serial.println("%");
  }
}

void updateLcd(float avgTemp, float humidity)
{
  if(!hasCleared)
  {
    hasCleared = true;
    lcd.clear();
  }

  lcd.setCursor(0, 0);
  lcd.print("Temp:");
  lcd.print(avgTemp, 1);
  lcd.print((char)223);
  lcd.print("F");
  lcd.setCursor(0, 1);
  lcd.print("Humidity:");
  lcd.print(humidity, 0);
  lcd.print("%");
}

#ifdef ENABLE_WIFI
void printWifiData() 
{
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  Serial.println(ip);

  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);
}

void printCurrentNet() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  byte bssid[6];
  WiFi.BSSID(bssid);
  Serial.print("BSSID: ");
  printMacAddress(bssid);

  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);

  byte encryption = WiFi.encryptionType();
  Serial.print("Encryption Type:");
  Serial.println(encryption, HEX);
  Serial.println();
}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}
#endif
