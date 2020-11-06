// this sketch is for a adafruit huzzah esp32
#define __INCLUDE__SCREEN__
#define __ENABLE_DEEP_SLEEP__

#include "secrets.h"
#include "AdafruitIO_WiFi.h"
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

AdafruitIO_Feed *tempFeed = io.feed("building.temperature");
AdafruitIO_Feed *humidityFeed = io.feed("building.humidity");
AdafruitIO_Feed *wifiFeed = io.feed("building.wifi-strength");
AdafruitIO_Feed *pressureFeed = io.feed("building.pressure");
AdafruitIO_Feed *iaqFeed = io.feed("building.iaq");
AdafruitIO_Feed *vocFeed = io.feed("building.voc");

#include <SPI.h>
#include <Wire.h>
  
#ifdef __INCLUDE__SCREEN__
  #include <Adafruit_GFX.h>
  #include <Adafruit_SH110X.h>
 
  Adafruit_SH110X display = Adafruit_SH110X(64, 128, &Wire);
#endif 

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60        /* Time ESP32 will go to sleep (in seconds) */

#include "bsec.h"
Bsec iaqSensor;
/* Configure the BSEC library with information about the sensor
    18v/33v = Voltage at Vdd. 1.8V or 3.3V
    3s/300s = BSEC operating mode, BSEC_SAMPLE_RATE_LP or BSEC_SAMPLE_RATE_ULP
    4d/28d = Operating age of the sensor in days
    generic_18v_3s_4d
    generic_18v_3s_28d
    generic_18v_300s_4d
    generic_18v_300s_28d
    generic_33v_3s_4d
    generic_33v_3s_28d
    generic_33v_300s_4d
    generic_33v_300s_28d
*/
const uint8_t bsec_config_iaq[] = {
#include "config/generic_33v_3s_4d/bsec_iaq.txt"
};

RTC_DATA_ATTR uint8_t sensor_state[BSEC_MAX_STATE_BLOB_SIZE] = {0};
RTC_DATA_ATTR bool hasState = false;

bsec_virtual_sensor_t sensor_list[10] = {
  BSEC_OUTPUT_RAW_TEMPERATURE,
  BSEC_OUTPUT_RAW_PRESSURE,
  BSEC_OUTPUT_RAW_HUMIDITY,
  BSEC_OUTPUT_RAW_GAS,
  BSEC_OUTPUT_IAQ,
  BSEC_OUTPUT_STATIC_IAQ,
  BSEC_OUTPUT_CO2_EQUIVALENT,
  BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
  BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
  BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
};

bool shouldLog = true;
bool forceUpdate = true;

unsigned long prevMillisCalcValues = 0;
long calcValuesInterval = TIME_TO_SLEEP * 1000;

String inputString = "";
bool stringComplete = false;

void setup() {
  Serial.begin(115200);

  Wire.begin();
  iaqSensor.begin(BME680_I2C_ADDR_SECONDARY, Wire);

  if (!CheckSensor()) {
    Serial.printf("Failed to init BME680, check wiring! \n");
    return;
  }
  iaqSensor.setConfig(bsec_config_iaq);
  if (!CheckSensor()) {
    Serial.println("Failed to set config! \n");
    return;
  }

#ifdef __ENABLE_DEEP_SLEEP__
  if (hasState) {
    //DumpState("setState", sensor_state);
    iaqSensor.setState(sensor_state);
    if (!CheckSensor()) {
      Serial.println("Failed to set state!");
      return;
    } else {
      Serial.println("Successfully set state");
    }
  } else {
    Serial.println("Saved state missing");
  }
#endif
 
  iaqSensor.updateSubscription(sensor_list, sizeof(sensor_list), BSEC_SAMPLE_RATE_LP);
  if (!CheckSensor()) {
    Serial.println("Failed to update subscription!");
    return;
  }

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
  display.begin(0x3C, true); // Address 0x3C default
  
  // Clear the buffer.
  display.clearDisplay();
  display.display();
 
  display.setRotation(1);
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
#endif

#ifdef __ENABLE_DEEP_SLEEP__
  iaqSensor.getState(sensor_state);
  hasState = true;
  //DumpState("getState", sensor_state);
  CheckSensor();
  
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
  iaqSensor.run();
  
  tempFeed->save((iaqSensor.temperature * 9 / 5) + 32, 0, 0, 0, 2);
  humidityFeed->save(iaqSensor.humidity, 0, 0, 0, 2);
  wifiFeed->save(WiFi.RSSI(), 0, 0, 0);
  pressureFeed->save(iaqSensor.pressure / 3386.39, 0, 0, 0, 2);
  iaqFeed->save(iaqSensor.staticIaq, 0, 0, 0, 2);
  vocFeed->save(iaqSensor.breathVocEquivalent, 0, 0, 0, 2);

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
  Serial.print("Temperature = "); Serial.print(iaqSensor.temperature); Serial.println(" *C");
  Serial.print("Pressure = "); Serial.print(iaqSensor.pressure / 3386.39); Serial.println(" inHg");
  Serial.print("Humidity = "); Serial.print(iaqSensor.humidity); Serial.println(" %");
  Serial.print("IAQ = "); Serial.print(iaqSensor.staticIaq); Serial.println("");
  Serial.print("IAQ Accuracy = "); Serial.println(iaqSensor.iaqAccuracy);
  Serial.print("CO2 equiv = "); Serial.print(iaqSensor.co2Equivalent); Serial.println("");
  Serial.print("Breath VOC = "); Serial.print(iaqSensor.breathVocEquivalent); Serial.println("");
  Serial.print(F("Wifi strength: "));Serial.println(WiFi.RSSI());
  
}

void updateLcd()
{
#ifdef __INCLUDE__SCREEN__
  display.clearDisplay();
  display.print("Temperature: "); display.println((iaqSensor.temperature * 9 / 5) + 32);
  display.print("Pressure: "); display.print(iaqSensor.pressure / 3386.39); display.println(" inHg");
  display.print("Humidity: "); display.print(iaqSensor.humidity); display.println(" %");
  display.print("IAQ: "); display.println(iaqSensor.staticIaq); 
  display.print("IAQ Accur: "); display.println(iaqSensor.iaqAccuracy);
  display.print("VOC: "); display.println(iaqSensor.breathVocEquivalent);
  display.print("Wifi strength: ");display.println(WiFi.RSSI());
  display.display();
#endif
}

bool CheckSensor() 
{
  if (iaqSensor.status < BSEC_OK) {
    Serial.printf("BSEC error, status %d!", iaqSensor.status);
    return false;;
  } else if (iaqSensor.status > BSEC_OK) {
    Serial.printf("BSEC warning, status %d!", iaqSensor.status);
  }

  if (iaqSensor.bme680Status < BME680_OK) {
    Serial.printf("Sensor error, bme680_status %d!", iaqSensor.bme680Status);
    return false;
  } else if (iaqSensor.bme680Status > BME680_OK) {
    Serial.printf("Sensor warning, status %d!", iaqSensor.bme680Status);
  }

  return true;
}

void DumpState(const char* name, const uint8_t* state) {
  Serial.printf("%s: \n", name);
  for (int i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++) {
    Serial.printf("%02x ", state[i]);
    if (i % 16 == 15) {
      Serial.print("\n");
    }
  }
  Serial.print("\n");
}
