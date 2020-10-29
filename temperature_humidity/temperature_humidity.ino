#include "DHT.h"
#include <hd44780.h>
#include <hd44780ioClass/hd44780_pinIO.h>

#include "UnoWiFi.h"
UnoWiFi wifi(true);

#define DHTPIN 7
#define DHTTYPE DHT11   // DHT 11
#define NONDHTSENORPIN A0

DHT dht(DHTPIN, DHTTYPE);
hd44780_pinIO lcd(12, 11, 5, 4, 3, 2);
bool hasCleared = false;
bool shouldLog = false;

unsigned long prevMillisCalcValues = 0;
long calcValuesInterval = 1000;

String inputString = "";
bool stringComplete = false;
bool forceUpdate = false;

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

  wifi.setup();
  
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
    char avgTempStr[10];
    dtostrf(avgTemp, 5, 1, avgTempStr);
    
    char humidityStr[10];
    dtostrf(humidity, 5, 1, humidityStr);
    
    char temp[200];
    snprintf_P(temp, sizeof(temp), PSTR("<html><head><title>Temp + Humidity</title></head><body><div>Temperature: %s <span>&#176;</span>F<br>Humidity: %s%%</div></body></html>"), avgTempStr, humidityStr);
    wifi.setServerOutput(temp);
  }

  wifi.loop();
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
      wifi.updateDebug(shouldLog);
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
    #ifdef ENABLE_WIFI
    else if(inputString.equalsIgnoreCase(F("net")))
    {
      wifi.printCurrentNet();
      wifi.printWifiData();
    }
    #endif
    else
    {
      Serial.println(F("supported commands debug, update, interval, lcd-write, lcd-clear, net"));
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
