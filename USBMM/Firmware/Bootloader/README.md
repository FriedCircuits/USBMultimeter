#USB Multimeter Bootloader
Posted for testing during development
Do not use since USB VID/PID needs to be changed.

Based on fork from 
[https://github.com/watterott/SAM-BAR](https://github.com/watterott/SAM-BAR)

# SAM-BAR
SAM-BAR (**S**mart **A**tmel **M**icrocontroller **B**oot **A**ssistant **R**eloaded) is an USB bootloader for Atmel/Microchip SAM D21 devices (ARM Cortex-M0+).
It is based on the following bootloaders:
* [SAM-BA (AppNote AT07175) by Atmel/Microchip (www.atmel.com)](http://www.atmel.com/images/Atmel-42366-SAM-BA-Bootloader-for-SAM-D21_ApplicationNote_AT07175.zip)
* [Arduino Zero Bootloader by Arduino LLC (www.arduino.cc)](https://github.com/arduino/ArduinoCore-samd/tree/master/bootloaders/zero)
* [SAMD-MSD-Bootloader by Justin Mattair (www.mattairtech.com)](https://github.com/mattairtech/SAMD-MSD-Bootloader)


## Features
* SAM-BA and Arduino Zero compatible Bootloader (USB CDC/VCP)
* Mass-Storage-Device Bootloader (USB MSD/MSC)
* Fits in 16KB (user program start at 0x4000)


## Software
* [Bootloader Source Code](https://github.com/friedcircuits/USBMultimeter/tree/master/USBMM/Firmware/Bootloader/bootloader)
* [Board Support Package for Arduino IDE](https://github.com/friedcircuits/USBMultimeter/tree/master/USBMM/Firmware/Bootloader/arduino)
