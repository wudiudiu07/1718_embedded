#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include "address_map_arm.h"
#include "interrupt_ID.h"

/* This is a kernel module that uses interrupts from the KEY port. The interrupt service 
 * routine increments a value. The LSB is displayed as a BCD digit on the display HEX0, 
 * and the value is also displayed as a binary number on the red lights LEDR. */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Altera University Program");
MODULE_DESCRIPTION("DE1-SoC Computer Pushbutton Interrupt Handler");

// 7-seg bit patterns for digits 0-9
char seg7[10] =   {0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 
                   0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01100111};

void *LW_virtual;             // used to map physical addresses for the light-weight bridge
volatile unsigned int *KEY_ptr;        // virtual address for the KEY port
volatile unsigned int * HEX3_HEX0_ptr; // virtual pointer to HEX displays

volatile unsigned int * timer0;
unsigned int time_MM;
unsigned int time_SS;
unsigned int time_DD;


static void writeTimeStartTimer() {
    unsigned int time = time_DD + time_SS * 60 + time_MM * 60;

    *(timer0 + 4) = 0x0; // Stop timer

    *(timer0 + 0x8) = time & 0xFFFF;     // Write time low
    *(timer0 + 0xC) = (time > 16) & 0xFFFF;    // Write time high
    
    *(timer0 + 4) = 0x7; // Start timer
}

void SetupPushButton() {
    // CLear any key pushes
    (*KEY_ptr + 0) = 0x0;
    // Clear the PIO edgecapture register (clear any pending interrupt)
    *(KEY_ptr + 0xC) = 0xF; 
    // Enable IRQ generation for the first button
    *(KEY_ptr + 2) = 0x1; 
}


irq_handler_t irq_handler_timer(int irq, void *dev_id, struct pt_regs *regs)
{
    // TO bit set to 1 when reach 0 
    // ITO -> interrupt when TO bit set by FPGA

    time_DD += 1;
    if (time_DD > 99) {
        time_DD = 0;
        time_SS += 1;
        if (time_SS > 59) {
            time_SS = 0;
            time_MM += 1;
            if (time_MM > 59) {
                time_MM = 0;
            }
        }
    }

    // Clear interrupt bit
    *(timer0 + 4) = 0x0; // Stop timer
    *(timer0 + 0) = 0;

    writeTimeStartTimer();
    
   // display least-sig BCD digit on 7-segment display
   //*HEX3_HEX0_ptr = seg7[value & 0xF];

   
   return (irq_handler_t) IRQ_HANDLED;
}

irq_handler_t irq_handler_pushbutton(int irq, void *dev_id, struct pt_regs *regs)
{
    printk("%02d:%02d:%02d\n", time_MM, time_SS, time_DD);

    // Clear the PIO edgecapture register (clear any pending interrupt)
    *(KEY_ptr + 0xC) = 0xF; 

   return (irq_handler_t) IRQ_HANDLED;
}


/* Display time on HEX: MM:SS:DD using HW timer on FPGA (timer0) */

static int __init intitialize_pushbutton_handler(void)
{
    time_DD = 0;
    time_MM = 0;
    time_SS = 0;

    // generate a virtual address for the FPGA lightweight bridge
    LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);
    timer0 = LW_virtual + TIMER0_BASE;

    KEY_ptr = LW_virtual + KEY_BASE;     // init virtual address for KEY port

    HEX3_HEX0_ptr = LW_virtual + HEX3_HEX0_BASE;   // init virtual address for HEX port
   //*HEX3_HEX0_ptr = seg7[0];   // display 0


   // Write counter start ->
   // Reload counter write 1 -> CONT bit 
   // Start write 1 -> control reg START bit
    writeTimeStartTimer();
    SetupPushButton();

   // register the interrupt handler, and then return
   return request_irq (TIMER0_IRQ, (irq_handler_t) irq_handler_timer, IRQF_SHARED,
      "timer0_irq_handler", (void *) (irq_handler));

    // register the interrupt handler, and then return
   return request_irq (KEY_IRQ, (irq_handler_t) irq_handler_pushbutton, IRQF_SHARED,
      "pushbutton_irq_handler", (void *) (irq_handler));

}

static void __exit cleanup_pushbutton_handler(void)
{
   *LEDR_ptr = 0; // Turn off LEDs and 7-seg display
   *HEX3_HEX0_ptr = 0;
   iounmap (LW_virtual);
   free_irq (KEY_IRQ, (void*) irq_handler); // De-register irq handler
}

module_init (intitialize_pushbutton_handler);
module_exit (cleanup_pushbutton_handler);
