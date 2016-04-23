
/**************************************************************************/
/*!
    @file     Adafruit_INA219.cpp
    @author   K.Townsend (Adafruit Industries)
	@license  BSD (see license.txt)

	Driver for the INA219 current sensor

	This is a library for the Adafruit INA219 breakout
	----> https://www.adafruit.com/products/???

	Adafruit invests time and resources providing this open source code,
	please support Adafruit and open-source hardware by purchasing
	products from Adafruit!

	@section  HISTORY

    v1.0 - First release
    v2.0  - Ported to STM32 for ChibiOS by William Garrido
*/
/**************************************************************************/

#include "ina219.h"


#define INA_DRIVER  &I2CD3
#define INA_SCLPORT    GPIOA
#define INA_SDAPORT    GPIOC
/*
static const I2CConfig i2ccfg = {
	OPMODE_I2C,
	400000,
	FAST_DUTY_CYCLE_2,
};
*/


void ina219Init(void);
float ina219GetBusVoltage_V(void);
float ina219GetShuntVoltage_mV(void);
float ina219GetCurrent_mA(void);

uint32_t ina219CurrentDivider_mA;
uint32_t ina219PowerDivider_mW;

void ina219WriteRegister(uint8_t reg, uint16_t value);
void ina219ReadRegister(uint8_t reg, uint16_t *value);

void ina219SetCalibration_32V_2A(void);
//void ina219SetCalibration_32V_1A(void);
int16_t ina219GetBusVoltage_raw(void);
int16_t ina219GetShuntVoltage_raw(void);
int16_t ina219GetCurrent_raw(void);


/**************************************************************************/
/*!
    @brief  INA219B Init
*/
/**************************************************************************/
void ina219Init(void) {
  ina219CurrentDivider_mA = 0;
  ina219PowerDivider_mW = 0;

  //palSetPadMode(INA_SCLPORT, 8, PAL_MODE_ALTERNATE(4) | PAL_STM32_OTYPE_OPENDRAIN);	/* SCL */
  //palSetPadMode(INA_SDAPORT, 9, PAL_MODE_ALTERNATE(4) | PAL_STM32_OTYPE_OPENDRAIN);	/* SDA */

   // Start the I2C
  //i2cStart(INA_DRIVER, &i2ccfg);
  ina219SetCalibration_32V_2A();
  //ina219SetCalibration_32V_1A();
  //ina219SetCalibration_16V_mA();
}



/**************************************************************************/
/*!
    @brief  Sends a single command byte over I2C
*/
/**************************************************************************/
void ina219WriteRegister (uint8_t reg, uint16_t value)
{
    uint8_t		txbuf[3];
	txbuf[0] = reg;
	txbuf[1] = ((value >> 8) & 0xFF);
	txbuf[2] = (value & 0xFF);


  i2cAcquireBus(INA_DRIVER);
  i2cMasterTransmitTimeout(INA_DRIVER, INA219_ADDRESS, txbuf, 3, 0, 0, MS2ST(INA219_TIMEOUT));
  i2cReleaseBus(INA_DRIVER);
}

/**************************************************************************/
/*!
    @brief  Reads a 16 bit values over I2C
*/
/**************************************************************************/
void ina219ReadRegister(uint8_t reg, uint16_t *value)
{
  uint8_t		rxbuf[2];
  rxbuf[0] = 0;
  rxbuf[1] = 0;

  i2cAcquireBus(INA_DRIVER);
  i2cMasterTransmitTimeout(INA_DRIVER, INA219_ADDRESS, &reg, 1, rxbuf, 2, MS2ST(INA219_TIMEOUT));
  i2cReleaseBus(INA_DRIVER);

  *value = (((uint16_t)rxbuf[0]) << 8) | rxbuf[1];

}


/**************************************************************************/
/*!
    @brief  Gets the raw bus voltage (16-bit signed integer, so +-32767)
*/
/**************************************************************************/
int16_t ina219GetBusVoltage_raw()
{
  uint16_t value;
  ina219ReadRegister(INA219_REG_BUSVOLTAGE, &value);

  // Shift to the right 3 to drop CNVR and OVF and multiply by LSB
  return (int16_t)((value >> 3) * 4);
}
/**************************************************************************/
/*!
    @brief  Gets the shunt voltage in volts
*/
/**************************************************************************/
float ina219GetBusVoltage_V() {
  int16_t value = ina219GetBusVoltage_raw();
  return value * 0.001;
}
/**************************************************************************/
/*!
    @brief  Configures to INA219 to be able to measure up to 32V and 2A
            of current.  Each unit of current corresponds to 100uA, and
            each unit of power corresponds to 2mW. Counter overflow
            occurs at 3.2A.

    @note   These calculations assume a 0.1 ohm resistor is present
*/
/**************************************************************************/
void ina219SetCalibration_32V_2A(void)
{
  ina219CurrentDivider_mA = 10;  // Current LSB = 100uA per bit (1000/100 = 10)
  ina219PowerDivider_mW = 2;     // Power LSB = 1mW per bit (2/1)

  // Set Calibration register to 'Cal' calculated above
  ina219WriteRegister(INA219_REG_CALIBRATION, 0x1000);

  // Set Config register to take into account the settings above
  uint16_t config = INA219_CONFIG_BVOLTAGERANGE_32V |
                    INA219_CONFIG_GAIN_8_320MV |
                    INA219_CONFIG_BADCRES_12BIT |
                    INA219_CONFIG_SADCRES_12BIT_1S_532US |
                    INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
 ina219WriteRegister(INA219_REG_CONFIG, config);
}

/**************************************************************************/
/*!
    @brief  Configures to INA219 to be able to measure up to 32V and 1A
            of current.  Each unit of current corresponds to 40uA, and each
            unit of power corresponds to 800uW. Counter overflow occurs at
            1.3A.

    @note   These calculations assume a 0.1 ohm resistor is present
*/
/**************************************************************************/
void ina219SetCalibration_32V_1A(void)
{
  // Set multipliers to convert raw current/power values
  ina219CurrentDivider_mA = 25;      // Current LSB = 40uA per bit (1000/40 = 25)
  ina219PowerDivider_mW = 1;         // Power LSB = 800uW per bit

  // Set Calibration register to 'Cal' calculated above
  ina219WriteRegister(INA219_REG_CALIBRATION, 0x2800);

  // Set Config register to take into account the settings above
  uint16_t config = INA219_CONFIG_BVOLTAGERANGE_32V |
                    INA219_CONFIG_GAIN_8_320MV |
                    INA219_CONFIG_BADCRES_12BIT |
                    INA219_CONFIG_SADCRES_12BIT_1S_532US |
                    INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
  ina219WriteRegister(INA219_REG_CONFIG, config);
}

//Testing lower current measurements
//http://cpre.kmutnb.ac.th/esl/learning/ina219b-current-sensor/ina219_sensor_demo.ino
void ina219SetCalibration_16V_400mA(void)
{
  // Set multipliers to convert raw current/power values
  ina219CurrentDivider_mA = 20;      // Current LSB = 40uA per bit (1000/40 = 25)
  ina219PowerDivider_mW = 1;         // Power LSB = 800uW per bit

  // Set Calibration register to 'Cal' calculated above
  ina219WriteRegister(INA219_REG_CALIBRATION, 8192);

  // Set Config register to take into account the settings above
  uint16_t config = INA219_CONFIG_BVOLTAGERANGE_16V |
                    INA219_CONFIG_GAIN_1_40MV |
                    INA219_CONFIG_BADCRES_12BIT |
                    INA219_CONFIG_SADCRES_12BIT_1S_532US |
                    INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
  ina219WriteRegister(INA219_REG_CONFIG, config);
}

/**************************************************************************/
/*!
    @brief  Gets the raw current value (16-bit signed integer, so +-32767)
*/
/**************************************************************************/
int16_t ina219GetCurrent_raw() {
  uint16_t value;
  ina219ReadRegister(INA219_REG_CURRENT, &value);
  return (int16_t)value;
}

/**************************************************************************/
/*!
    @brief  Gets the current value in mA, taking into account the
            config settings and current LSB
*/
/**************************************************************************/
float ina219GetCurrent_mA() {
  float valueDec = ina219GetCurrent_raw(); //
  valueDec /= ina219CurrentDivider_mA;
  return valueDec;
}


/**************************************************************************/
/*!
    @brief  Gets the raw shunt voltage (16-bit signed integer, so +-32767)
*/
/**************************************************************************/
int16_t ina219GetShuntVoltage_raw() {
  uint16_t value;
  ina219ReadRegister(INA219_REG_SHUNTVOLTAGE, &value);
  return (int16_t)value;
}


/**************************************************************************/
/*!
    @brief  Gets the shunt voltage in mV (so +-327mV)
*/
/**************************************************************************/
float ina219GetShuntVoltage_mV() {
  int16_t value;
  value = ina219GetShuntVoltage_raw();
  return value * 0.01;
}


