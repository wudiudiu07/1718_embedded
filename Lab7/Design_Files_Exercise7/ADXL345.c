#include "ADXL345.h"
#include "address_map_arm.h"

//#define USE_INTERRUPT 1

// configure the Pin Mux block
// HPS to connect the ADXL345’s I2C wires to I2C0
void Pinmux_Config(volatile unsigned int *mux){
    // Set up pin muxing (in sysmgr) to connect ADXL345 wires to I2C0
    *(mux + SYSMGR_I2C0USEFPGA) = 0;
    *(mux + SYSMGR_GENERALIO7) = 1;
    *(mux + SYSMGR_GENERALIO8) = 1;
}

// Initialize the I2C0 controller for use with the ADXL345 chip
void I2C0_Init(volatile unsigned int *i2c){

    // Abort any ongoing transmits and disable I2C0.
    *(i2c + I2C0_ENABLE) = 2;
    
    // Wait until I2C0 is disabled
    while(((*(i2c + I2C0_ENABLE_STATUS)) & 0x1) == 1){}
    
    // Configure the config reg with the desired setting (act as 
    // a master, use 7bit addressing, fast mode (400kb/s)).
    *(i2c + I2C0_CON) = 0x65;
    
    // Set target address (disable special commands, use 7bit addressing)
    *(i2c + I2C0_TAR) = 0x53;
    
    // The ADXL345 requires the SCL clock period to be at least 2.5 ¹s, 
    // the SCL high time to be at least 0.6 ¹s, and the SCL low time to be at least 1.3 ¹s.
    // All three conditions are met by setting the high period to be 0.9 ¹s (90 cycles of the 100 MHz input clock to
    // I2C0), and setting the low period to be 1.6 ¹s (160 cycles of the 100 MHz input clock to I2C0).

    // Set SCL high/low counts (Assuming default 100MHZ clock input to I2C0 Controller).
    // The minimum SCL high period is 0.6us, and the minimum SCL low period is 1.3us,
    // However, the combined period must be 2.5us or greater, so add 0.3us to each.
    *(i2c + I2C0_FS_SCL_HCNT) = 60 + 30; // 0.6us + 0.3us
    *(i2c + I2C0_FS_SCL_LCNT) = 130 + 30; // 1.3us + 0.3us
    
    // Enable the controller
    *(i2c + I2C0_ENABLE) = 1;
    
    // Wait until controller is enabled
    while(((*(i2c + I2C0_ENABLE_STATUS)) & 0x1) == 0){}
}

// Function to allow components on the FPGA side (ex. Nios II processors) to 
// access the I2C0 controller through the F2H bridge. This function
// needs to be called from an ARM program, and to allow a Nios II program
// to access the I2C0 controller.
/*void I2C0_Enable_FPGA_Access(){

    // Deassert fpga bridge resets
    *RSTMGR_BRGMODRST = 0;
    
    // Enable non-secure masters to access I2C0
    *L3REGS_L4SP = *L3REGS_L4SP | 0x4;
    
    // Enable non-secure masters to access pinmuxing registers (in sysmgr)
    *L3REGS_L4OSC1 = *L3REGS_L4OSC1 | 0x10;
}*/

// Write value to internal register at address
void ADXL345_REG_WRITE(volatile unsigned int *i2c,
    uint8_t address, uint8_t value){
    
    // Send reg address (+0x400 to send START signal)
    *(i2c + I2C0_DATA_CMD) = address + 0x400;
    
    // Send value
    *(i2c + I2C0_DATA_CMD) = value;
}

// Read value from internal register at address
// read of a single internal register at internal address address and writes the value read into value
void ADXL345_REG_READ(volatile unsigned int *i2c, 
    uint8_t address, uint8_t *value){

    // Send reg address (+0x400 to send START signal)
    *(i2c + I2C0_DATA_CMD) = address + 0x400;
    
    // Send read signal
    *(i2c + I2C0_DATA_CMD) = 0x100;
    
    // Read the response (first wait until RX buffer contains data)  
    while (*(i2c + I2C0_RXFLR) == 0){}
    *value = *(i2c + I2C0_DATA_CMD);
}

// Read multiple consecutive internal registers
// performs a read of len consecutive internal registers, starting at internal address address
// It stores the values read in the array values
// Function is used when reading the six DATA registers inside the 
//  ADXL345, which are at consecutive addresses 0x32 - 0x37
// Single read of multiple consecutive registers, it ensures that none of the DATA registers are
//  modified while the read is being performed
void ADXL345_REG_MULTI_READ(volatile unsigned int *i2c,
    uint8_t address, uint8_t values[], uint8_t len){
    int i;
    int nth_byte = 0;

    // Send reg address (+0x400 to send START signal)
    *(i2c + I2C0_DATA_CMD) = address + 0x400;
    
    // Send read signal len times
    for (i=0;i<len;i++)
        *(i2c + I2C0_DATA_CMD) = 0x100;

    // Read the bytes
    while (len){
        if ((*(i2c + I2C0_RXFLR)) > 0){
            values[nth_byte] = *(i2c + I2C0_DATA_CMD);
            nth_byte++;
            len--;
        }
    }
}

// Initialize the ADXL345 chip
// configures the ADXL345 to run in +/- 16 g mode, and sample acceleration at a rate of 100 Hz.
// It also configures the ADXL345 to run in full resolution mode, which forces a resolution of
//  3.9 mg (the least significant bit of the DATA values represents 3.9 mg).
void ADXL345_Init(volatile unsigned int *i2c, 
    unsigned int *resolution, unsigned int *range, unsigned int *rate){
    unsigned int resolution_local, range_local, rate_local;

    // Check if need set default values
    if (resolution == NULL) {
        resolution_local = XL345_10BIT;
    }
    else {
        resolution_local = *resolution;
    }

    if (range == NULL) {
        range_local = XL345_RANGE_16G;
    }
    else {
        range_local = *range;
    }

    if (rate == NULL) {
        rate_local = XL345_RATE_12_5;
    }
    else {
        rate_local = *rate;
    }

    // stop measure
    ADXL345_REG_WRITE(i2c, ADXL345_REG_POWER_CTL, XL345_STANDBY);

    // +- 16g range, 10-bit resolution
    ADXL345_REG_WRITE(i2c, ADXL345_REG_DATA_FORMAT, 
        range_local | resolution_local);
    
    // Output Data Rate: 12.5Hz
    ADXL345_REG_WRITE(i2c, ADXL345_REG_BW_RATE, rate_local);

#ifdef USE_INTERRUPT
    // NOTE: The DATA_READY bit is not reliable. It is updated at a much higher rate than the Data Rate
    // Use the Activity and Inactivity interrupts.
    //----- Enabling interrupts -----//
    ADXL345_REG_WRITE(i2c, ADXL345_REG_THRESH_ACT, 0x04);	//activity threshold
    ADXL345_REG_WRITE(i2c, ADXL345_REG_THRESH_INACT, 0x02);	//inactivity threshold
    ADXL345_REG_WRITE(i2c, ADXL345_REG_TIME_INACT, 0x02);	//time for inactivity
    ADXL345_REG_WRITE(i2c, ADXL345_REG_ACT_INACT_CTL, 0xFF);	//Enables AC coupling for thresholds
    ADXL345_REG_WRITE(i2c, ADXL345_REG_INT_ENABLE, XL345_ACTIVITY | XL345_INACTIVITY );	//enable interrupts
    //-------------------------------//
#endif 

    // start measure
    ADXL345_REG_WRITE(i2c, ADXL345_REG_POWER_CTL, XL345_MEASURE);
}

// Calibrate the ADXL345. The DE1-SoC should be placed on a flat
// surface, and must remain stationary for the duration of the calibration.
void ADXL345_Calibrate(volatile unsigned int *i2c){
    int average_x = 0;
    int average_y = 0;
    int average_z = 0;
    int16_t XYZ[3];
    int8_t offset_x;
    int8_t offset_y;
    int8_t offset_z;
    
    int i = 0;
    uint8_t saved_bw;
    uint8_t saved_dataformat;

    // stop measure
    ADXL345_REG_WRITE(i2c, ADXL345_REG_POWER_CTL, XL345_STANDBY);
    
    // Get current offsets
    ADXL345_REG_READ(i2c, ADXL345_REG_OFSX, (uint8_t *)&offset_x);
    ADXL345_REG_READ(i2c, ADXL345_REG_OFSY, (uint8_t *)&offset_y);
    ADXL345_REG_READ(i2c, ADXL345_REG_OFSZ, (uint8_t *)&offset_z);
    
    // Use 100 hz rate for calibration. Save the current rate.
    ADXL345_REG_READ(i2c, ADXL345_REG_BW_RATE, &saved_bw);
    ADXL345_REG_WRITE(i2c, ADXL345_REG_BW_RATE, XL345_RATE_100);
    
    // Use 16g range, full resolution. Save the current format.
    ADXL345_REG_READ(i2c, ADXL345_REG_DATA_FORMAT, &saved_dataformat);
    ADXL345_REG_WRITE(i2c, ADXL345_REG_DATA_FORMAT, XL345_RANGE_16G | XL345_FULL_RESOLUTION);
    
    // start measure
    ADXL345_REG_WRITE(i2c, ADXL345_REG_POWER_CTL, XL345_MEASURE);
    
    // Get the average x,y,z accelerations over 32 samples (LSB 3.9 mg)
    while (i < 32){
		// Note: use DATA_READY here, can't use ACTIVITY because board is stationary.
        if (ADXL345_IsDataReady(i2c)){
            ADXL345_XYZ_Read(i2c, XYZ);
            average_x += XYZ[0];
            average_y += XYZ[1];
            average_z += XYZ[2];
            i++;
        }
    }
    average_x = ROUNDED_DIVISION(average_x, 32);
    average_y = ROUNDED_DIVISION(average_y, 32);
    average_z = ROUNDED_DIVISION(average_z, 32);
    
    // stop measure
    ADXL345_REG_WRITE(i2c, ADXL345_REG_POWER_CTL, XL345_STANDBY);
    
    // printf("Average X=%d, Y=%d, Z=%d\n", average_x, average_y, average_z);
    
    // Calculate the offsets (LSB 15.6 mg)
    offset_x += ROUNDED_DIVISION(0-average_x, 4);
    offset_y += ROUNDED_DIVISION(0-average_y, 4);
    offset_z += ROUNDED_DIVISION(256-average_z, 4);
    
    // printf("Calibration: offset_x: %d, offset_y: %d, offset_z: %d (LSB: 15.6 mg)\n",offset_x,offset_y,offset_z);
    
    // Set the offset registers
    ADXL345_REG_WRITE(i2c, ADXL345_REG_OFSX, offset_x);
    ADXL345_REG_WRITE(i2c, ADXL345_REG_OFSY, offset_y);
    ADXL345_REG_WRITE(i2c, ADXL345_REG_OFSZ, offset_z);
    
    // Restore original bw rate
    ADXL345_REG_WRITE(i2c, ADXL345_REG_BW_RATE, saved_bw);
    
    // Restore original data format
    ADXL345_REG_WRITE(i2c, ADXL345_REG_DATA_FORMAT, saved_dataformat);
    
    // start measure
    ADXL345_REG_WRITE(i2c, ADXL345_REG_POWER_CTL, XL345_MEASURE);
}

// Return true if there was activity since the last read (checks ACTIVITY bit).
bool ADXL345_WasActivityUpdated(volatile unsigned int *i2c){
	bool bReady = false;
    uint8_t data8;
    
    ADXL345_REG_READ(i2c, ADXL345_REG_INT_SOURCE, &data8);
    if (data8 & XL345_ACTIVITY)
        bReady = true;
    
    return bReady;
}

// Return true if there is new data (checks DATA_READY bit).
// sample acceleration 100 times a second,
// makes it possible for a program to read the ADXL345 far more often than 100 times each second, leading
// to duplicate reads of the same samples. To prevent this, we can check the Data_ready bit of the ADXL345’s
// INT_SOURCE register and only call ADXL345_XYZ_Read when there is new data available
bool ADXL345_IsDataReady(volatile unsigned int *i2c){
    bool bReady = false;
    uint8_t data8;
    
    ADXL345_REG_READ(i2c, ADXL345_REG_INT_SOURCE, &data8);
    if (data8 & XL345_DATAREADY)
        bReady = true;
    
    return bReady;
}

// Read acceleration data of all three axes
// reads the acceleration for the X, Y and Z axes and stores the data in szData16[0], szData16[1], and szData16[2]
// six DATA registers, reads two registers for each axis, one representing the bottom 8 bits and the other 
//  representing the top 8 bits of the acceleration sample
// The least-significant bit of these values represents 3.9 mg, and the values range from -4096 to 4095 
//  (a resolution of 13 bits).
void ADXL345_XYZ_Read(volatile unsigned int *i2c, int16_t szData16[3]){
    uint8_t szData8[6];
    ADXL345_REG_MULTI_READ(i2c, 0x32, (uint8_t *)&szData8, sizeof(szData8));

    szData16[0] = (szData8[1] << 8) | szData8[0]; 
    szData16[1] = (szData8[3] << 8) | szData8[2];
    szData16[2] = (szData8[5] << 8) | szData8[4];
}

// Read the ID register
void ADXL345_IdRead(volatile unsigned int *i2c, uint8_t *pId){
    ADXL345_REG_READ(i2c, ADXL345_REG_DEVID, pId);
}

void ADXL345_SETUP_TAP_DOUBLETAP(volatile unsigned int *i2c) {
    // stop measure
    ADXL345_REG_WRITE(i2c, ADXL345_REG_POWER_CTL, XL345_STANDBY);

    //Tap Detection Setting

    //3g tap threshold, scale factor = 62.5 mg => 0x30=48*62.5mg=3000mg or 3g
    ADXL345_REG_WRITE(i2c, ADXL345_THRESH_TAP, 0x30); 

    //0.02second, scale factor =  625 mus
    //0.02sec = 20000 microsec => 20000/625=32 = 0x20
    ADXL345_REG_WRITE(i2c, ADXL345_DUR, 0x20); 
    
    //0.02 = 1.25 ms (scale factor) * 16
    //0.02sec = 20ms => 20/1.25=16 = 0x10
    ADXL345_REG_WRITE(i2c, ADXL345_Latent, 0x10);

    //0.3 seconds = 1.25 ms (scale factor) * 240
    //0.3 sec = 300ms => 300/1.25=240 = 0xF0
    ADXL345_REG_WRITE(i2c, ADXL345_Window, 0xF0);
    
    ADXL345_REG_WRITE(i2c, ADXL345_TAP_AXES, ADXL345_tap_xyz); //enable X- Y- Z- axis tap detection
    
    //Enable interrupts
    ADXL345_REG_WRITE(i2c, ADXL345_REG_INT_ENABLE, XL345_DOUBLETAP | XL345_SINGLETAP);
    
    // start measure
    ADXL345_REG_WRITE(i2c, ADXL345_REG_POWER_CTL, XL345_MEASURE);
}