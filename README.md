# Node Test of an ADS115 ADC through USB-ISS to I2C

This test program and prototype aims to use a PC to test how to talk to an [ADS115](http://www.ti.com/lit/ds/symlink/ads1115.pdf) Analog Digital Converter and evaluate necessary code planned to be coded in C in an embedded design (for a board whose name is **ANIX**)

Use of node _[serial port](https://www.npmjs.com/package/serialport)_ package to talk to a [Devantect USB-ISS - Enhanced USB-I2C Module](https://robot-electronics.co.uk/products/communication-adapters/interface-modules/usb-iss-enhanced-usb-i2c-module.html) from _Robot Electronics_ which will drive the I2C to talk to the ADS115 ADC
