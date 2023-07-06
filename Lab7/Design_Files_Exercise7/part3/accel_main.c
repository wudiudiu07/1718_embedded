#include <linux/fs.h>           // struct file, struct file_operations
#include <linux/init.h>         // for __init, see code
#include <linux/module.h>       // for module init and exit macros
#include <linux/miscdevice.h>   // for misc_device_register and struct miscdev
#include <linux/uaccess.h>      // for copy_to_user, see code
#include <asm/io.h>             // for mmap

#include "../ADXL345.h"
#include "../address_map_arm.h"

#define SUCCESS 0
#define DEV_NAME "accel" // Character device name

static volatile unsigned int *MUX_virtual; 
static volatile unsigned int *I2C_virtual; 
static int16_t mg_per_lsb;
unsigned int resolution, range, rate;

// Declare variables and prototypes needed for a character device driver
#define MAX_SIZE 256                // we assume that no message will be longer than this
static int chardev_registered = 0;

//----------------------- Character Device -------------------------------------
static int device_open (struct inode *, struct file *);
static int device_release (struct inode *, struct file *);
static ssize_t device_read (struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset);

static struct file_operations chardev_fops = {
    .owner = THIS_MODULE,
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};

static int device_open(struct inode *inode, struct file *file) 
{
    return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file) 
{
    return 0;
}

// return the character string "ccc rrr"
//  ccc = three-digit decimal number for number of columns in the VGA screen
//  rrr = number of rows
static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t *offset) 
{
    size_t bytes = 0;
    size_t notCopied = 0;
    char chardev_msg[256];
    int16_t XYZ[3];

    // Read Info
    bool isNew = ADXL345_IsDataReady(I2C_virtual); 
    ADXL345_XYZ_Read(I2C_virtual, XYZ);

    // Copy time
    if (0 >= sprintf(chardev_msg, "%01d %4d %4d %4d %2d\n", isNew, XYZ[0], XYZ[1], XYZ[2], mg_per_lsb)) {
        printk (KERN_ERR "%s:%d\n", __FILE__, __LINE__);
    }
    
    bytes = (strlen(chardev_msg) + 1) - (*offset);    // how many bytes not yet sent?
    bytes = bytes > length ? length : bytes;     // too much to send all at once?

    if (bytes) {
        if ((notCopied = copy_to_user(buffer, &chardev_msg[*offset], bytes)) != 0) {
            printk (KERN_ERR "/dev/%s: copy_to_user unsuccessful %d left\n", DEV_NAME, notCopied);
        }
    }
    (*offset) = bytes;    // keep track of number of bytes sent to the user
    // printk (KERN_ERR "%d\n", bytes);

    return bytes;
}

static ssize_t device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset) 
{
    char cmd_string[MAX_SIZE];
    size_t bytes;
    size_t notCopied = 0;
    uint8_t devid;

    // Set bytes to incoming request size
    bytes = length;
    // Check if we have space for request size
    if (bytes > MAX_SIZE - 1)    // can copy all at once, or not?
        bytes = MAX_SIZE - 1;

    if ((notCopied = copy_from_user (cmd_string, buffer, bytes)) != 0) {
        printk (KERN_ERR "/dev/%s: copy_from_user unsuccessful %d left\n", DEV_NAME, notCopied);
    }
    // NULL terminate, minus 1 to remove a new line character which I don't know why -> echo stop /dev/stopwatch
    cmd_string[bytes - 1] = '\0';

    //printk(KERN_ERR "cmd: %s\n", cmd_string);

    // Check passed in cmd:
    if (0 == strncmp("device", cmd_string, strlen("device"))) {
        // 0xE5 is read from DEVID(0x00) if I2C is functioning correctly
        ADXL345_IdRead(I2C_virtual, &devid);
        printk(KERN_ERR "DevID: %x\n", devid);
    }
    else if (0 == strncmp("init", cmd_string, strlen("init"))) {
        // re-initialize ADXL345
        ADXL345_Init(I2C_virtual, &resolution, &range, &rate);
    }
    else if (0 == strncmp("calibrate", cmd_string, strlen("calibrate"))) {
        ADXL345_Calibrate(I2C_virtual);
    }
    else if (0 == strncmp("format", cmd_string, strlen("format"))) {
        sscanf(cmd_string, "format %d %d", &resolution, &range);

        //NOTE: Putting here, but wondering if should change init so it takes values are arg
        // Wondering about the "init" cmd
        resolution = (resolution == 0) ? XL345_10BIT : XL345_FULL_RESOLUTION;
        if (resolution == XL345_FULL_RESOLUTION) {
            // Enabling full resolution mode forces the least significant bit (LSB) of the sample to represent 3.9 mg
            mg_per_lsb = 3.9;
        }
        else {
            mg_per_lsb = ROUNDED_DIVISION(range*1000, 512);
        }

        switch(range) {
            case 2:
                // 10-bit 2's complement -> gives 9 bits for range = 2^9=512
                range = XL345_RANGE_2G;
                break;
            case 4:
                range = XL345_RANGE_4G;
                break;
            case 8:
                range = XL345_RANGE_8G;
                break;
            case 16:
                range = XL345_RANGE_16G;
                break; 
            default:
                printk(KERN_ERR "Invalid argument: range value not valid\n");
                break;
        }

        ADXL345_Init(I2C_virtual, &resolution, &range, &rate);
    }
    else if (0 == strncmp("rate", cmd_string, strlen("rate"))) {
        char *rate_ptr = &(cmd_string[strlen("rate ")]);
        bool validRate = 1;
        
        //printk(KERN_ERR "rate:%s", rate_ptr);
        if (0 == strcmp(rate_ptr, "0.10")) {
            rate = XL345_RATE_0_10;
        }
        else if (0 == strcmp(rate_ptr, "0.20")) {
            rate = XL345_RATE_0_20;
        }
        else if (0 == strcmp(rate_ptr, "0.39")) {
            rate = XL345_RATE_0_39;
        }
        else if (0 == strcmp(rate_ptr, "0.78")) {
            rate = XL345_RATE_0_78;
        }                       
        else if (0 == strcmp(rate_ptr, "1.56")) {
            rate = XL345_RATE_1_56;
        }   
        else if (0 == strcmp(rate_ptr, "3.13")) {
            rate = XL345_RATE_3_13;
        }     
        else if (0 == strcmp(rate_ptr, "6.25")) {
            rate = XL345_RATE_6_25;
        } 
        else if (0 == strcmp(rate_ptr, "12.5")) {
            rate = XL345_RATE_12_5;
        } 
        else if (0 == strcmp(rate_ptr, "25")) {
            rate = XL345_RATE_25;
        } 
        else if (0 == strcmp(rate_ptr, "50")) {
            rate = XL345_RATE_50;
        } 
        else if (0 == strcmp(rate_ptr, "100")) {
            rate = XL345_RATE_100;
        } 
        else if (0 == strcmp(rate_ptr, "200")) {
            rate = XL345_RATE_200;
        } 
        else if (0 == strcmp(rate_ptr, "400")) {
            rate = XL345_RATE_400;
        } 
        else if (0 == strcmp(rate_ptr, "800")) {
            rate = XL345_RATE_800;
        } 
        else if (0 == strcmp(rate_ptr, "1600")) {
            rate = XL345_RATE_1600;
        }
        else if (0 == strcmp(rate_ptr, "3200")) {
            rate = XL345_RATE_3200;
        }
        else {
            printk(KERN_ERR "Invalid Rate argument\n");
            validRate = 0;
        }

        if (validRate) {
            //printk(KERN_ERR "Wrote rate: %x\n", rate);
            ADXL345_Init(I2C_virtual, &resolution, &range, &rate);
            ADXL345_REG_READ(I2C_virtual, ADXL345_REG_BW_RATE, (uint8_t *)&rate);
            //printk(KERN_ERR "Read rate: %x\n", rate);
        }
    }

    // Note: we do NOT update *offset; we just copy the data into chardev_msg
    return bytes;
}

static struct miscdevice chardev = { 
    .minor = MISC_DYNAMIC_MINOR, 
    .name = DEV_NAME,
    .fops = &chardev_fops,
    .mode = 0666
};

//--------------------------------------------------------------------------------------------------

static int __init init_driver(void) 
{
    int err = 0;

    printk(KERN_ERR "Loading Driver\n");

    // Memory map for HPS Mux which connects to I2C
    MUX_virtual = (unsigned int *)ioremap_nocache (SYSMGR_BASE, SYSMGR_SPAN);
    if (!MUX_virtual) {
        printk(KERN_ERR "ioremap_nocache returned NULL for MUX\n");
        goto error;
    }

    // Memory map for HPS I2C port which connects to accelerometer
    I2C_virtual = (unsigned int*) ioremap_nocache (I2C0_BASE, I2C0_SPAN);
    if (!I2C_virtual) {
        printk (KERN_ERR "Error: ioremap_nocache returned NULL for I2C controller\n");
        goto error;
    }

    // Configure Pin Muxing
    Pinmux_Config(MUX_virtual);

    // Initialize I2C0 Controller
    I2C0_Init(I2C_virtual);

    resolution=XL345_10BIT; range=XL345_RANGE_16G; rate=XL345_RATE_12_5;
    ADXL345_Init(I2C_virtual, &resolution, &range, &rate);
    mg_per_lsb = ROUNDED_DIVISION(16*1000, 512); // 10-bit 2's complement -> gives 9 bits for range

    // Create character device
    err = misc_register (&chardev);
    if (err < 0) {
        printk (KERN_ERR "/dev/%s: misc_register() failed\n", DEV_NAME);
        goto error; 
    } 
    else {
        printk (KERN_INFO "/dev/%s driver registered\n", DEV_NAME);
        chardev_registered = 1;
    }

    return 0;

error:
    if (chardev_registered) {
        misc_deregister (&chardev);
        printk (KERN_INFO "/dev/%s driver de-registered\n", DEV_NAME);
        chardev_registered = 0;
    }
    if (I2C_virtual != 0) {
        iounmap ((void *)I2C_virtual);
        I2C_virtual = NULL;
    }
    if (MUX_virtual != 0) {
        iounmap ((void*)MUX_virtual);
        MUX_virtual = NULL;
    }
    return err;
}

static void __exit exit_driver(void) 
{
    printk(KERN_ERR "Loading Driver\n");

    // Remove the device from the kernel
    if (chardev_registered) {
        misc_deregister (&chardev);
        printk (KERN_INFO "/dev/%s driver de-registered\n", DEV_NAME);
        chardev_registered = 0;
    }

    if (I2C_virtual != 0) {
        iounmap ((void *)I2C_virtual);
        I2C_virtual = NULL;
    }
    if (MUX_virtual != 0) {
        iounmap ((void*)MUX_virtual);
        MUX_virtual = NULL;
    }
}

MODULE_LICENSE("GPL");
module_init (init_driver);
module_exit (exit_driver);
