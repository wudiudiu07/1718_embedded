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
char seg7[10] =   {0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 
                   0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01100111};

void * LW_virtual;             // Lightweight bridge base address
volatile unsigned int *SW_ptr;        // virtual address for the switch port
volatile unsigned int *KEY_ptr;         // virtual address for the KEY port
volatile unsigned int *Counter_ptr;     // vitual address for the timer port
volatile unsigned int *HEX3_HEX0_ptr, *HEX5_HEX4_ptr;
int time_dd = 99; //hundrendths
int time_ss = 59; //second
int time_mm = 59; //mins
int stop = 0; //default 0 --> running

//timer interrupt
irq_handler_t irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
   *(Counter_ptr) = 0;//clear TO bit
    if (!stop){
        //increment time, check if time overflow 59:59:99
        if (time_dd==0){
           if(time_ss == 0){
              if (time_mm != 0){
                 time_ss = 59;
                 time_mm --;
              }
           }
           else {
              time_dd = 99;
              time_ss --;
           }
        }
        else {
            time_dd --; //hundredths decremented by 1
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
    
    //read key edge register
    //key_value = *(KEY_ptr + 3);
    //read sw value
    sw_value = *(SW_ptr);
    
    //modify min,sec,hundredths, using SW , KEY
    if(*(KEY_ptr + 3) == 0b0001) stop = !stop;
    //printk(KERN_ERR"Stop: %d\n", *(KEY_ptr + 3));

    if (*(KEY_ptr + 3) == 0b0010){//DD
        if (*(SW_ptr) > 99) 
            time_dd = 99;
        else 
            time_dd = *(SW_ptr);
    }
    else {
        if (*(KEY_ptr + 3) == 0b0100){ //SS
            if (*(SW_ptr) > 59) 
               time_ss = 59;
            else 
               time_ss = *(SW_ptr);
        }
        else if (*(KEY_ptr + 3) == 0b1000){ //MM
            if (*(SW_ptr) > 59) 
               time_mm = 59;
            else 
               time_mm = *(SW_ptr);
        }
    }
 
    //update time; Display HEX output 
    *HEX3_HEX0_ptr = (unsigned int)( seg7[time_dd%10] | seg7[time_dd/10] << 8 |
                   seg7[time_ss%10]<<16 | seg7[time_ss/10]<<24 );
    *HEX5_HEX4_ptr = (unsigned int)(seg7[time_mm%10] | seg7[time_mm/10] << 8 );

    //clear edge register
    *(KEY_ptr + 3) = 0xF; 
    
    return (irq_handler_t) IRQ_HANDLED;
}


//initial the key interrupt
static int initialize_key_handler(void)
{
    int value;
    
    //init virtual address for KEY port
    KEY_ptr = LW_virtual + KEY_BASE;
    // Clear the PIO edgecapture register (clear any pending interrupt)
    *(KEY_ptr + 3) = 0xF; 
    // Enable IRQ generation for the 4 buttons
    *(KEY_ptr + 2) = 0xF;
    //init virtual address for SW port
    SW_ptr = LW_virtual + SW_BASE;

    value = request_irq (KEY_IRQ, (irq_handler_t) irq_handler_key, IRQF_SHARED, 
      "irq_key_handler", (void *) (irq_handler_key));
    return value;
}


//initial 
static int __init initialize_timer_handler(void)
{
   int value;
   // generate a virtual address for the FPGA lightweight bridge
   LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);

   Counter_ptr = LW_virtual + TIMER0_BASE; // init virtual address for counter port

   //init the counter start value (low & high) 5f5 E100 = 10^8 = hundredths of seconds
   *(Counter_ptr+2) = 0x4240;
   *(Counter_ptr+3) = 0xF;
   //construct the control register of Timer0
   *(Counter_ptr+1) = 0x7;

   HEX3_HEX0_ptr = LW_virtual + HEX3_HEX0_BASE;   // init virtual address for HEX port
   HEX5_HEX4_ptr = LW_virtual + HEX5_HEX4_BASE;   // init virtual address for HEX port
   *HEX3_HEX0_ptr = seg7[9] |
                  seg7[9]<<8 |
                  seg7[9]<<16 |
                  seg7[5]<<24 ;   // display 0000

   *HEX5_HEX4_ptr = seg7[9] |
                  seg7[5]<<8; //display 00

   // Register the interrupt handler.
   value = request_irq (TIMER0_IRQ, (irq_handler_t) irq_handler, IRQF_SHARED, 
      "irq_handler", (void *) (irq_handler));
      



    initialize_key_handler();
   return value;
}

/////////////////////////////////////////////////////////////////////////////////////////////



static void __exit cleanup_handler(void)
{
   *HEX3_HEX0_ptr = 0;
   *HEX5_HEX4_ptr = 0;
   free_irq (TIMER0_IRQ, (void*) irq_handler);
   free_irq (KEY_IRQ, (void*) irq_handler_key);
   iounmap (LW_virtual);
}
/////////////////////////////////////
module_init(initialize_timer_handler);

module_exit(cleanup_handler);

