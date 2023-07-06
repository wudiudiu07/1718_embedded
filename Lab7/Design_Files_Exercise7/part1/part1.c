#include "../ADXL345.h"
#include "../physical.h"
#include "../address_map_arm.h"
#include <stdio.h>

//#define USE_INTERRUPT 1

/* Configure ADXL345:
output data rate = 12.5Hz
Fixed resolution = 10-bit resolution
+/- 16g range
-> 31mg per LSB
registers have a width of 8 bits.
 */
int main(void) 
{
    int fd_mem = -1;    // used to open /dev/mem for access to physical addresses
    volatile unsigned int *MUX_virtual;  // used to map physical addresses for the MUX on the HPS to connect I2C port
    volatile unsigned int *I2C_virtual;  // used to map physical addresses for the I2C port on the HPS to talk to accelerometer ADXL345
    uint8_t devid;

    int16_t mg_per_lsb = ROUNDED_DIVISION(16*1000, 512); // 10-bit 2's complement -> gives 9 bits for range
    int16_t XYZ[3];

    // Create virtual memory access 
    if ((fd_mem = open_physical (fd_mem)) == -1) {
        return (-1);
    }
    
    // Memory map for HPS Mux which connects to I2C
    if ((MUX_virtual = map_physical (fd_mem, SYSMGR_BASE, SYSMGR_SPAN)) == NULL) {
        return (-1);
    }

    // Memory map for HPS I2C port which connects to accelerometer
    if ((I2C_virtual = map_physical (fd_mem, I2C0_BASE, I2C0_SPAN)) == NULL) {
        return (-1);
    }

    // Configure Pin Muxing
    Pinmux_Config(MUX_virtual);

    // Initialize I2C0 Controller
    I2C0_Init(I2C_virtual);

    // 0xE5 is read from DEVID(0x00) if I2C is functioning correctly
    ADXL345_IdRead(I2C_virtual, &devid);

    printf("DevID: %x\n", devid);

    if (devid != 0xE5) {
        printf("Incorrect device ID\n");
    }
    else {
        // Initialize accelerometer chip
        ADXL345_Init(I2C_virtual, NULL, NULL, NULL);
        //ADXL345_Calibrate(I2C_virtual);

        while(1) {
            if (ADXL345_IsDataReady(I2C_virtual) 
#ifdef USE_INTERRUPT
                && ADXL345_WasActivityUpdated(I2C_virtual)
#endif
            ) {
                ADXL345_XYZ_Read(I2C_virtual, XYZ);
                
                printf("X=%d mg, Y=%d mg, Z=%d mg\n", 
                    XYZ[0]*mg_per_lsb, XYZ[1]*mg_per_lsb, XYZ[2]*mg_per_lsb);
            }
        }
    }

    // Clean up
    unmap_physical ((void *)I2C_virtual, I2C0_SPAN);   // release the physical-memory mapping
    unmap_physical ((void *)MUX_virtual, SYSMGR_SPAN);   // release the physical-memory mapping
    close_physical(fd_mem);

    return 0;
}
