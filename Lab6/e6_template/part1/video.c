#include <linux/fs.h>           // struct file, struct file_operations
#include <linux/init.h>         // for __init, see code
#include <linux/module.h>       // for module init and exit macros
#include <linux/miscdevice.h>   // for misc_device_register and struct miscdev
#include <linux/uaccess.h>      // for copy_to_user, see code
#include <asm/io.h>             // for mmap

#include "../include/address_map_arm.h"

#define SUCCESS 0
#define DEV_NAME "video" // Character device name

// Declare global variables needed to use the pixel buffer
static volatile void *LW_virtual;               // used to access FPGA light-weight bridge
static volatile unsigned int * pixel_ctrl_ptr;  // virtual address of pixel buffer controller
static int pixel_buffer;    // used for virtual address of pixel buffer
static int resolution_x, resolution_y; // VGA screen size

// Declare variables and prototypes needed for a character device driver
#define MAX_SIZE 256                // we assume that no message will be longer than this
static int chardev_registered = 0;

// address is then passed to the function named get_screen_specs, which reads Resolution register in pixel controller
// use this information to set the global variables resolution_x and resolution_y
static void get_screen_specs(void) 
{   
    unsigned int resolutionReg = *(pixel_ctrl_ptr + 2);

    resolution_x = resolutionReg & 0xFFFF;
    resolution_y = resolutionReg >> 16;

    //printk(KERN_ERR "Resolution: %x, Status: %x\n", resolutionReg, *(pixel_ctrl_ptr + 3));
    //printk(KERN_ERR "ResolutionX: %d, ResolutionY: %d\n", resolution_x, resolution_y);
}

static void plot_pixel(int x, int y, short color) 
{   
    short *buf = (short *)(pixel_buffer + ((y << 10) | (x << 1)));
    *buf = color; // & 0xFFFF;
}

static void clear_screen(void) 
{
    int col=0, row=0;
    for (row = 0; row < resolution_y; ++row) {
        for (col = 0; col < resolution_x; ++col) {
            plot_pixel(col, row, 0);
        }
    }
    //printk(KERN_ERR "Clear:%d %d\n", col, row);
}

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

    // Copy time
    if (0 >= sprintf(chardev_msg, "%03d %03d\n", resolution_x, resolution_y)) {
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

// clear = command should erase the VGA screen by setting all pixels in the pixel buffer to the color 0
// pixel x,y color = should set the pixel on VGA screen at coordinates (x,y) to the value color
static ssize_t device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset) 
{
    char cmd_string[MAX_SIZE];
    size_t bytes;
    size_t notCopied = 0;

    unsigned int x, y, color;

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

    // printk(KERN_ERR "cmd: %s\n", cmd_string);

    // Check passed in cmd:
    if (0 == strncmp("clear", cmd_string, strlen("clear"))) {
        clear_screen();
    }
    else if (0 == strncmp("pixel", cmd_string, strlen("pixel"))) {
        sscanf(cmd_string, "pixel %d,%d %x", &x, &y, &color);
        //printk(KERN_ERR "x:%d, y:%d color:%x\n", x, y, color);

        if (x < 0) x = 0;
        else if (x >= resolution_x) x = resolution_x - 1;
        if (y < 0) y = 0;
        else if (y >= resolution_y) y = resolution_y - 1;
        plot_pixel(x, y, color);
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

/* Code to initialize the video driver */
static int __init start_video(void) 
{
    int err = 0;

    // generate a virtual address for the FPGA lightweight bridge
    LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);
    if (!LW_virtual) {
        printk(KERN_ERR "ioremap failed\n");
        goto error;
    }

    // Create virtual memory access to the pixel buffer controller
    pixel_ctrl_ptr = (unsigned int *) (LW_virtual + PIXEL_BUF_CTRL_BASE);
    get_screen_specs(); // determine X, Y screen size

    // Create virtual memory access to the pixel buffer
    pixel_buffer = (int) ioremap_nocache (FPGA_ONCHIP_BASE, FPGA_ONCHIP_SPAN);
    if (pixel_buffer == 0) {
        printk (KERN_ERR "Error: ioremap_nocache returned NULL\n");
        goto error;
    }

    // Erase the pixel buffer
    clear_screen();

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
    if (LW_virtual) {
        iounmap (LW_virtual);
        LW_virtual = NULL;
    }
    if (pixel_buffer != 0) {
        iounmap ((void*)pixel_buffer);
        pixel_buffer = 0;
    }
    return err;
}

static void __exit stop_video(void) 
{
    // Remove the device from the kernel
    if (chardev_registered) {
        misc_deregister (&chardev);
        printk (KERN_INFO "/dev/%s driver de-registered\n", DEV_NAME);
        chardev_registered = 0;
    }

    // unmap the physical-to-virtual mappings
    if (LW_virtual) {
        iounmap (LW_virtual);
        LW_virtual = NULL;
    }
    if (pixel_buffer != 0) {
        iounmap ((void*)pixel_buffer);
        pixel_buffer = 0;
    }
}

MODULE_LICENSE("GPL");
module_init (start_video);
module_exit (stop_video);
