#include "../physical.h"
#include "../address_map_arm.h"
#include "../audio.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

/* write a program that is capable of playing multiple tones simultaneously (chords). 
accept a single command-line input string of 13 ''1's and ''0's,*/
void setupAudioPort(volatile unsigned int *);
void write_to_audio_port(volatile unsigned int *, int);
double tones[] = {261.626, 277.183, 293.665, 311.127, 329.628, 349.228, 
                369.994, 391.995, 415.305, 440.000, 466.164, 493.883, 
                523.251};
int numTones = 13; //sizeof(tones)/sizeof(double);

int main(int argc, char *argv[])
{
    int fd_mem = -1;    // used to open /dev/mem for access to physical addresses
    void *LW_virtual;   // used to map physical addresses for the light-weight bridge
    volatile unsigned int *audio_virtual;  // used to map physical addresses for the audio port
    int i;

    int nth_sample;
    double freq = 261.626;
    int tone_volumes[13];

    char * ptr;
    int chord;

    if (argc != 2 || numTones != strlen(argv[1])) {
        printf("Program takes 1 %d-character string argument for chord: C(261.626) -> C(523.251)\n", numTones);
        return -1;
    }

    // Convert chord arg
    ptr = argv[1];
    i = 0;
    while (ptr[i] != '\0') {
        //printf("char = %c\n",ptr[i]);
        if (ptr[i] == '1') {
            //printf("i = %d\n", i);
            tone_volumes[i] = MAX_VOLUME / numTones;
            //printf("i = %d\n", tone_volumes[i]);
        }
        else {
            tone_volumes[i] = 0;
        }
        ++i;
    }

    // Create virtual memory access 
    if ((fd_mem = open_physical (fd_mem)) == -1) {
        return (-1);
    }
    
    // Memory map for HPS Mux which connects to I2C
    if ((LW_virtual = map_physical (fd_mem, LW_BRIDGE_BASE, LW_BRIDGE_SPAN)) == NULL) {
        return (-1);
    }

    // Set virtual address pointer to audio port
    audio_virtual = (unsigned int *) (LW_virtual + AUDIO_BASE);

    // To initialize the audio port you first should 
    // clear the Write FIFO by using the CW bit in the Control register
    setupAudioPort(audio_virtual);

    // Write 8000 samples = 1 second
    for (nth_sample = 0; nth_sample < 8000; nth_sample++) {
        chord = 0;
        for (i = 0; i < numTones; ++i) {
            chord += (tone_volumes[i] * sin(nth_sample * PI2 * tones[i]/SAMPLING_RATE)); //radian = 2*PI*HZ
        }
        if (chord > 0x7fffffff ){
            printf("The chord = %d\n", chord);
        }
        write_to_audio_port(audio_virtual, chord);
    }

    // Clean up
    unmap_physical ((void *)LW_virtual, LW_BRIDGE_SPAN);   // release the physical-memory mapping
    close_physical(fd_mem);

    return 0;
}

void setupAudioPort(volatile unsigned int *audio_virtual) {
    // Set control register
    // RE and WE enable interrupts -> set to zero
    // CW set to 1 to clear the Write FIFO, and remains cleared until set back to 0
    *audio_virtual = 0b1000; // enable write
    *audio_virtual = 0; // clear
}

void write_to_audio_port(volatile unsigned int *audio_virtual, int value) {
    // Check if space in write fifo
    volatile unsigned int *fifoSize = (audio_virtual + 1);
    if ((*fifoSize & 0x00FF0000) == 0) {
        while ( (*fifoSize & 0x00FF0000) == 0); // check not full
    }
    if ((*fifoSize & 0xFF000000) == 0) {
        while ( (*fifoSize & 0xFF000000) == 0); // check not full
    }
    
    *(audio_virtual + 2) = value;
    *(audio_virtual + 3) = value;
}