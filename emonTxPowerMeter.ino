
// TODO doc cli
// avrdude -V -F -C /usr/share/arduino/hardware/tools/avrdude.conf -p atmega328p -P /dev/ttyACM0 -c stk500v1 -b 115200 -U flash:w:emonTxPowerMeter.ino.with_bootloader.standard.hex

#include <OneWire.h>
#include <DallasTemperature.h>
#include <EmonLib.h>
#include <timer.h>

// Configure DallasTemperature lib : no alarms
#define REQUIRESALARMS false

// Configure EmonTX Shield board
#define GPIO_ONEWIRE 4      // DS18B20 bus
#define GPIO_LED 9          // Onboard led on EmonTx sheld
#define PIN_VOLTAGE 0       // ADC pin for AC voltage sensor
#define CT_COUNT 2          // Number of CT sensors used. Must be between 1 and 4
#define TIMER_POWER 8000   // Every 8 s
#define TIMER_TEMP 100000    // Every minutes 40 s


/******************************************************* 
 * OneWire - DS18B20 data structure
 */

#define ONEWIRE_ADDR_LEN 16     // 6 bytes + 3 chars header + EOS = 16 chars

struct ST_DEVICE {
  DeviceAddress addr;
  char addr_str[ONEWIRE_ADDR_LEN];
  float tempC;    // Celcius from sensor
  float temp;     // Calibrateur temperature
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
  { "28-00000ab525b3", -3.49},
  { "28-0316859752ff", 0.0},
};

// Global variables
OneWire oneWire(GPIO_ONEWIRE);
DallasTemperature sensors(&oneWire);
ST_DEVICE *devices;
uint8_t devices_count = 0; // sizeof(array) does not work on dynamic array


/******************************************************* 
 * Power sensors data structure
 */

EnergyMonitor monitors[CT_COUNT];

auto timer = timer_create_default(); // create a timer with default settings

char *convertAddress(char *str, DeviceAddress addr)
{
  // Linux kernel format for 1-wire adresses
  snprintf(str, ONEWIRE_ADDR_LEN, "%02x-%02x%02x%02x%02x%02x%02x", addr[0], addr[6], addr[5],addr[4], addr[3], addr[2], addr[1]);
  return str;
}


float calibrate(char *addr, float input)
{
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

void fetchTemperature()
{
  sensors.requestTemperatures();
  
  for(uint8_t i=0; i < devices_count; i++)
  {
    devices[i].tempC = sensors.getTempC(devices[i].addr);
    if(devices[i].tempC == DEVICE_DISCONNECTED_C)
    { 
      devices[i].temp = DEVICE_DISCONNECTED_C;
    }
    else
    {
      devices[i].temp=calibrate(devices[i].addr_str, devices[i].tempC);
    }
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
    Serial.print(devices[i].temp);
  }

  Serial.print(" }");
}



void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("\r\nemonTxPowerMeter v1.0 started.");
  pinMode(GPIO_LED, OUTPUT);

  // Init DS18B20
  sensors.begin();
  devices_count = sensors.getDS18Count();
  
  // Count devices on the bus
  Serial.print("Found ");
  Serial.print(devices_count, DEC);
  Serial.println(" DS18B20 devices :");

  devices = (ST_DEVICE*) malloc(sizeof(ST_DEVICE) * devices_count);
  //devices = new ST_DEVICE[devices_count];
  
  // Scan the bus for DS18B20 devices.
  DeviceAddress deviceAddress;
  oneWire.reset_search();
  for(uint8_t i=0; oneWire.search(deviceAddress);)
  {
    if (sensors.validAddress(deviceAddress) && sensors.validFamily(deviceAddress))
    { 
      if (i >= devices_count)
      { 
        Serial.println("ERROR: Index error while counting devices");
        break;
      }
      
      memcpy(devices[i].addr, deviceAddress, sizeof(DeviceAddress));
      convertAddress(devices[i].addr_str, deviceAddress);
      i++;
    }
  }

  // Display list for found devices
  for(uint8_t i=0; i < devices_count; i++)
  {
    Serial.print(" -> ");
    Serial.println(devices[i].addr_str);
  }

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

  // Setup timer
  timer.every(TIMER_POWER, [](void*) -> bool {runPower(); return true;} );
  timer.every(TIMER_TEMP, [](void*) -> bool {runTemp(); return true;} );

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
  runTemp();
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

void runTemp()
{
  digitalWrite(GPIO_LED, HIGH);
  
  // Get data
  fetchTemperature();

  // Send data
  Serial.print("{ \"temp\": ");
  printTemperature();
  Serial.println(" }");
  
  digitalWrite(GPIO_LED, LOW);
}

void loop()
{ 
  timer.tick();
}
