#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include "../address_map_arm.h"
#include "../interrupt_ID.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Altera University Program");
MODULE_DESCRIPTION("DE1SoC Pushbutton Interrupt Handler");
// 7-seg bit patterns for digits 0-9
static char seg7[10] =   {0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 
                   0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01100111};

static void * LW_virtual;             // Lightweight bridge base address
static volatile unsigned int *SW_ptr;        // virtual address for the switch port
static volatile unsigned int *KEY_ptr;         // virtual address for the KEY port
static volatile unsigned int *Counter_ptr;     // vitual address for the timer port
static volatile unsigned int *HEX3_HEX0_ptr, *HEX5_HEX4_ptr;
static int time_dd = 0; //hundrendths
static int time_ss = 0; //second
static int time_mm = 0; //mins
static unsigned int stop = 0; //default 0 --> running

//timer interrupt
irq_handler_t irq_handler_timer(int irq, void *dev_id, struct pt_regs *regs)
{
    if (!stop){
        *(Counter_ptr) = 0;//clear TO bit
        //increment time, check if time overflow 59:59:99
        if (time_dd==99){
           time_dd = 0;
           if(time_ss == 59){
              time_ss = 0;
              if (time_mm == 59){
                 time_mm = 0;
              }
              else {
                 time_mm ++;
              }
           }
           else {
              time_ss++;
           }
        }
        else {
           time_dd ++; //hundredths incremented by 1
        }

        //Display HEX output 
        *HEX3_HEX0_ptr = (unsigned int) ( seg7[time_dd%10] | seg7[time_dd/10] << 8 |
                       seg7[time_ss%10]<<16 | seg7[time_ss/10]<<24);

        *HEX5_HEX4_ptr = (unsigned int)(seg7[time_mm%10]|seg7[time_mm/10] << 8);
    }
    return (irq_handler_t) IRQ_HANDLED;
}

//key interrupt handler, read sw and change the value of counter
irq_handler_t irq_handler_key(int irq, void *dev_id, struct pt_regs *regs)
{
    unsigned int key_value = 0;
    unsigned int sw_value = 0;
    
    //printk("%p %p\n", KEY_ptr, SW_ptr);

    //read key edge register
    key_value = *(KEY_ptr + 0x3);
    
    //clear edge register
    *(KEY_ptr + 0x3) = 0xF; 
    
    //printk("%d\n", key_value);
    
    //read sw value
    sw_value = *(SW_ptr);
    if (sw_value > 99) sw_value = 99;
    
    //modify min,sec,hundredths, using SW , KEY
    if(key_value == 0b0001) stop = !stop;
    //printk("Stop: %d\n", stop);
    if (key_value == 0b0010) //DD
        time_dd = sw_value;
    else {
        if (sw_value >59) sw_value = 59;
        if (key_value == 0b0100) //SS
            time_ss = sw_value;
        else if (key_value == 0b1000) //MM
            time_mm = sw_value;
    }
 
    //update time; Display HEX output 
    /**HEX3_HEX0_ptr = (unsigned int)( seg7[time_dd%10] | seg7[time_dd/10] << 8 |
                   seg7[time_ss%10]<<16 | seg7[time_ss/10]<<24 );
    *HEX5_HEX4_ptr = (unsigned int)(seg7[time_mm%10] | seg7[time_mm/10] << 8 );*/

    
    return (irq_handler_t) IRQ_HANDLED;
}

//initial 
static int __init initialize_timer_handler(void)
{
   int value;
   // generate a virtual address for the FPGA lightweight bridge
   LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);

   //LEDR_ptr = LW_virtual + LEDR_BASE;  // init virtual address for LEDR port
   Counter_ptr = (unsigned int *)(LW_virtual + TIMER0_BASE); // init virtual address for counter port

   //init the counter start value (low & high) 5f5 E100 = 10^8 = hundredths of seconds
   *(Counter_ptr+2) = 0x4240;
   *(Counter_ptr+3) = 0xF;
   //construct the control register of Timer0
   *(Counter_ptr+1) = 0x7;

   HEX3_HEX0_ptr = (unsigned int *)(LW_virtual + HEX3_HEX0_BASE);   // init virtual address for HEX port
   HEX5_HEX4_ptr = (unsigned int *)(LW_virtual + HEX5_HEX4_BASE);   // init virtual address for HEX port
   *HEX3_HEX0_ptr =  (unsigned int ) (seg7[0] | seg7[0]<<8 | seg7[0]<<16 | seg7[0]<<24);   // display 0000

   *HEX5_HEX4_ptr =  (unsigned int) (seg7[0] | seg7[0]<<8); //display 00

    //initial the key interrupt
    //init virtual address for KEY port
    KEY_ptr = (unsigned int *)(LW_virtual + KEY_BASE);
    // Clear the PIO edgecapture register (clear any pending interrupt)
    *(KEY_ptr + 0x3) = 0xF; 
    // Enable IRQ generation for the 4 buttons
    *(KEY_ptr + 0x2) = 0xF;
    //init virtual address for SW port
    SW_ptr = (unsigned int *)(LW_virtual + SW_BASE);
    printk("%p %p", KEY_ptr, SW_ptr);
    
    value = request_irq (KEY_IRQ, (irq_handler_t) irq_handler_key, IRQF_SHARED, 
      "irq_handler_key", (void *) (irq_handler_key));

   // Register the interrupt handler.
   value = request_irq (TIMER0_IRQ, (irq_handler_t) irq_handler_timer, IRQF_SHARED, 
      "irq_handler_timer", (void *) (irq_handler_timer));
      
   return value;
}

/////////////////////////////////////////////////////////////////////////////////////////////



static void __exit cleanup_handler(void)
{
   *HEX3_HEX0_ptr = 0;
   *HEX5_HEX4_ptr = 0;
   free_irq (TIMER0_IRQ, (void*) irq_handler_timer);
   free_irq (KEY_IRQ, (void*) irq_handler_key);
   iounmap (LW_virtual);
}
/////////////////////////////////////
module_init(initialize_timer_handler);

module_exit(cleanup_handler);

