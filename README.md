

Upload firmware from command line :
avrdude -V -F -C /usr/share/arduino/hardware/tools/avrdude.conf -p atmega328p -P /dev/ttyACM0 -c stk500v1 -b 115200 -U flash:w:emonTxPowerMeter.ino.with_bootloader.standard.hex
