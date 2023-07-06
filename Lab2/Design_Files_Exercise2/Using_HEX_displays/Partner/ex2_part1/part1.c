#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include "../address_map_arm.h"
#include "../interrupt_ID.h"

/* This is a kernel module that uses interrupts from the KEY port. The interrupt service 
 * routine increments a value. The LSB is displayed as a BCD digit on the display HEX0, 
 * and the value is also displayed as a binary number on the red lights LEDR. */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Altera University Program");
MODULE_DESCRIPTION("DE1-SoC Computer Pushbutton Interrupt Handler");

// 7-seg bit patterns for digits 0-9
char seg7[10] =   {0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 
                   0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01100111};
//Intel SoC FPGA
char msg[14] =   {0b00000100, 0b01010100, 0b01111000, 0b01111001, 0b00111000, //Intel
                  0b0000000, //space
                  0b01101101, 0b01011100, 0b00111001,//SoC
                  0b0000000, //space
                  0b01110001, 0b01110011, 0b01111101, 0b01110111, //FPGA
};


void *LW_virtual;             // used to map physical addresses for the light-weight bridge
volatile int *KEY_ptr;        // virtual address for the KEY port
volatile int * HEX3_HEX0_ptr; // virtual pointer to HEX displays
volatile int * HEX5_HEX4_ptr; // virtual pointer to HEX displays
volatile sig_atomic_t stop; //stop scrolling if 1, continue if 0
void catchSIGINT(int);

int main(void){ //scrolling the message  stop if key is pressed.
    struct timespec ts;
	time_t start_time, elapsed_time;

    signal(SIGINT, catchSIGINT);
	start_time = time (NULL);

    HEX3_HEX0_ptr = LW_virtual + HEX3_HEX0_BASE;   // init virtual address for HEX port
    HEX5_HEX4_ptr = LW_virtual + HEX5_HEX4_BASE;

	ts.tv_sec = 0;										// used to delay
	ts.tv_nsec = 1000000000;							// 10^9 ns = 1 sec

    //initial HEX value
    //HEX_pattern_0 = {0b0000000, 0b0000000, 0b0000000, 0b0000000};
    //HEX_pattern_1 = {0b0000000, 0b0000000, 0b0000000};
    HEX_pattern_0 = {msg[2],msg[3],msg[4],msg[5]}; //3-0
    HEX_pattern_1 = {msg[0],msg[1]};               //5,4



	while (!stop)
	{
		*HEX3_HEX0_ptr = HEX_pattern_0;					// 7 segment
        *HEX5_HEX4_ptr = HEX_pattern_1;
		shift_pattern (&HEX_pattern_0,&HEX_pattern_1);

		/* wait for timer */
		nanosleep (&ts, NULL);

	}

	return 0;
}

void catchSIGINT(int signum)
{
	stop = 1;
}

void shift_pattern(unsigned *HEX_pattern_0, unsigned *HEX_pattern_1)
{   
    static int value = 0;
    if (value <= 8) {
        *(HEX_pattern_0) = msg[(value+2)&0xF]|  //HEX3-0
                        msg[(value+3)&0xF]<<8|
                        msg[(value+4)&0xF]<<16|
                        msg[(value+5)&0xF]<<24;

        *(HEX_pattern_1) = msg[(value)&0xF]|    //HEX5-4
                        msg[(value+1)&0xF]<<8;
    }
    else if (value <= 13){
        i=0;
        j=0;
        index = value;
        if (index !=13){
            while(i < 12-value){
                *(HEX_pattern_0) = *(HEX_pattern_0)|
                                msg[(index+2+i)&0xF]<<((3-i)*8);
                if (i<2){
                    *(HEX_pattern_1) = *(HEX_pattern_1)|
                                msg[(index+i)&0xF]<<((1-i)*8);
                }
                i++;
            }
        }
        if(index>=11){
            if(index != 13){
                while(i<=1){
                    *(HEX_pattern_1) = *(HEX_pattern_1)|
                                msg[(index+i)&0xF]<<((1-i)*8);
                i++;
                } 
            }
            else {
                *(HEX_pattern_1) = *(HEX_pattern_1)|
                                msg[(index)&0xF]<<8;
            }
        }

        while (j < value-8){
            *(HEX_pattern_0) = *(HEX_pattern_0)|
                        msg[(index-9-j)&0xF]<<(j*8);
            if(index == 13){
                *(HEX_pattern_1) = *(HEX_pattern_1)|
                        msg[0&0xF];
            }
            j++;
        }
    }


    if (value == 13)
        value = 0;
    else
        value++ ;
}

irq_handler_t irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	printk(KERN_INFO "Interrupt called\n");
    if (stop == 1) stop = 0;
    if (stop == 0) stop = 1;


   // Clear the edgecapture register (clears current interrupt)
   *(KEY_ptr + 3) = 0xF; 
   
   return (irq_handler_t) IRQ_HANDLED;
}

static int __init intitialize_pushbutton_handler(void)
{
   // generate a virtual address for the FPGA lightweight bridge
   LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);

   KEY_ptr = LW_virtual + KEY_BASE;     // init virtual address for KEY port
   // Clear the PIO edgecapture register (clear any pending interrupt)
   *(KEY_ptr + 3) = 0xF; 
   // Enable IRQ generation for the 4 buttons
   *(KEY_ptr + 2) = 0xF; 

  
   // register the interrupt handler, and then return
   return request_irq (KEY_IRQ, (irq_handler_t) irq_handler, IRQF_SHARED,
      "pushbutton_irq_handler", (void *) (irq_handler));
}

static void __exit cleanup_pushbutton_handler(void)
{
   *HEX3_HEX0_ptr = 0;
   iounmap (LW_virtual);
   free_irq (KEY_IRQ, (void*) irq_handler); // De-register irq handler
}

module_init (intitialize_pushbutton_handler);
module_exit (cleanup_pushbutton_handler);
