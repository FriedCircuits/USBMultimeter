#ifndef _USBDATA_H
#define _USBDATA_H

/*Header handles controlling ADG3242 to control if USB D+/D- are connected.
  Future prototype will use HD3SS6126 to control both USB2 and 3.
  We could just add another ADG3242 and only disconnect RX or TX of USB3 instead of both.
  The HD3 is a bit overkill really.
*/

#define USBDATAPORT GPIOA
#define USBDATAPAD  5

void usbdataEnable(void);
void usbdataDisable(void);


void usbdataEnable(){
    palSetPadMode(USBDATAPORT, USBDATAPAD, PAL_MODE_OUTPUT_PUSHPULL); palClearPad(USBDATAPORT, USBDATAPAD);
}

void usbdataDisable(){
    palSetPadMode(USBDATAPORT, USBDATAPAD, PAL_MODE_OUTPUT_PUSHPULL); palSetPad(USBDATAPORT, USBDATAPAD);
}

#endif /* _USBDATA_H */
