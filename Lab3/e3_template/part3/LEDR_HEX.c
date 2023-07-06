#include <linux/fs.h>               // struct file, struct file_operations
#include <linux/init.h>             // for __init, see code
#include <linux/module.h>           // for module init and exit macros
#include <linux/miscdevice.h>       // for misc_device_register and struct miscdev
#include <linux/uaccess.h>          // for copy_to_user, see code
#include <asm/io.h>                 // for mmap

#include "../include/address_map_arm_vm.h"
//#include "../interrupt_ID.h"

#define SUCCESS 0
#define DEV_NAME_LEDR "LEDR"
#define DEV_NAME_HEX "HEX"
#define MAX_SIZE 256                        // we assume that no message will be longer than this
static void * LW_virtual;                   // Lightweight bridge base address
static volatile unsigned int *LEDR_ptr;     // virtual address for the LEDR port
static volatile unsigned int *HEX3_HEX0_ptr;      // virtual address for the HEX port
static volatile unsigned int *HEX5_HEX4_ptr;      // virtual address for the HEX port

// 7-seg bit patterns for digits 0-9
char seg7[10] =   {0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 
                   0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01100111};
//-------------------------------------------------------------------------
// Character Device for LEDR
static int device_open_ledr (struct inode *, struct file *);
static int device_release_ledr (struct inode *, struct file *);
static ssize_t device_write_ledr(struct file *, const char *, size_t, loff_t *);

static int chardev_ledr_registered = 0;
static struct file_operations chardev_ledr_fops = {
    .owner = THIS_MODULE,
    .write = device_write_ledr,
    .open = device_open_ledr,
    .release = device_release_ledr
};

static struct miscdevice chardev_ledr = { 
    .minor = MISC_DYNAMIC_MINOR, 
    .name = DEV_NAME_LEDR,
    .fops = &chardev_ledr_fops,
    .mode = 0666
};

/* Called when a process opens chardev */
static int device_open_ledr(struct inode *inode, struct file *file)
{
    return SUCCESS;
}

/* Called when a process closes chardev */
static int device_release_ledr(struct inode *inode, struct file *file)
{
    return 0;
}

static char chardev_msg[MAX_SIZE];  // the character array that can be read or written

/* Called when a process writes to chardev. Stores the data received into chardev_msg, and 
 * returns the number of bytes stored. */
static ssize_t device_write_ledr(struct file *filp, const char *buffer, size_t length, loff_t *offset)
{
    size_t bytes;
    unsigned int ledr_value;
    
    bytes = length;
    if (bytes > MAX_SIZE - 1)    // can copy all at once, or not?
        bytes = MAX_SIZE - 1;

    if (copy_from_user (chardev_msg, buffer, bytes) != 0) {
        printk (KERN_ERR "Error: copy_from_user unsuccessful");
    }
    // NULL terminate, minus 1 to remove a new line character which I don't know why -> echo stop /dev/stopwatch
    chardev_msg[bytes - 1] = '\0';

    //printk(KERN_ERR "%s:%d cmd: %s\n", __FILE__, __LINE__, chardev_msg);

    //convert char to int
    //long ledr_value = strtol(chardev_msg, NULL, 16);
    sscanf(chardev_msg, "%03x", &ledr_value);
    //printk(KERN_ERR "LEDR: %x\n", ledr_value);
    
    *LEDR_ptr = ledr_value;

    return bytes;
}

//-------------------------------------------------------------------------
// Character Device for HEX
static int device_open_hex (struct inode *, struct file *);
static int device_release_hex (struct inode *, struct file *);
static ssize_t device_write_hex(struct file *, const char *, size_t, loff_t *);

static int chardev_hex_registered = 0;
static struct file_operations chardev_hex_fops = {
    .owner = THIS_MODULE,
    .write = device_write_hex,
    .open = device_open_hex,
    .release = device_release_hex
};


/* Called when a process opens chardev */
static int device_open_hex(struct inode *inode, struct file *file)
{
    return SUCCESS;
}

/* Called when a process closes chardev */
static int device_release_hex(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t device_write_hex(struct file *filp, const char *buffer, size_t length, loff_t *offset)
{
    size_t bytes;
    unsigned int hex_value;

    bytes = length;
    if (bytes > MAX_SIZE - 1)    // can copy all at once, or not?
        bytes = MAX_SIZE - 1;
    
    if (copy_from_user (chardev_msg, buffer, bytes) != 0) {
        printk (KERN_ERR "Error: copy_from_user unsuccessful");
    }
    // NULL terminate, minus 1 to remove a new line character which I don't know why -> echo stop /dev/stopwatch
    chardev_msg[bytes - 1] = '\0';

    //printk(KERN_ERR "%s:%d cmd: %s\n", __FILE__, __LINE__, chardev_msg);

    //convert char to int
    sscanf(chardev_msg, "%x", &hex_value); 
    //printk(KERN_ERR "HEX: %x\n", hex_value);

    *HEX3_HEX0_ptr = (unsigned int) (seg7[hex_value%10]|seg7[(hex_value/10)%10] << 8 |
                  seg7[(hex_value/100)%10]<<16|seg7[(hex_value/1000)%10]<<24);
    *HEX5_HEX4_ptr = (unsigned int) (seg7[(hex_value/10000)%10]|
                  seg7[(hex_value/100000)%10] << 8) ;    

    return bytes;
}

static struct miscdevice chardev_hex = { 
    .minor = MISC_DYNAMIC_MINOR, 
    .name = DEV_NAME_HEX,
    .fops = &chardev_hex_fops,
    .mode = 0666
};
//-------------------------------------------------------------------------

static int __init start_chardev(void)
{
    int err = 0;

    // generate a virtual address for the FPGA lightweight bridge
    LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);
    if (!LW_virtual) {
		printk(KERN_ERR "ioremap failed\n");
        goto error;
	}
    LEDR_ptr = LW_virtual + LEDR_BASE;   // init virtual address for LEDR port
    *LEDR_ptr = 0x000;                   // clear any light

    HEX3_HEX0_ptr = LW_virtual + HEX3_HEX0_BASE;   // init virtual address for HEX port
    HEX5_HEX4_ptr = LW_virtual + HEX5_HEX4_BASE;   
    *HEX3_HEX0_ptr = 0;   // display 0
    *HEX5_HEX4_ptr = 0;

    err = misc_register (&chardev_ledr);
    if (err < 0) {
        printk (KERN_ERR "/dev/%s: misc_register() failed\n", DEV_NAME_LEDR);
        goto error;
    }
    else {
        printk (KERN_INFO "/dev/%s driver registered\n", DEV_NAME_LEDR);
        chardev_ledr_registered = 1;
    }
    strcpy (chardev_msg, "0"); /* initialize the message */

    err = misc_register (&chardev_hex);
    if (err < 0) {
        printk (KERN_ERR "/dev/%s: misc_register() failed\n", DEV_NAME_HEX);
        goto error;
    }
    else {
        printk (KERN_INFO "/dev/%s driver registered\n", DEV_NAME_HEX);
        chardev_hex_registered = 1;
    }
    strcpy (chardev_msg, "0"); /* initialize the message */
   
   return err;
   
error:
    if (chardev_ledr_registered) {
        misc_deregister (&chardev_ledr);
        printk (KERN_INFO "/dev/%s driver de-registered\n", DEV_NAME_LEDR);
        chardev_ledr_registered = 0;
    }
    if (chardev_hex_registered) {
        misc_deregister (&chardev_hex);
        printk (KERN_INFO "/dev/%s driver de-registered\n", DEV_NAME_HEX);
        chardev_hex_registered = 0;
    }
    if (LW_virtual) {
        *LEDR_ptr = 0x000;                   // clear any light
        *HEX3_HEX0_ptr = 0;   // display 0
        *HEX5_HEX4_ptr = 0;

        iounmap (LW_virtual);
        LW_virtual = NULL;
    }

   return err;
}


static void __exit stop_chardev(void)
{
    if (chardev_ledr_registered) {
        misc_deregister (&chardev_ledr);
        printk (KERN_INFO "/dev/%s driver de-registered\n", DEV_NAME_LEDR);
        chardev_ledr_registered = 0;
    }
    if (chardev_hex_registered) {
        misc_deregister (&chardev_hex);
        printk (KERN_INFO "/dev/%s driver de-registered\n", DEV_NAME_HEX);
        chardev_hex_registered = 0;
    }
    if (LW_virtual) {
        *LEDR_ptr = 0x000;                   // clear any light
        *HEX3_HEX0_ptr = 0;   // display 0
        *HEX5_HEX4_ptr = 0;

        iounmap (LW_virtual);
        LW_virtual = NULL;
    }
}


MODULE_LICENSE("GPL");
module_init (start_chardev);
module_exit (stop_chardev);
