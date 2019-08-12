/*************************************************************************
 *
 * This file is part of the emonTxPowerMeter Arduino sketch.
 * Copyleft 2019 Nicolas Agius <nicolas.agius@lps-it.fr>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * ***********************************************************************/

 /*
  * This sketch is designed to run on an Arduino Uno with the emonTX shield.
  * It fetches power reading from CT sensors and temperature+humidity from
  * DS18B20, DS2438 sensors on the one-wire bus.  The output data is sent
  * periodically in JSON on the serial port.
  *
  * For more information on the shield, see
  * https://wiki.openenergymonitor.org/index.php/EmonTx_Arduino_Shield
  *
  * Required libraries :
  *  - OneWire
  *  - DallasTemperature
  *  - EmonLib
  *  - arduino-timer
  */

#include <OneWire.h>
#include <DallasTemperature.h>
#include <EmonLib.h>
#include <timer.h>
#include "DS2438.h"

// Configure DallasTemperature lib : no alarms
#define REQUIRESALARMS false

// Configure EmonTX Shield board
#define GPIO_ONEWIRE 4        // OneWire pin bus
#define GPIO_LED 9            // Onboard led on EmonTx sheld
#define PIN_VOLTAGE 0         // ADC pin for AC voltage sensor
#define CT_COUNT 2            // Number of CT sensors used. Must be between 1 and 4

// Configure output timer
#define TIMER_POWER 8000      // Send power readings every 8 s
#define TIMER_ONEWIRE 100000  // Send temperature and humidity readings every minutes and 40 s


/******************************************************* 
 * OneWire sensors data structure
 */

// Data for DS2438 lib
#define DS2438MODEL 0x26      // Family ID

#define ONEWIRE_ADDR_LEN 16   // 6 bytes + 3 chars header + EOS = 16 chars

struct ST_DEVICE {
  DeviceAddress addr;
  char addr_str[ONEWIRE_ADDR_LEN];
  float rawT;         // Raw temperature from sensor in Celcius
  float T;            // Calibrated temperature in Celcius
  bool hasHumidity;
  float rh;           // Relative humidity in %
  DS2438 *driver;
};

struct ST_CALIBRATION {
  char addr[ONEWIRE_ADDR_LEN];
  float offset;
};

// Builtin calibration data for known devices
ST_CALIBRATION calibration[] = {
  { "28-0416549140ff", -0.12},
  { "28-0316442b74ff", -1.56},
  { "28-04168438ddff", -0.18},
  { "28-00044c9e09ff", -0.43},
  { "28-000003dd2964", -0.43},
  { "28-00000ab5377d", -1.18},
  { "28-00000ab525b3", -3.01},
  { "28-0316859752ff", 0.0},
  { "26-00000238aec7", -1.28},
};

// Global variables
OneWire oneWire(GPIO_ONEWIRE);
DallasTemperature sensors(&oneWire);
ST_DEVICE *devices;
uint8_t devices_count = 0; // sizeof(array) does not work on dynamic array

EnergyMonitor monitors[CT_COUNT];

auto timer = timer_create_default(); // create a timer with default settings

/*******************************************************
 * Helpers
 */

char *convertAddress(char *str, DeviceAddress addr)
{
  // Linux kernel format for 1-wire adresses
  snprintf(str, ONEWIRE_ADDR_LEN, "%02x-%02x%02x%02x%02x%02x%02x", addr[0], addr[6], addr[5],addr[4], addr[3], addr[2], addr[1]);
  return str;
}

bool isDS18(DeviceAddress addr)
{
  return sensors.validFamily(addr);
}

bool isDS2438(DeviceAddress addr)
{
  switch(addr[0])
  {
    case DS2438MODEL:
      return true;
    default:
      return false;
   }
}

float calibrate(char *addr, float input)
{
  // Forward no-data
  if (input == DEVICE_DISCONNECTED_C)
    return DEVICE_DISCONNECTED_C;

  for(uint8_t i=0; i <= (sizeof(calibration) / sizeof(ST_CALIBRATION)); i++)
  {
    if(strcmp(addr, calibration[i].addr) == 0)
    {
      return input + calibration[i].offset;
    }
  }

  // No calibration data found
  return input;
}

void fetchOneWireData()
{
  sensors.requestTemperatures();
  
  for(uint8_t i=0; i < devices_count; i++)
  {
    if(isDS18(devices[i].addr))
    {
      devices[i].rawT = sensors.getTempC(devices[i].addr);
    }
    else if(isDS2438(devices[i].addr))
    {
      devices[i].driver->update();
      if(devices[i].driver->isError())
      {
        devices[i].rh = DEVICE_DISCONNECTED_C;
        devices[i].rawT = DEVICE_DISCONNECTED_C;
      }
      else
      {
        double temp = devices[i].driver->getTemperature();
        float vdd = devices[i].driver->getVoltage(DS2438_CHB);
        float vad = devices[i].driver->getVoltage(DS2438_CHA);

         // Formula from datasheet to calculate compensated relative humidity with HIH-4000 sensor
         // https://sensing.honeywell.com/honeywell-sensing-hih4000-series-product-sheet-009017-5-en.pdf
         float urh = ((vad/vdd) - 0.16) / 0.0062;
         devices[i].rh = (float)(urh / (1.0546 - 0.00216 * temp));

         devices[i].rawT = (double)temp;
      }
    }
    devices[i].T=calibrate(devices[i].addr_str, devices[i].rawT);
  }
}

void printTemperature()
{
  Serial.print("{ ");
  
  for(uint8_t i=0; i < devices_count; i++)
  {
    Serial.print(i?", \"":"\"");
    Serial.print(devices[i].addr_str);
    Serial.print("\": ");
    Serial.print(devices[i].T);
  }

  Serial.print(" }");
}

void printHumidity()
{
  Serial.print("{ ");

  for(uint8_t i=0, j=0; i < devices_count; i++)
  {
    if(devices[i].hasHumidity)
    {
      Serial.print(j++?", \"":"\"");
      Serial.print(devices[i].addr_str);
      Serial.print("\": ");
      Serial.print(devices[i].rh);
    }
  }

  Serial.print(" }");
}

void scanOneWireBus()
{
  // Init DS18B20
  sensors.begin();

  // Count devices on the bus
  devices_count = sensors.getDeviceCount();
  Serial.print("Found ");
  Serial.print(devices_count, DEC);
  Serial.println(" devices on 1-wire bus :");

  devices = (ST_DEVICE*) malloc(sizeof(ST_DEVICE) * devices_count);

  // Scan the bus for DS18B20 and DS2438 devices.
  DeviceAddress addr;
  oneWire.reset_search();
  uint8_t idx=0;
  while(oneWire.search(addr))
  {
    if (sensors.validAddress(addr))
    { 
      if (idx >= devices_count)
      { 
        Serial.println("ERROR: Index error while counting devices");
        break;
      }

      if (isDS18(addr) || isDS2438(addr))
      {
        memcpy(devices[idx].addr, addr, sizeof(DeviceAddress));
        convertAddress(devices[idx].addr_str, addr);

        if (isDS2438(addr))
        {
          devices[idx].hasHumidity = true;

          devices[idx].driver = new DS2438(&oneWire, addr);
          devices[idx].driver->begin();
        }
        else
        {
          devices[idx].hasHumidity = false;
        }

        idx++;
      }
      else
      {
        char addr_str[ONEWIRE_ADDR_LEN];
        convertAddress(addr_str, addr);
        Serial.print("Unknown device: ");
        Serial.println(addr_str);
      }
    }
  }

  // Keep number of identified devices
  devices_count = idx;

  // Display list for found devices
  for(uint8_t i=0; i < devices_count; i++)
  {
    Serial.print(" -> ");
    Serial.print(devices[i].addr_str);
    if(devices[i].hasHumidity)
      Serial.print(" +H");
    Serial.println("");
  }
}

void initializeCT()
{
  // Display number of energy monitor
  Serial.print("Initializing ");
  Serial.print(CT_COUNT, DEC);
  Serial.println(" power sensors.");

  // Calibrate energy monitors
  // Check https://learn.openenergymonitor.org/electricity-monitoring/ctac/calibration for calibration procedure
  for(uint8_t i=0; i<CT_COUNT; i++)
  {
    // CT pins are in order : CT1 = pin 1, CT2 = pin 2 ...
    // Calibration factor = CT ratio / burden resistance = (100A / 0.05A) / 33 Ohms = 60.606
    monitors[i].current(i+1, 56.9);

    // (ADC input, calibration, phase_shift)
    monitors[i].voltage(PIN_VOLTAGE, 258.4, 1.2);
  }
}

void calculatePower()
{
  for(uint8_t i=0; i<CT_COUNT; i++)
  {
    // Calculate all. No.of crossings, time-out 
    monitors[i].calcVI(20,2000);
  }
}

void printPower()
{
  Serial.print("{ ");
  
  for(uint8_t i=0; i < CT_COUNT; i++)
  {
    Serial.print("\"CT");
    Serial.print(i+1);
    Serial.print("\": { \"realPower\": ");
   
    Serial.print(monitors[i].realPower);
    Serial.print(", \"apparentPower\": ");
    Serial.print(monitors[i].apparentPower);
    Serial.print(", \"powerFactor\": ");
    Serial.print(monitors[i].powerFactor);
    Serial.print(", \"Irms\": ");
    Serial.print(monitors[i].Irms);
    Serial.print(", \"Vrms\": ");
    Serial.print(monitors[i].Vrms);
    Serial.print(i+1==CT_COUNT?" } ":" }, ");
  }
  
  Serial.print(" }");
}

void runPower()
{
  digitalWrite(GPIO_LED, HIGH);
  
  // Get data
  calculatePower();

  // Send data
  Serial.print("{ \"power\": ");
  printPower();
  Serial.println(" }");
  
  digitalWrite(GPIO_LED, LOW);
}

void runOneWire()
{
  digitalWrite(GPIO_LED, HIGH);
  
  // Get data
  fetchOneWireData();

  // Send data
  Serial.print("{ \"celcius\": ");
  printTemperature();
  Serial.println(" }");

  Serial.print("{ \"humidity\": ");
  printHumidity();
  Serial.println(" }");

  digitalWrite(GPIO_LED, LOW);
}

void setup()
{
  Serial.begin(115200);
  Serial.println("\r\nemonTxPowerMeter v1.1 started.");
  pinMode(GPIO_LED, OUTPUT);

  scanOneWireBus();
  initializeCT();

  // Setup timer
  timer.every(TIMER_POWER, [](void*) -> bool {runPower(); return true;} );
  timer.every(TIMER_ONEWIRE, [](void*) -> bool {runOneWire(); return true;} );

  //Say hello
  digitalWrite(GPIO_LED, HIGH);
  delay(300);
  digitalWrite(GPIO_LED, LOW);
  delay(100);
  digitalWrite(GPIO_LED, HIGH);
  delay(100);
  digitalWrite(GPIO_LED, LOW);
  delay(100);
  digitalWrite(GPIO_LED, HIGH);
  delay(300);
  digitalWrite(GPIO_LED, LOW);
  delay(500);

  // First run
  runPower();
  runOneWire();
}

void loop()
{ 
  timer.tick();
}
