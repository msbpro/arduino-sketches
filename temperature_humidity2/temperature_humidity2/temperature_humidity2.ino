#define __INCLUDE__SCREEN__
#define __ENABLE_DEEP_SLEEP__

#include "secrets.h"
#include "AdafruitIO_WiFi.h"
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// set up the 'temperature' feed
AdafruitIO_Feed *tempFeed = io.feed("building.temperature");
// set up the 'humidity' feed
AdafruitIO_Feed *humidityFeed = io.feed("building.humidity");
// set up the 'wifi-strength' feed
AdafruitIO_Feed *wifiFeed = io.feed("building.wifi-strength");

#include "DHT.h"
#include "DHT_U.h"
#include <Adafruit_Sensor.h>

#define DHTPIN 14
#define DHTTYPE DHT22
DHT_Unified dht(DHTPIN, DHTTYPE);

#ifdef __INCLUDE__SCREEN__

  #include <SPI.h>
  #include <Wire.h>
  #include <Adafruit_SSD1306.h>
  
  #define SCREEN_WIDTH 128 // OLED display width, in pixels
  #define SCREEN_HEIGHT 64 // OLED display height, in pixels
  //SPI setup below
  #define OLED_MOSI   27 //data
  #define OLED_CLK   33 //clk
  #define OLED_DC    15 //dc sa0
  #define OLED_CS    21 //cs
  #define OLED_RESET 32 //rst
  Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);
  
#endif 

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  30        /* Time ESP32 will go to sleep (in seconds) */

bool shouldLog = true;
bool forceUpdate = true;

unsigned long prevMillisCalcValues = 0;
long calcValuesInterval = 30000;

String inputString = "";
bool stringComplete = false;

//RTC_DATA_ATTR int bootCount = 0;

void setup() {
  Serial.begin(115200);

  dht.begin();
  
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

#ifdef __INCLUDE__SCREEN__
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println(F("done loading"));
  display.display();
#endif

#ifdef __ENABLE_DEEP_SLEEP__
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);

  sleepLoop();
  Serial.println("Going to deep-sleep now");
  Serial.flush(); 

  esp_deep_sleep_start();
#endif
}

void commonLoop()
{
  // io.run(); is required for all sketches.
  // it should always be present at the top of your loop
  // function. it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();

  if (Serial.available() > 0) 
  {
    checkSerial();
  }
  checkCLI();
}

void loop() 
{
  commonLoop();
  
  bool localForceUpdate = forceUpdate;
  unsigned long curMillis = millis();
  if((curMillis - prevMillisCalcValues >= calcValuesInterval) || localForceUpdate)
  {
    forceUpdate = false;
    prevMillisCalcValues = curMillis;
    checkTemp();
  }
}

void sleepLoop()
{
  commonLoop();
  checkTemp();
}

void checkTemp()
{
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  sensors_event_t event;  
  dht.humidity().getEvent(&event);
  float humidity = event.relative_humidity;
  dht.temperature().getEvent(&event);
  // Read temperature as Celsius
  float tempC = event.temperature;
  // Read temperature as Fahrenheit
  float tempF = (tempC * 9 / 5) + 32;
  long wifiStrength = WiFi.RSSI();
  
  logValues(tempC, tempF, humidity, wifiStrength);

  updateLcd(tempF, humidity, wifiStrength);

  tempFeed->save(tempF, 0, 0, 0, 2);
  humidityFeed->save(humidity, 0, 0, 0, 2);
  wifiFeed->save(wifiStrength, 0, 0, 0);
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
      #ifdef __INCLUDE__SCREEN__
      String outStr = inputString.substring(9);
      outStr.trim();
      display.clearDisplay();
      display.setCursor(0,0);
      display.print(outStr);
      display.display();
      #endif
    }
    else if(inputString.equalsIgnoreCase(F("lcd-clear")))
    {
      #ifdef __INCLUDE__SCREEN__
      display.clearDisplay();
      display.setCursor(0,0);
      display.display();
      #endif
    }
    else if(inputString.equalsIgnoreCase(F("net")))
    {
      #ifdef __INCLUDE__SCREEN__
      display.clearDisplay();
      display.setCursor(0,0);
      display.print(F("wifi: "));
      display.println(WiFi.RSSI());
      display.display();
      #endif
      Serial.print(F("wifi: "));
      Serial.println(WiFi.RSSI());
    }
    else
    {
      Serial.println(F("supported commands debug, update, interval, lcd-write, lcd-clear, net"));
    }
    inputString = "";
    stringComplete = false;
  }
}

void logValues(float tempC, float tempF, float humidity, long wifiStrength)
{
  if(!shouldLog)
  {
    return;
  }
  Serial.print(F("degrees C: "));
  Serial.print(tempC);
  Serial.print(F(" / degrees F: "));
  Serial.print(tempF);
  Serial.print(F(" / humdity %:"));
  Serial.println(humidity, 0);
  Serial.print(F("Wifi strength"));
  Serial.println(wifiStrength);
}

void updateLcd(float temp, float humidity, long wifiStrength)
{
#ifdef __INCLUDE__SCREEN__
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(F("Temp:"));
  display.print(temp, 1);
  display.print((char)223);
  display.println(F("F"));
  display.print(F("Humidity:"));
  display.print(humidity, 0);
  display.println(F("%"));
  display.print(F("wifi "));
  display.println(wifiStrength);
  display.display();
#endif
}
