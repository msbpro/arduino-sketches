#define __INCLUDE__SCREEN__
#define __ENABLE_DEEP_SLEEP__

#include "secrets.h"
#include "AdafruitIO_WiFi.h"
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

AdafruitIO_Feed *soilFeed = io.feed("plant.soil");

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
  #define OLED_CS    14 //cs
  #define OLED_RESET 32 //rst
  Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);
  
#endif 

const uint64_t uS_TO_S_FACTOR = 1000000;  /* Conversion factor for micro seconds to seconds */
const uint64_t TIME_TO_SLEEP = 60 * 60;  /* Time ESP32 will go to sleep (in seconds) */
const uint64_t SLEEP_TIME = uS_TO_S_FACTOR * TIME_TO_SLEEP;

#define VBATPIN A13
float measuredvbat;
const float lowBat = 3.4;

bool shouldLog = true;
bool forceUpdate = true;

unsigned long prevMillisCalcValues = 0;
uint64_t calcValuesInterval = TIME_TO_SLEEP * 1000;

String inputString = "";
bool stringComplete = false;

//RTC_DATA_ATTR int bootCount = 0;
const int SOIL_SENSOR_PIN = A2;

const int airVal = 2570;
const int waterVal = 1050;
int intervals =(airVal - waterVal) / 3;
int sensorVal = 0;
long humidityVal = 0;

void setup() {
  Serial.begin(115200);
  
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
  esp_sleep_enable_timer_wakeup(SLEEP_TIME);

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

  //get battery voltage
  measuredvbat = analogRead(VBATPIN);
  //convert it from 12 bit ADC
  measuredvbat /= 4095.0;
  //measured across a voltage divider in half so times by 2
  measuredvbat *= 2;
  //pin reference voltage
  measuredvbat *= 3.3;
  //adc reference voltage of esp32 (https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html#overview)
  measuredvbat *= 1.1;
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
    checkSensor();
  }
}

void sleepLoop()
{
  commonLoop();
  checkSensor();
}

void checkSensor()
{
  sensorVal = analogRead(SOIL_SENSOR_PIN);
  humidityVal = map(sensorVal, airVal, waterVal, 0, 100);
  soilFeed->save(humidityVal, 0, 0, 0);
  logValues();
  updateLcd();
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

void logValues()
{
  if(!shouldLog)
  {
    return;
  }
  Serial.print("Soil Sensor: "); Serial.println(sensorVal);
  Serial.print("Humidity %: "); Serial.println(humidityVal);
  if(sensorVal > waterVal && sensorVal < (waterVal + intervals))
  {
    Serial.println("Very Wet");
  }
  else if(sensorVal > (waterVal + intervals) && sensorVal < (airVal - intervals))
  {
    Serial.println("Wet");
  }
  else if(sensorVal < airVal && sensorVal > (airVal - intervals))
  {
    Serial.println("Dry");
  }
  else
  {
    Serial.println("Very Dry");
  }
  Serial.print(F("Wifi strength: "));Serial.println(WiFi.RSSI());
  Serial.print("VBat: " ); Serial.println(measuredvbat);
  if(measuredvbat <= lowBat)
  {
    Serial.println("LOW BATTERY!");
  }
}

void updateLcd()
{
#ifdef __INCLUDE__SCREEN__
  display.clearDisplay();
  display.print("Soil Sensor: "); display.println(sensorVal);
  display.print("Humidity %: "); display.println(humidityVal);
  if(sensorVal > waterVal && sensorVal < (waterVal + intervals))
  {
    display.println("Very Wet");
  }
  else if(sensorVal > (waterVal + intervals) && sensorVal < (airVal - intervals))
  {
    display.println("Wet");
  }
  else if(sensorVal < airVal && sensorVal > (airVal - intervals))
  {
    display.println("Dry");
  }
  else
  {
    display.println("Very Dry");
  }
  display.print("Wifi strength: ");display.println(WiFi.RSSI());
  display.print("VBat: " ); display.println(measuredvbat);
  if(measuredvbat <= lowBat)
  {
    display.println("LOW BATTERY!");
  }
  display.display();
#endif
}
