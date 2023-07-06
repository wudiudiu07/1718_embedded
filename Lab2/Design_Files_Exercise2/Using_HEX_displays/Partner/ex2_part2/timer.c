#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include "address_map_arm.h"
#include "interrupt_ID.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Altera University Program");
MODULE_DESCRIPTION("DE1SoC Pushbutton Interrupt Handler");
// 7-seg bit patterns for digits 0-9
char seg7[10] =   {0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 
                   0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01100111};

void * LW_virtual;             // Lightweight bridge base address
volatile unsigned int *LEDR_ptr;        // virtual address for the LEDR port
volatile unsigned int *KEY_ptr;         // virtual address for the KEY port
volatile unsigned int *Counter_ptr;     // vitual address for the timer port
volatile unsigned int *HEX3_HEX0_ptr, *HEX5_HEX4_ptr;
int time_dd = 0; //hundrendths
int time_ss = 0; //second
int time_mm = 0; //mins

irq_handler_t irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
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
                  seg7[time_ss%10]<<16 | seg7[time_ss/10]<<24 );

   *HEX5_HEX4_ptr = (unsigned int) ( seg7[time_mm%10] | seg7[time_mm/10] << 8 );
    
   return (irq_handler_t) IRQ_HANDLED;
}
//initial 
static int __init initialize_timer_handler(void)
{
   int value;
 
    //printk("Driver loaded");
    // generate a virtual address for the FPGA lightweight bridge
   LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);

   //LEDR_ptr = LW_virtual + LEDR_BASE;  // init virtual address for LEDR port
   Counter_ptr = LW_virtual + TIMER0_BASE; // init virtual address for counter port
   //init the counter start value (low & high) 5f5 E100 = 10^8 = hundredths of seconds
   *(Counter_ptr+2) = 0x4240;
   *(Counter_ptr+3) = 0xF;

   //construct the control register of Timer0
   *(Counter_ptr+1) = 0x7;

   HEX3_HEX0_ptr = LW_virtual + HEX3_HEX0_BASE;   // init virtual address for HEX port
   HEX5_HEX4_ptr = LW_virtual + HEX5_HEX4_BASE;   // init virtual address for HEX port
   *HEX3_HEX0_ptr = seg7[0] |
                  seg7[0]<<8 |
                  seg7[0]<<16 |
                  seg7[0]<<24 ;   // display 0000

   *HEX5_HEX4_ptr = seg7[0] |
                  seg7[0]<<8; //display 00

   // Register the interrupt handler.
   value = request_irq (TIMER0_IRQ, (irq_handler_t) irq_handler, IRQF_SHARED, 
      "irq_handler", (void *) (irq_handler));
   return value;
}

static void __exit cleanup_timer_handler(void)
{
   *HEX3_HEX0_ptr = 0;
   *HEX5_HEX4_ptr = 0;
   iounmap (LW_virtual);
   free_irq (TIMER0_IRQ, (void*) irq_handler);
}

module_init(initialize_timer_handler);
module_exit(cleanup_timer_handler);

