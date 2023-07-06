#include "../physical.h"
#include "../address_map_arm.h"
#include "../audio.h"

#include <stdio.h>
#include <math.h>

/* write a program that plays the tones of the middle C chromatic scale through the audio-out port
program should play these tones in sequence from the low C (261.63 Hz) to the higher C (523.25 Hz),
tones should be outputted as sinusoidal waves, tone lasting 300 ms. */

double tones[] = {261.626, 277.183, 293.665, 311.127, 329.628, 349.228, 
                369.994, 391.995, 415.305, 440.000, 466.164, 493.883, 
                523.251};
const int numTones = 13; //sizeof(tones)/sizeof(double);

volatile unsigned int *audio_virtual;  // used to map physical addresses for the audio port

void setupAudioPort() {
    // Set control register
    // RE and WE enable interrupts -> set to zero
    // CW set to 1 to clear the Write FIFO, and remains cleared until set back to 0
    *audio_virtual = 0b1000; // enable write
    *audio_virtual = 0; // clear
}

void write_to_audio_port(int value) {
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

int main(void) {
    int fd_mem = -1;    // used to open /dev/mem for access to physical addresses
    void *LW_virtual;   // used to map physical addresses for the light-weight bridge
    int i;

    int nth_sample;
    int vol = MAX_VOLUME / numTones; // Max volume, which is later multiplied by sin() (which ranges from -1 to 1)

    // Create virtual memory access 
    if ((fd_mem = open_physical (fd_mem)) == -1) {
        return (-1);
    }
    
    if ((LW_virtual = map_physical (fd_mem, LW_BRIDGE_BASE, LW_BRIDGE_SPAN)) == NULL) {
        return (-1);
    }   

    // Set virtual address pointer to audio port
    audio_virtual = (unsigned int *) (LW_virtual + AUDIO_BASE);

    // To initialize the audio port you first should 
    // clear the Write FIFO by using the CW bit in the Control register
    
    setupAudioPort(); //first, clear the WRITE FIFO by using the CW bit in Control Reg
    
    //*(audio_virtual) = 0xC;     //CW=1, CR=1, WE=0, RE=0
    //*(audio_virtual) = 0x0;   // reset to 0 to use
    //printf("audio_virtual = %p\n", &audio_virtual);

    // sin function uses radians to measure the phase, 
    // need to calculate the number of radians per sample for each tone

    // use a loop to output 300 ms of samples for each tone
    // audio sample is a 32-bit signed integer
    // produce an integer sample by multiplying the output of the sin function (-1.0 to 1.0) by 0x7FFFFFFF
    // audio device sampling rate is 8,000 samples per second, which is 1/8000 second per sample
    
    // 1/8000 sec = 0.0125 second/sample = 1.25 ms/sample
    // want tone to last 300 ms
    // 300/0.125 = 2400 samples

    // Write
    for (i = 0; i < numTones; ++i) {        
        for (nth_sample = 0; nth_sample < 2400; nth_sample++) {
            write_to_audio_port( vol * sin(nth_sample * PI2 * tones[i] / SAMPLING_RATE) ); //radian = 2*PI*HZ
            //printf("%f %d\n", vol * sin(nth_sample * PI2 * tones[i] / SAMPLING_RATE),
            //         (int)(vol * sin(nth_sample * PI2 * tones[i] / SAMPLING_RATE)));
        }
    }

    // Clean up
    unmap_physical ((void *)LW_virtual, LW_BRIDGE_SPAN);   // release the physical-memory mapping
    close_physical(fd_mem);

    return 0;
}


