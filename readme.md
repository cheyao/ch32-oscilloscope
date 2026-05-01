# CH32V003 Oscilloscope

![](https://cyao.dev/1000005400.webp)

See [demo video](demo.mp4)

## Routing:

Oled should be connected to the only I2C port. The data lines need a 2k pull-up resistor each.

ADC sampling ports are A0-A3.

Buttons go on PD0 (change channel) and PD2 (snapshot). They should be floating when open and gnd when closed.

Build the board however you like! It'll work.

Diagram:

![](https://cyao.dev/1000005402.webp)

## Firmware

Go to the `src` directory, and run `make`.

Then run `minichlink -c 0x1209b803 -w oscilloscope.bin flash -b` to flash your ch32.

## Bom

- 1x SSD1306 128X64 OLED
- 2x standard 6x6mm buttons
- 4x jumper wires (2 min, more optional)
- 1x ch32v003 dev board (using uiapduino in pic)
- 2x 2k resistors
- 1x Perfboard
- 4x M2 heatset inserts (OD3.5mm, 4mm length)
- 4x 5mm M2 screws with flat heads
- Optional: 1x 1uF decoupling capacitor

