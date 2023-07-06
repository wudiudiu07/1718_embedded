#include <linux/fs.h>               // struct file, struct file_operations
#include <linux/init.h>             // for __init, see code
#include <linux/module.h>           // for module init and exit macros
#include <linux/miscdevice.h>       // for misc_device_register and struct miscdev
#include <linux/uaccess.h>          // for copy_to_user, see code
#include <asm/io.h>                 // for mmap
#include "../include/address_map_arm_vm.h"

#define SUCCESS 0

// two character device drivers; 
// One driver provides the state of the KEY pushbutton switches /dev/KEY
// other driver provides the state of the SW slider switches, via the file /dev/SW
#define DEV_NAME_KEY "KEY"
#define DEV_NAME_SW "SW"

//// declare global variables that will hold virtual addresses for the KEY and SW ports
static void * LW_virtual;                   // Lightweight bridge base address
static volatile unsigned int *SW_ptr;       // virtual address for the switch port
static volatile unsigned int *KEY_ptr;      // virtual address for the KEY port

#define MAX_SIZE 256                // we assume that no message will be longer than this
static char chardev_msg[MAX_SIZE];  // read string buffer - needs to be global for while(read()) as used to track bytes and offset cross calls

//-------------------------------------------------------------------------
// Character Device for KEY
static int device_open_key (struct inode *, struct file *);
static int device_release_key (struct inode *, struct file *);
static ssize_t device_read_key (struct file *, char *, size_t, loff_t *);

static int chardev_key_registered = 0;
static struct file_operations chardev_key_fops = {
    .owner = THIS_MODULE,
    .read = device_read_key,
    .open = device_open_key,
    .release = device_release_key
};

/* Called when a process opens chardev */
static int device_open_key(struct inode *inode, struct file *file)
{
    return SUCCESS;
}

/* Called when a process closes chardev */
static int device_release_key(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t device_read_key(struct file *filp, char *buffer, size_t length, loff_t *offset) {
    size_t bytes = 0;
    size_t notCopied = 0;

    // For the KEY driver you should read the state of the KEYs from the port's Edgecapture register
    unsigned int key_value = *(KEY_ptr + 0x3);

    // Return the KEY values to the user as character data (ASCII) in the read function for the KEY driver
    // One way to convert binary data into character data is to make use of a library function such as sprintf
    if (0 >= sprintf(chardev_msg, "%01x\n", key_value)) {
        printk (KERN_ERR "%s:%d\n", __FILE__, __LINE__);
    }

    bytes = (strlen(chardev_msg) + 1) - (*offset);    // how many bytes not yet sent?
    bytes = bytes > length ? length : bytes;     // too much to send all at once?

    if (bytes) {
        if ((notCopied = copy_to_user(buffer, &chardev_msg[*offset], bytes)) != 0) {
            printk (KERN_ERR "/dev/%s: copy_to_user unsuccessful %d left\n", DEV_NAME_KEY, notCopied);
        }
        // Clear the PIO edgecapture register (clear any saved states for push button)
        *(KEY_ptr + 0x3) = 0xF; 
   }
   (*offset) = bytes;    // keep track of number of bytes sent to the user

    return bytes;
}

static struct miscdevice chardev_key = { 
    .minor = MISC_DYNAMIC_MINOR, 
    .name = DEV_NAME_KEY,
    .fops = &chardev_key_fops,
    .mode = 0666
};
//-------------------------------------------------------------------------

// Character Device for SW
static int device_open_sw (struct inode *, struct file *);
static int device_release_sw (struct inode *, struct file *);
static ssize_t device_read_sw (struct file *, char *, size_t, loff_t *);

static int chardev_sw_registered = 0;
static struct file_operations chardev_sw_fops = {
    .owner = THIS_MODULE,
    .read = device_read_sw,
    .open = device_open_sw,
    .release = device_release_sw
};

/* Called when a process opens chardev */
static int device_open_sw(struct inode *inode, struct file *file)
{
    return SUCCESS;
}

/* Called when a process closes chardev */
static int device_release_sw(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t device_read_sw(struct file *filp, char *buffer, size_t length, loff_t *offset) {
    size_t bytes = 0;
    size_t notCopied = 0;

    // For the SW driver, read the slider switch settings from the port's Data register
    unsigned int sw_value = *(SW_ptr);

    // Return these values to the user in the form of character data, via the driver's read function
    if (0 >= sprintf(chardev_msg, "%03x\n", sw_value)) {
        printk (KERN_ERR "%s:%d\n", __FILE__, __LINE__);
    }

    bytes = (strlen(chardev_msg) + 1) - (*offset);    // how many bytes not yet sent?
    bytes = bytes > length ? length : bytes;     // too much to send all at once?

    if (bytes) {
        if ((notCopied = copy_to_user(buffer, &chardev_msg[*offset], bytes)) != 0) {
            printk (KERN_ERR "/dev/%s: copy_to_user unsuccessful %d left\n", DEV_NAME_SW, notCopied);
        }
    }
    (*offset) = bytes;    // keep track of number of bytes sent to the user
    
    return bytes;
}

static struct miscdevice chardev_sw = { 
    .minor = MISC_DYNAMIC_MINOR, 
    .name = DEV_NAME_SW,
    .fops = &chardev_sw_fops,
    .mode = 0666
};
//-------------------------------------------------------------------------


static int __init init_drivers(void) {
    int err = 0;
    
    // Initialize these variables in init_drivers, using the kernel function ioremap_nocache
    LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);
    if (!LW_virtual) {
		printk(KERN_ERR "ioremap failed\n");
        goto error;
	}

    KEY_ptr = (unsigned int *)(LW_virtual + KEY_BASE);
    SW_ptr = (unsigned int *)(LW_virtual + SW_BASE);

    // Clear the PIO edgecapture register (clear any saved states for push button)
    *(KEY_ptr + 0x3) = 0xF; 

    err = misc_register (&chardev_key);
    if (err < 0) {
        printk (KERN_ERR "/dev/%s: misc_register() failed\n", DEV_NAME_KEY);
        goto error; 
    } 
    else {
        printk (KERN_INFO "/dev/%s driver registered\n", DEV_NAME_KEY);
    }
    chardev_key_registered = 1;

    err = misc_register (&chardev_sw);
    if (err < 0) {
        printk (KERN_ERR "/dev/%s: misc_register() failed\n", DEV_NAME_SW);
        goto error; 
    }
    else {
        printk (KERN_INFO "/dev/%s driver registered\n", DEV_NAME_SW);
    }
    chardev_sw_registered = 1;

    return err;

error:
    if (chardev_key_registered) {
        misc_deregister (&chardev_key);
        printk (KERN_INFO "/dev/%s driver de-registered\n", DEV_NAME_KEY);
        chardev_key_registered = 0;
    }
    if (chardev_sw_registered) {
        misc_deregister (&chardev_sw);
        printk (KERN_INFO "/dev/%s driver de-registered\n", DEV_NAME_SW);
        chardev_sw_registered = 0;
    }
    if (LW_virtual) {
        iounmap (LW_virtual);
        LW_virtual = NULL;
    }

    return err;
}

static void __exit stop_drivers(void)
{
    if (chardev_key_registered) {
        misc_deregister (&chardev_key);
        printk (KERN_INFO "/dev/%s driver de-registered\n", DEV_NAME_KEY);
        chardev_key_registered = 0;
    }
    if (chardev_sw_registered) {
        misc_deregister (&chardev_sw);
        printk (KERN_INFO "/dev/%s driver de-registered\n", DEV_NAME_SW);
        chardev_sw_registered = 0;
    }
    if (LW_virtual) {
        iounmap (LW_virtual);
        LW_virtual = NULL;
    }
}

MODULE_LICENSE("GPL");
module_init (init_drivers);
module_exit (stop_drivers);