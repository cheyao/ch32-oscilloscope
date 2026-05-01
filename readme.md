# CH32V003 Oscilloscope

See [demo video](demo.mp4)

## Routing:

Oled should be connected to the only I2C port. The data lines need a 2k pull-up resistor each.

ADC sampling ports are A0-A3.

Buttons go on PD0 (change channel) and PD2 (snapshot). They should be floating when open and gnd when closed.

Build the board however you like! It'll work.

## Firmware

Go to the `src` directory, and run `make`.

Then run `minichlink -c 0x1209b803 -w oscilloscope.bin flash -b` to flash your ch32.

