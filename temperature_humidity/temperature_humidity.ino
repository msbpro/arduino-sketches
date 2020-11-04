// for uno wifi we need the old library
#include "DHT_old.h"
//#include "DHT.h"
#include <hd44780.h>
#include <hd44780ioClass/hd44780_pinIO.h>

//#include "UnoWiFi.h"
//UnoWiFi wifi(true);
#include "secrets.h"
#define USE_AIRLIFT     // required for Arduino Uno WiFi R2 board compatability
#include "AdafruitIO_WiFi.h"
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, SECRET_SSID, SECRET_PASS, SPIWIFI_SS, SPIWIFI_ACK, SPIWIFI_RESET, NINA_GPIO0, &SPI);

// set up the 'temperature' feed
AdafruitIO_Feed *tempFeed = io.feed("room.temperature");
// set up the 'humidity' feed
AdafruitIO_Feed *humidityFeed = io.feed("room.humidity");
// set up the 'wifi-strength' feed
AdafruitIO_Feed *wifiFeed = io.feed("room.wifi-strength");

#define DHTPIN 7
#define DHTTYPE DHT11   // DHT 11
#define NONDHTSENORPIN A0

DHT dht(DHTPIN, DHTTYPE);
hd44780_pinIO lcd(12, 11, 5, 4, 3, 2);
bool hasCleared = false;
bool shouldLog = false;

unsigned long prevMillisCalcValues = 0;
long calcValuesInterval = 1000;

long wifiSendInterval = 30000;
unsigned long prevMillisWifiSend = wifiSendInterval * 2;

String inputString = "";
bool stringComplete = false;
bool forceUpdate = true;

void setup() {
  Serial.begin(9600);
  while (!Serial) 
  {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print(F("Initializing..."));
  dht.begin();
  inputString.reserve(200);

  // connect to io.adafruit.com
  Serial.print(F("Connecting to Adafruit IO"));
  io.connect();
 
  // wait for a connection
  while (io.status() < AIO_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
 
  // we are connected
  Serial.println();
  Serial.println(io.statusText());

  //wifi.setup();
  
}

void loop() {
  io.run();
  
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
    long wifiStrength = WiFi.RSSI();
    
    logValues(sensorVal, voltage, tempC, tempF, humidity, tempC2, tempF2, wifiStrength);
    float avgTemp = (tempF + tempF + tempF2)/3;
    updateLcd(avgTemp, humidity);

    if(curMillis - prevMillisWifiSend >= wifiSendInterval)
    {
      prevMillisWifiSend = curMillis;
      if(shouldLog)
      {
        Serial.println(F("sending data over wifi"));
      }
      tempFeed->save(avgTemp, 0, 0, 0, 2);
      humidityFeed->save(humidity, 0, 0, 0, 2);
      wifiFeed->save(wifiStrength, 0, 0, 0);
    }
    
    /*
    char avgTempStr[10];
    dtostrf(avgTemp, 5, 1, avgTempStr);
    
    char humidityStr[10];
    dtostrf(humidity, 5, 1, humidityStr);

    char temp[200];
    snprintf_P(temp, sizeof(temp), PSTR("<html><head><title>Temp + Humidity</title></head><body><div>Temperature: %s <span>&#176;</span>F<br>Humidity: %s%%</div></body></html>"), avgTempStr, humidityStr);
    wifi.setServerOutput(temp);
    */
  }

  //wifi.loop();
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
    if(inputString.equalsIgnoreCase(F("debug")))
    {
      shouldLog = !shouldLog;
      //wifi.updateDebug(shouldLog);
    }
    else if(inputString.equalsIgnoreCase(F("update")))
    {
      forceUpdate = true;
    }
    else if(inputString.startsWith(F("interval")))
    {
      String interval = inputString.substring(8);
      interval.trim();
      long intInterval = interval.toInt();
      if(intInterval > 0)
      {
        calcValuesInterval = intInterval;
      }
    }
    else if(inputString.startsWith(F("lcd-write")))
    {
      String outStr = inputString.substring(9);
      outStr.trim();
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(outStr);
    }
    else if(inputString.equalsIgnoreCase(F("lcd-clear")))
    {
      lcd.clear();
      lcd.setCursor(0,0);
    }
    /*
    #ifdef ENABLE_WIFI
    else if(inputString.equalsIgnoreCase(F("net")))
    {
      wifi.printCurrentNet();
      wifi.printWifiData();
    }
    #endif
    */
    else
    {
      Serial.println(F("supported commands debug, update, interval, lcd-write, lcd-clear, net"));
    }
    inputString = "";
    stringComplete = false;
  }
}

void logValues(int sensorVal, float voltage, float tempC, float tempF, float humidity, float tempC2, float tempF2, long wifiStrength)
{
  if(!shouldLog)
  {
    return;
  }
  Serial.print(F("sensor Value: "));
  Serial.print(sensorVal);
  Serial.print(F(", Volts: "));
  Serial.print(voltage);
  Serial.print(F(", degrees C: "));
  Serial.print(tempC);
  Serial.print(F(", degrees F: "));
  Serial.println(tempF);
  if (isnan(humidity) || isnan(tempC2) || isnan(tempF2)) 
  {
    Serial.println("Failed to read from DHT sensor!");
  }
  else
  {
    Serial.print(F("dht11: "));
    Serial.print(tempC2);
    Serial.print(F(" C / "));
    Serial.print(tempF2);
    Serial.print(F("F / "));
    Serial.print(humidity, 0);
    Serial.println(F("%"));
  }
  Serial.print(F("Wifi strength"));
  Serial.println(wifiStrength);
}

void updateLcd(float avgTemp, float humidity)
{
  if(!hasCleared)
  {
    hasCleared = true;
    lcd.clear();
  }

  lcd.setCursor(0, 0);
  lcd.print(F("Temp:"));
  lcd.print(avgTemp, 1);
  lcd.print((char)223);
  lcd.print(F("F"));
  lcd.setCursor(0, 1);
  lcd.print(F("Humidity:"));
  lcd.print(humidity, 0);
  lcd.print(F("%"));
}
