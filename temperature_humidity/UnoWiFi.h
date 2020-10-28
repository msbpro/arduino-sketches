#ifndef  UNOWIFI_H
#define  UNOWIFI_H

#include "consts.h"

#if ARDUINO >= 100
 #include "Arduino.h"
 #include "Print.h"
#else
 #include "WProgram.h"
#endif

#ifdef __ENABLE_WIFI__
  #include <SPI.h>
  #include <WiFiNINA.h>
#endif

class UnoWiFi
{
  public:
    UnoWiFi(),
    UnoWiFi(bool debug);
   void 
    setup(),
    loop(),
    updateDebug(bool debug),
    printCurrentNet();

   protected:
    void 
      printWifiData(),
      printMacAddress(byte mac[]);
};

#endif
