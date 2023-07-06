#ifndef ACCELEROMETER_ADXL345_H_
#define ACCELEROMETER_ADXL345_H_

#if defined(THIS_IS_DRIVER)
#warning "Building Driver"
#include <linux/types.h>
/*typedef unsigned char __uint8_t;
typedef unsigned short int __uint16_t;

typedef __uint8_t uint8_t;
typedef __uint16_t uint16_t;

typedef signed char __int8_t;
typedef signed short int __int16_t;

typedef __int8_t int8_t;
typedef __int16_t int16_t;*/

#else
#warning "Building User Program"
#include <stdint.h>

#define NULL 0

typedef int bool;
#define true 1
#define false 0
#endif

/* Output data rate */
#define XL345_RATE_3200       0x0f
#define XL345_RATE_1600       0x0e
#define XL345_RATE_800        0x0d
#define XL345_RATE_400        0x0c
#define XL345_RATE_200        0x0b
#define XL345_RATE_100        0x0a
#define XL345_RATE_50         0x09
#define XL345_RATE_25         0x08
#define XL345_RATE_12_5       0x07
#define XL345_RATE_6_25       0x06
#define XL345_RATE_3_13       0x05
#define XL345_RATE_1_56       0x04
#define XL345_RATE_0_78       0x03
#define XL345_RATE_0_39       0x02
#define XL345_RATE_0_20       0x01
#define XL345_RATE_0_10       0x00

/* Bit values in DATA_FORMAT                                            */

/* Register values read in DATAX0 through DATAZ1 are dependant on the 
   value specified in data format.  Customer code will need to interpret
   the data as desired.                                                 */
#define XL345_RANGE_2G             0x00
#define XL345_RANGE_4G             0x01
#define XL345_RANGE_8G             0x02
#define XL345_RANGE_16G            0x03
#define XL345_DATA_JUST_RIGHT      0x00
#define XL345_DATA_JUST_LEFT       0x04
#define XL345_10BIT                0x00
#define XL345_FULL_RESOLUTION      0x08
#define XL345_INT_LOW              0x20
#define XL345_INT_HIGH             0x00
#define XL345_SPI3WIRE             0x40
#define XL345_SPI4WIRE             0x00
#define XL345_SELFTEST             0x80

/* Bit values in INT_ENABLE, INT_MAP, and INT_SOURCE are identical
   use these bit values to read or write any of these registers.        */
#define XL345_OVERRUN              0x01
#define XL345_WATERMARK            0x02
#define XL345_FREEFALL             0x04
#define XL345_INACTIVITY           0x08
#define XL345_ACTIVITY             0x10
#define XL345_DOUBLETAP            0x20
#define XL345_SINGLETAP            0x40
#define XL345_DATAREADY            0x80

/* Bit values in POWER_CTL                                              */
#define XL345_WAKEUP_8HZ           0x00
#define XL345_WAKEUP_4HZ           0x01
#define XL345_WAKEUP_2HZ           0x02
#define XL345_WAKEUP_1HZ           0x03
#define XL345_SLEEP                0x04
#define XL345_MEASURE              0x08
#define XL345_STANDBY              0x00
#define XL345_AUTO_SLEEP           0x10
#define XL345_ACT_INACT_SERIAL     0x20
#define XL345_ACT_INACT_CONCURRENT 0x00

// Tap and Double Tap values
//#define ADXL345_threshold 0x30
//#define ADXL345_duration 0x32
//#define ADXL345_latency 0x10
//#define ADXL345_window 0xF0
#define ADXL345_tap_xyz 0x07
#define ADXL345_threshold 0x1A
#define ADXL345_duration 0x32
#define ADXL345_latency 0x16
#define ADXL345_window 0xF0

// ADXL345 Register List
#define ADXL345_REG_DEVID       		0x00
#define ADXL345_REG_POWER_CTL   		0x2D
#define ADXL345_REG_DATA_FORMAT 		0x31
#define ADXL345_REG_FIFO_CTL    		0x38
#define ADXL345_REG_BW_RATE     		0x2C
#define ADXL345_REG_INT_ENABLE  		0x2E  // default value: 0x00
#define ADXL345_REG_INT_MAP     		0x2F  // default value: 0x00
#define ADXL345_REG_INT_SOURCE  		0x30  // default value: 0x02
#define ADXL345_REG_DATAX0      		0x32  // read only
#define ADXL345_REG_DATAX1      		0x33  // read only
#define ADXL345_REG_DATAY0      		0x34  // read only
#define ADXL345_REG_DATAY1      		0x35  // read only
#define ADXL345_REG_DATAZ0      		0x36  // read only
#define ADXL345_REG_DATAZ1      		0x37  // read only
#define ADXL345_REG_OFSX        		0x1E
#define ADXL345_REG_OFSY        		0x1F
#define ADXL345_REG_OFSZ        		0x20
#define ADXL345_REG_THRESH_ACT		0x24
#define ADXL345_REG_THRESH_INACT		0x25
#define ADXL345_REG_TIME_INACT		0x26
#define ADXL345_REG_ACT_INACT_CTL	0x27

// Tap and Double Tap Register
#define ADXL345_THRESH_TAP 0x1D
#define ADXL345_DUR 0x21
#define ADXL345_Latent 0x22
#define ADXL345_Window 0x23
#define ADXL345_TAP_AXES 0x2A

// Rounded division macro
#define ROUNDED_DIVISION(n, d) (((n < 0) ^ (d < 0)) ? ((n - d/2)/d) : ((n + d/2)/d))

// ADXL345 Functions
void ADXL345_Init(volatile unsigned int *i2c, 
   unsigned int *resolution, unsigned int *range, unsigned int *rate);
void ADXL345_Calibrate(volatile unsigned int *i2c);
bool ADXL345_IsDataReady(volatile unsigned int *i2c);
void ADXL345_XYZ_Read(volatile unsigned int *i2c, int16_t szData16[3]);
void ADXL345_IdRead(volatile unsigned int *i2c, uint8_t *pId);
void ADXL345_REG_READ(volatile unsigned int *i2c, uint8_t address, uint8_t *value);
void ADXL345_REG_WRITE(volatile unsigned int *i2c, uint8_t address, uint8_t value);
void ADXL345_REG_MULTI_READ(volatile unsigned int *i2c, uint8_t address, uint8_t values[], uint8_t len);
void ADXL345_SETUP_TAP_DOUBLETAP(volatile unsigned int *i2c);

// I2C0 Functions
void I2C0_Init(volatile unsigned int *i2c);

// Pinmux Functions
void Pinmux_Config(volatile unsigned int *mux);

// Added
bool ADXL345_WasActivityUpdated(volatile unsigned int *i2c);

#endif /*ACCELEROMETER_ADXL345_SPI_H_*/
