#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>               // struct file, struct file_operations
#include <linux/init.h>             // for __init, see code
#include <linux/module.h>           // for module init and exit macros
#include <linux/miscdevice.h>       // for misc_device_register and struct miscdev
#include <linux/uaccess.h>          // for copy_to_user, see code
#include <asm/io.h>                 // for mmap

#include "../include/address_map_arm.h"
#include "../include/interrupt_ID.h"

/* character device driver that implements a stopwatch.
stopwatch should use the format MM:SS:DD - MM are minutes, SS are seconds, and DD are hundredths of a second
initialize the stopwatch time to 59:59:99 to 00:00:00 */

#define SUCCESS 0
#define DEV_NAME "stopwatch" // Character device name

static volatile void * LW_virtual;          // Lightweight bridge base address
static volatile unsigned int *timer0_ptr;   // virtual address for the FPGA timer
static volatile unsigned int *HEX3_HEX0_ptr;      // virtual address for the HEX port
static volatile unsigned int *HEX5_HEX4_ptr;      // virtual address for the HEX port

// Character device variables
#define MAX_SIZE 256                // we assume that no message will be longer than this
static int chardev_registered = 0;

// Timer values
static int time_mm = 59; //mins
static int time_ss = 59; //second
static int time_dd = 99; //hundrendths
static int display = 0; //default 0 --> running

// 7-seg bit patterns for digits 0-9
static char seg7[10] = {0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 
                   0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01100111};

// timer interrupt: gets called every 1/100 seconds
// Updates time
irq_handler_t irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
   *(timer0_ptr) = 0; //clear interrupt status bit - TO

   if (time_dd > 0) { // First try to decrement dd
      --time_dd;
   }
   else if (time_ss > 0) { // Then try to decrement ss if dd=0
      --time_ss;
      time_dd = 99; // Reset
   }
   else if (time_mm > 0) {  // Then try to decrement mm if ss=0
      --time_mm;
      time_ss = 59; // Reset
      time_dd = 99; // Reset
   }

   if (display) {
      //Display HEX output 
      *HEX3_HEX0_ptr = (unsigned int) ( seg7[time_dd%10] | (seg7[time_dd/10] << 8) |
                                       (seg7[time_ss%10] << 16) | (seg7[time_ss/10] << 24) );

      *HEX5_HEX4_ptr = (unsigned int)( seg7[time_mm%10] | (seg7[time_mm/10] << 8) );
   }

   // Check if done
   if (time_dd == 0 && time_mm == 0 && time_ss == 0) {
      *(timer0_ptr + 1) = 0x8; //stop timer
   }

   return (irq_handler_t) IRQ_HANDLED;
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

/* Called when a process opens chardev */
static int device_open(struct inode *inode, struct file *file)
{
   return SUCCESS;
}

/* Called when a process closes chardev */
static int device_release(struct inode *inode, struct file *file)
{
   return 0;
}

static void printTimerStatus(void) {
   printk (KERN_ERR "%02d:%02d:%02d\n", time_mm, time_ss, time_dd);
   printk (KERN_ERR "Status: 0x%x\n", *timer0_ptr);
   printk (KERN_ERR "Control: 0x%x\n", *(timer0_ptr + 1));
   printk (KERN_ERR "Counterlow: 0x%x\n", *(timer0_ptr + 2));
   printk (KERN_ERR "Counterhigh: 0x%x\n", *(timer0_ptr + 3));
}

/* Called when a process reads from chardev. Provides character data from chardev_msg.
 * Returns, and sets *offset to, the number of bytes read. */
static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t *offset) {
   size_t bytes = 0;
   size_t notCopied = 0;
   char chardev_msg[256];

   // Copy time
   if (0 >= sprintf(chardev_msg, "%02d:%02d:%02d\n", time_mm, time_ss, time_dd)) {
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

   //printTimerStatus();

   return bytes;
}

/* Called when a process writes to chardev. Stores the data received into chardev_msg, and 
 * returns the number of bytes stored. */
static ssize_t device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset)
{
   char cmd_string[MAX_SIZE];
   size_t bytes;
   int hasStopped = 0;
   char tmp[256];
   int i = 0, j = 0, z = 0;
   unsigned int *time[3] = {&time_mm, &time_ss, &time_dd};
   size_t notCopied = 0;

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
   
   printk(KERN_ERR "cmd: %s\n", cmd_string);

   // Check if passed in cmd is
   if (0 == strcmp("stop", cmd_string)) {
      // stop: command causes the stopwatch to pause
      *(timer0_ptr + 1) = 0b1011;
   }
   else if (0 == strcmp("run", cmd_string)) {
      // run: command causes the stopwatch to operate normally
      *(timer0_ptr + 1) = 0b0111;
   }
   else if (0 == strcmp("disp", cmd_string)) {
      // disp: command causes the stopwatch to show the time every 1/100 seconds on the seven-segment displays HEX5-HEX0
      display = 1;

      //Display HEX output here in case in stop mode
      if (time_mm == 0 && time_ss == 0 && time_dd == 0) {
         hasStopped = 1;
      }

      if ( (*(timer0_ptr + 1) & 0x8) || hasStopped) {
         *HEX3_HEX0_ptr = (unsigned int) ( seg7[time_dd%10] | (seg7[time_dd/10] << 8) |
                                          (seg7[time_ss%10] << 16) | (seg7[time_ss/10] << 24) );

         *HEX5_HEX4_ptr = (unsigned int)( seg7[time_mm%10] | (seg7[time_mm/10] << 8) );
      }
   }
   else if (0 == strcmp("nodisp", cmd_string)) {
      // nodisp: command turns off the seven-segment display feature, and clears HEX5-HEX0
      display = 0;
      *HEX3_HEX0_ptr = 0;
      *HEX5_HEX4_ptr = 0;
   }
   else { // Attempt to handle MM:SS:DD
      // MM:SS:DD: command is used to set the time
      if (time_mm == 0 && time_ss ==0 && time_dd == 0) {
         hasStopped = 1;
      }

      //cmd_string[strlen(cmd_string)] = '\0';
      //cmd_string[strlen(cmd_string) + 1] = '\0';

      j = 0; z = 0;
      for (i = 0; i <= strlen(cmd_string); ++i) {
         if (cmd_string[i] != ':' && cmd_string[i] != '\0') {
            tmp[j++] = cmd_string[i];
         }
         else {
            tmp[j] = '\0';
            if (strlen(tmp) > 0) {
               sscanf(tmp, "%02d", time[z]);
            }
            ++z; // go to next time
            j = 0; // rest for next
         }
      }

      printk (KERN_ERR "timer: %x HasReachedZero:%d\n", *(timer0_ptr + 1), hasStopped);
      printk (KERN_ERR "New Time: %02d %02d %02d\n", time_mm, time_ss, time_dd);

      //Display HEX output here in case in stop mode
      if ( (*(timer0_ptr + 1) & 0x8) || hasStopped) {
         *HEX3_HEX0_ptr = (unsigned int) ( seg7[time_dd%10] | 
                                          (seg7[time_dd/10] << 8) |
                                          (seg7[time_ss%10] << 16) | 
                                          (seg7[time_ss/10] << 24) );

         *HEX5_HEX4_ptr = (unsigned int)( seg7[time_mm%10] | 
                                          (seg7[time_mm/10] << 8) );
      }

      if (hasStopped && (time_mm > 0 || time_ss > 0 || time_dd > 0)) {
         *(timer0_ptr + 1) |= 0b11; // restart
      }
   }

   //printTimerStatus();

   // Note: we do NOT update *offset; we just copy the data into chardev_msg
   return bytes;
}

static struct miscdevice chardev = { 
    .minor = MISC_DYNAMIC_MINOR, 
    .name = DEV_NAME,
    .fops = &chardev_fops,
    .mode = 0666
};
//------------------------------------------------------------------------------

static void setupTimer0(void) {
   // Set timer start time
   // Timer0 = 100 MHz = 10^8 Hz => 1/(10^8 Hz) seconds = 0.00000001 seconds per count
   // We want 1/100 = 0.01 seconds per count
   // So, 0.01 / 0.00000001 = 1,000,000 # of counts of timer = 0xF4240 in hex
   *(timer0_ptr + 2) = 0x4240;
   *(timer0_ptr + 3) = 0xF;

   //construct the control register of Timer0
   *(timer0_ptr+1) = 0x7; // Need timer to count down to get 1/100 sec and then repeat counting, so set CONT bit
}

static int __init initStopwatch(void)
{
   int err = 0;

   // generate a virtual address for the FPGA lightweight bridge
   LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);
   if (!LW_virtual) {
      printk(KERN_ERR "ioremap failed\n");
      goto error;
	}

   timer0_ptr = (unsigned int *)(LW_virtual + TIMER0_BASE);
   setupTimer0();
   
   HEX3_HEX0_ptr = LW_virtual + HEX3_HEX0_BASE;   // init virtual address for HEX port
   HEX5_HEX4_ptr = LW_virtual + HEX5_HEX4_BASE;   
   *HEX3_HEX0_ptr = 0;   // display 0
   *HEX5_HEX4_ptr = 0;

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

   // Register the interrupt handler.
   err = request_irq (TIMER0_IRQ, (irq_handler_t) irq_handler, IRQF_SHARED, "stopwatch_irq", (void *) (irq_handler));
   if (err != 0) {
      printk (KERN_ERR "%s:%d irq register failed\n", __FILE__, __LINE__);
      goto error;
   }

   return err;

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

   return err;
}

static void __exit cleanup_handler(void)
{
   *(timer0_ptr + 1) = 0x0; // Stop timer and interrupt
   *(timer0_ptr) = 0x0; // clear timer status register

   // Clear
   *HEX3_HEX0_ptr = 0;
   *HEX5_HEX4_ptr = 0;

   free_irq (TIMER0_IRQ, (void*) irq_handler);
   
   if (chardev_registered) {
      misc_deregister (&chardev);
      printk (KERN_INFO "/dev/%s driver de-registered\n", DEV_NAME);
      chardev_registered = 0;
   }
   if (LW_virtual) {
      iounmap (LW_virtual);
      LW_virtual = NULL;
   }
}

MODULE_LICENSE("GPL");
module_init(initStopwatch);
module_exit(cleanup_handler);
