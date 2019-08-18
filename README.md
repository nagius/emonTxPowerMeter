# emonTxPowerMeter

This sketch is designed to run on an Arduino Uno with the emonTX shield designed by [OpenEnergyMonitor](https://openenergymonitor.org/) to create an OpenSource power meter.

It compute power reading from current transformer sensors clamped on electrical wires.
It also add capabilities to fetch data from the one-wire bus exposed on the hat. It's currently compatible with DS18B20 for temperature and DS2438 + HIH4000 for temperature and humidity.
The output data is sent periodically in a JSON format on the serial port.

For more information on the emonTx products, see the [OpenEneryMonitor](https://guide.openenergymonitor.org/setup/emontx/) website.

## Hardware 

You will need an Aruino Uno, its power supply, an [AC-AC voltage sensor](https://shop.openenergymonitor.com/ac-ac-power-supply-adapter-ac-voltage-sensor-euro-plug/) and an [emonTx shield with one or more CT sensors](https://shop.openenergymonitor.com/emontx-shield/).

Please refer to the official documentation to assemble and setup the emonTx module on top of the arduino :

https://wiki.openenergymonitor.org/index.php/EmonTx_Arduino_Shield

Then clamp the CT sensors on the appliances you want to monitor :

https://learn.openenergymonitor.org/electricity-monitoring/ct-sensors/installation

*REMEMBER TO SWITCH OFF YOUR MAIN POWER WHEN WORKING AROUND UNPROTECTED POWER CABLES*

You can also connect few 1-wire sensors to the emonTx shield.
If you're interested in humidity sensors, have a look at the [DS2438 + HIH4000](onewire/) module.

## Configuration

This sketch require very few configuration if you're using the standard emonTx shield for Arduino Uno.
Simply adjust the variable `CT_COUNT` to the number of CT sensors in your setup.

An accurate measurement will require proper calibration to fit your setup and electrical charateristics.
Please refer to the official documentation to apply the calibration procedure properly : https://learn.openenergymonitor.org/electricity-monitoring/ctac/calibration
The values you need to tune are in the function `initializeCT()`.

## Compilation

This sketch can be uploaded to the arduino with the Arduino IDE. You will need the following libraries intalled :

  - OneWire
  - DallasTemperature
  - EmonLib
  - arduino-timer


Optionnaly, to upload a pre-compiled firmware from the command line (for example if your board is not connected to the machine you run the IDE), use :
```
avrdude -V -F -C /usr/share/arduino/hardware/tools/avrdude.conf -p atmega328p -P /dev/ttyACM0 -c stk500v1 -b 115200 -U flash:w:emonTxPowerMeter.ino.with_bootloader.standard.hex
```

## Usage

Simply plug the board on a USB port and open the attached serial port (most likely `/dev/ttyACM0`).
The arduino will boot-up, detect all 1-wire devices and start reporting values on the serial port. Settings are 115200-8-N-1.

It will calculate and display power measurements every 8 seconds (see `TIMER_POWER`) and temperature+humidity measurements every 40 seconds (see `TIMER_ONEWIRE`).

Output example :
```
emonTxPowerMeter v1.1 started.
Found 5 devices on 1-wire bus :
 -> 28-00000ab525b3
 -> 28-0416549140ff
 -> 28-0316859752ff
 -> 28-00044c9e09ff
 -> 26-00000238aec7 +H
Initializing 2 power sensors.
{ "power": { "CT1": { "realPower": 6.30, "apparentPower": 158.13, "powerFactor": 0.04, "Irms": 0.67, "Vrms": 235.36 }, "CT2": { "realPower": 139.22, "apparentPower": 222.39, "powerFactor": 0.63, "Irms": 0.94, "Vrms": 235.49 }  } }
{ "celcius": { "28-00000ab525b3": 23.61, "28-0416549140ff": 22.50, "28-0316859752ff": 22.56, "28-00044c9e09ff": 21.44, "26-00000238aec7": 15.38 } }
{ "humidity": { "26-00000238aec7": 70.11 } }
{ "power": { "CT1": { "realPower": 2.94, "apparentPower": 99.38, "powerFactor": 0.03, "Irms": 0.42, "Vrms": 235.37 }, "CT2": { "realPower": 142.34, "apparentPower": 217.96, "powerFactor": 0.65, "Irms": 0.93, "Vrms": 235.40 }  } }
{ "power": { "CT1": { "realPower": 6.10, "apparentPower": 60.64, "powerFactor": 0.10, "Irms": 0.26, "Vrms": 235.25 }, "CT2": { "realPower": 142.07, "apparentPower": 216.39, "powerFactor": 0.66, "Irms": 0.92, "Vrms": 235.27 }  } }
{ "power": { "CT1": { "realPower": 3.05, "apparentPower": 39.89, "powerFactor": 0.08, "Irms": 0.17, "Vrms": 235.13 }, "CT2": { "realPower": 144.63, "apparentPower": 217.03, "powerFactor": 0.67, "Irms": 0.92, "Vrms": 235.02 }  } }

```

The data is encoded in JSON, one line per type of data : power, temperature, humidity.
See the script [emonTxPowerMeter-parser.rb](misc/emonTxPowerMeter-parser.rb) for an example of parser.

## License

Copyleft 2019 - Nicolas AGIUS - GNU GPLv3

Thanks to Joe Bechter for the DS2438 Arduino library.
