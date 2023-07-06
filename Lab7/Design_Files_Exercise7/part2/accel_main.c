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

// Declare variables and prototypes needed for a character device driver
#define MAX_SIZE 256                // we assume that no message will be longer than this
static int chardev_registered = 0;

//----------------------- Character Device -------------------------------------
static int device_open (struct inode *, struct file *);
static int device_release (struct inode *, struct file *);
static ssize_t device_read (struct file *, char *, size_t, loff_t *);

static struct file_operations chardev_fops = {
   .owner = THIS_MODULE,
   .read = device_read,
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
    uint8_t devid;

    printk(KERN_ERR "Loading Driver\n");

    mg_per_lsb = ROUNDED_DIVISION(16*1000, 512); // 10-bit 2's complement -> gives 9 bits for range

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

    // 0xE5 is read from DEVID(0x00) if I2C is functioning correctly
    ADXL345_IdRead(I2C_virtual, &devid);
    printk(KERN_ERR "DevID: %x\n", devid);

    ADXL345_Init(I2C_virtual, NULL, NULL, NULL);

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
