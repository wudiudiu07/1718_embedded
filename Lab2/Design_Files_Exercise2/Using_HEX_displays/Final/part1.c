#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <time.h>
#include "address_map_arm.h"
#include <string.h>

/* Prototypes for functions used to access physical memory addresses */
int open_physical (int);
void * map_physical (int, unsigned int, unsigned int);
void close_physical (int);
int unmap_physical (void *, unsigned int);

// used to exit the program cleanly
volatile sig_atomic_t stop;
void catchSIGINT(int);

unsigned int charToHex(char c) {
    unsigned int hex_bit_values;
    switch(c) {
        case 'I':
            hex_bit_values = 0b110;
            break;
        case 'n':
            hex_bit_values = 0b01010100;
            break;
        case 't':
            hex_bit_values = 0b01111000;
            break;
        case 'e':
            hex_bit_values = 0b01111001;
            break;
        case 'l':
            hex_bit_values = 0b00111000;
            break;
        case ' ':
            hex_bit_values = 0;
            break;
        case 'S':
            hex_bit_values = 0b01101101;
            break;
        case 'o':
            hex_bit_values = 0b01011100;
            break;
        case 'C':
            hex_bit_values = 0b00111001;
            break;
        case 'F':
            hex_bit_values = 0b01110001;
            break;            
        case 'P':
            hex_bit_values = 0b01110011;
            break;
        case 'G':
            hex_bit_values = 0b01111101;
            break;            
        case 'A':
            hex_bit_values = 0b01110111;
            break;    
    }
    return hex_bit_values;
}

void StringToHex(char *msg, int offset, unsigned int * HEX_Top_ptr, unsigned int * HEX_Bot_ptr) {
    unsigned int hex_top_val = 0;
    unsigned int hex_bot_val = 0;
    int i = 0;
        
    for (i = 0; i < 2; ++i, ++offset) {
        char c = *(msg + offset);
        if (c == '\0') { // End of string
            offset = 0;
            c = *(msg + offset);
        }
        hex_bot_val |= charToHex(c) << (8 - (i* 8));
    }   

   for (i = 0; i < 4; ++i, ++offset) {
        char c = *(msg + offset);
        if (c == '\0') { // End of string
            offset = 0;
            c = *(msg + offset);
        }
        hex_top_val |= charToHex(c) << (24 - (i * 8));
    }  

    *HEX_Top_ptr = hex_top_val;
    *HEX_Bot_ptr = hex_bot_val;
}

// Display to Terminal
void outTerminal(char *msg, int offset) {
    const int displaySize = 6;
    char display[displaySize + 1];
    int i = 0;
    
    for (i = 0; i < displaySize; ++i, ++offset) {
        if (msg[offset] == '\0') {
            offset = 0;
        }
        display[i] = msg[offset];
    }
    display[6] = '\0';

    // Clear terminal window
    printf ("\e[2J");
    fflush (stdout);

    // Draw Box for 6 digits
    printf("\e[01;100H ------ \n");
    printf("\e[02;100H|\e[31m%s\e[00m|\n", display);
    printf("\e[03;100H ------ \n");
}


int main(void)
{
   volatile unsigned int * HEX_Top_ptr, *HEX_Bot_ptr;
   volatile unsigned int *KEY_ptr;
   
   int fd = -1;               // used to open /dev/mem for access to physical addresses
   void *LW_virtual;          // used to map physical addresses for the light-weight bridge
    
    struct timespec ts;
	time_t start_time, elapsed_time;

    // Setup scroll loop delay
    ts.tv_sec = 0;										// used to delay
	ts.tv_nsec = 500000000;							// 10^8 ns = 0.1 sec

	// catch SIGINT from ctrl+c, instead of having it abruptly close this program
    signal(SIGINT, catchSIGINT);
    start_time = time (NULL);

   // Create virtual memory access to the FPGA light-weight bridge
   if ((fd = open_physical (fd)) == -1)
      return (-1);
   if ((LW_virtual = map_physical (fd, LW_BRIDGE_BASE, LW_BRIDGE_SPAN)) == NULL)
      return (-1);

    // Set virtual address pointer to I/O port
    HEX_Top_ptr = (unsigned int *) (LW_virtual + HEX3_HEX0_BASE);
    HEX_Bot_ptr = (unsigned int *) (LW_virtual + HEX5_HEX4_BASE);
    
    KEY_ptr = (unsigned int *) (LW_virtual + KEY_BASE);
    *(KEY_ptr + 0x3) = 0xF;
    
   // Scroll Message on HEX LEDS
    char msg[] = "Intel SoC FPGA ";
    int epoch = 0;
    int output = 1; // Boolean
    while (!stop) {
        // Check if push button
        if ( *(KEY_ptr + 0x3) > 0 ) {
            *(KEY_ptr + 0x3) = 0xF; // Clear button push
            output = !output;
        }
        
        if (output) {
            outTerminal(msg, epoch);
            StringToHex(msg, epoch, (unsigned int *)HEX_Top_ptr, (unsigned int *)HEX_Bot_ptr);

            /* wait for timer */
            nanosleep (&ts, NULL);
            elapsed_time = time (NULL) - start_time;
            if (elapsed_time > 1000 || elapsed_time < 0) stop = 1;
            
            ++epoch;
            if (epoch > strlen(msg)) {
                epoch = 0;
            }
        }

        //printf("Key Push=%d\n", *(KEY_ptr + 0x3));
    }
    
   unmap_physical (LW_virtual, LW_BRIDGE_SPAN);   // release the physical-memory mapping
   close_physical (fd);   // close /dev/mem
   return 0;
}


/* Function to allow clean exit of the program */
void catchSIGINT(int signum)
{
	stop = 1;
}


// Open /dev/mem, if not already done, to give access to physical addresses
int open_physical (int fd)
{
   if (fd == -1)
      if ((fd = open( "/dev/mem", (O_RDWR | O_SYNC))) == -1)
      {
         printf ("ERROR: could not open \"/dev/mem\"...\n");
         return (-1);
      }
   return fd;
}

// Close /dev/mem to give access to physical addresses
void close_physical (int fd)
{
   close (fd);
}

/*
 * Establish a virtual address mapping for the physical addresses starting at base, and
 * extending by span bytes.
 */
void* map_physical(int fd, unsigned int base, unsigned int span)
{
   void *virtual_base;

   // Get a mapping from physical addresses to virtual addresses
   virtual_base = mmap (NULL, span, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, base);
   if (virtual_base == MAP_FAILED)
   {
      printf ("ERROR: mmap() failed...\n");
      close (fd);
      return (NULL);
   }
   return virtual_base;
}

/*
 * Close the previously-opened virtual address mapping
 */
int unmap_physical(void * virtual_base, unsigned int span)
{
   if (munmap (virtual_base, span) != 0)
   {
      printf ("ERROR: munmap() failed...\n");
      return (-1);
   }
   return 0;
}

