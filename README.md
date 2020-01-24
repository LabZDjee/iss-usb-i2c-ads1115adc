# Node Test of an ADS1115 ADC through USB-ISS to I2C

This test program and prototype aims to use a PC to test how to talk to an [ADS115](http://www.ti.com/lit/ds/symlink/ads1115.pdf) Analog Digital Converter and evaluate necessary code planned to be coded in C in an embedded design (for a board whose name is **ANIX**)

Use of node _[serial port](https://www.npmjs.com/package/serialport)_ package to talk to a [Devantect USB-ISS - Enhanced USB-I2C Module](https://robot-electronics.co.uk/products/communication-adapters/interface-modules/usb-iss-enhanced-usb-i2c-module.html) from _Robot Electronics_ which will drive the I2C to talk to the ADS1115 ADC

How to run it (when connected to an ADS1115 ADC such as the [adafruit break board](https://www.adafruit.com/product/1085) through the USB-ISS interface):

`node --experimental-modules anix-test.mjs {comPort}`

where `{comPort}` is the communication port that the USB-ISS module elected (COMx, /dev/tty...)

This test program can talk to any of the four devices in sequence. Details of how is described in the `configuration.mjs` module, object `AnixCfgs`. Property `profile` can take any of the following values:

| 0    | disabled                                                 |
| ---- | -------------------------------------------------------- |
| 2    | two differential channels (0/1 and 2-3)                  |
| 4    | four single end channels (0, 1, 2, 3)                    |
| 12   | one differential (0/1), two single ended (2, 3) channels |
| 21   | two single ended (0, 1), one differential (2/3) channels |

Property `gains` as an array defines the maximum voltages the front amplifier supports for each channel (if less than 4 channels are used, remaining values are ignored). Its values are defined in npm module [@labzdjee/ads1115-config](https://www.npmjs.com/package/@labzdjee/ads1115-config). Property `iirTakeIns` is an array of numbers between 1 and 100, which defines which proportion in percent of a new value is injected to an IIR low pass first order filter for displayed results. A value of 100 will disable this filtering.  Period for this IIR filter is defined in `recurrenceInMs`

