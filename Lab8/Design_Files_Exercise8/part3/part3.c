#define _GNU_SOURCE 						// required for set_processor_affinity code#
#include "../physical.h"
#include "../address_map_arm.h"
#include "../audio.h"

#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <fcntl.h>
#include <linux/input.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <string.h>

#define KEY_RELEASED 0
#define KEY_PRESSED 1
static const char *const press_type[2] = {"RELEASED", "PRESSED "};
#define NUM_OF_TONES 13
#define NUM_OF_TONES_SIZE 14

//ln -s /dev/input/by-id/usb-HTLTEK_Gaming_keyboard-event-kbd
void setupAudioPort();
void write_to_audio_port(int);
void * audio_thread();

char *Note[] = {"C#", "D#", " ", "F#", "G#", "A#", "C", "D", "E", "F", "G", "A", "B", "C"};
char ASCII[] = {'2',  '3',  '4', '5',  '6',  '7',  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I'};
//double tones[] = {261.626, 277.183, 293.665, 311.127, 329.628, 349.228, 
//                369.994, 391.995, 415.305, 440.000, 466.164, 493.883, 
//                523.251};
double tones[] ={277.183, 311.127, 0, 369.994, 415.305, 466.164, //"C#", "D#", " ", "F#", "G#", "A#"
                261.626, 293.665, 329.628, 349.228, 391.995, 440.000, 493.883, 523.251}; //"C", "D", "E", "F", "G", "A", "B", "C"
int tone_volumes[NUM_OF_TONES_SIZE];
pthread_mutex_t mutex_tone_volume; // mutex for main and audio_thread
volatile unsigned int *audio_virtual;  // used to map physical addresses for the audio port

volatile sig_atomic_t stop = 0;
void catchSIGINT(int signum){
    stop = 1;
}


// Set the current thread?s affinity to the core specified
int set_processor_affinity(unsigned int core) {
    cpu_set_t cpuset;
    pthread_t current_thread = pthread_self();
    if (core >= sysconf(_SC_NPROCESSORS_ONLN)) {
        printf("CPU Core %d does not exist!\n", core);
        return -1;
    }
    // Zero out the cpuset mask
    CPU_ZERO(&cpuset);
    // Set the mask bit for specified core
    CPU_SET(core, &cpuset);
    return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t),
        &cpuset);
}

// audio thread - to write samples 
// audio thread is similar to the code from Part II, because it has to create, and possibly sum together, sinusoids 
//  corresponding to each tone
// slowly reduces the volume of each tone depending on whether or not the key for this tone is still being pressed

// main thread writes to when a piano key is pressed tone_volume
// audio thread gradually reduces the volume of a tone depending on whether the key is still pressed

int KeyToTone(int key) {
    int tone_index = -1;

    printf("%d\n", key);
    if (key > 2 && key < 9) {
        //printf("key %c (note %s)\n", ASCII[key-3], Note[key-3]);
        tone_index = key-3;
    }
    else if (key > 15 && key < 24) {
        //printf("key %c (note %s)\n", ASCII[key-10], Note[key-10]);
        tone_index = key-10;
    }
    else {
        printf("invalid key input\n");
        //printf("You %s key code 0x%04x\n", press_type[ev.value], key); 
    }
    return tone_index;
}

int main(int argc, char *argv[])
{
    int err;
    pthread_t tid;
    int i;

    struct input_event ev;
    int fd_keyboard, event_size = sizeof (struct input_event), key;

    int fd_mem = -1;    // used to open /dev/mem for access to physical addresses
    void *LW_virtual;   // used to map physical addresses for the light-weight bridge

    char *keyboard; //= "/dev/input/by-id/usb-HTLTEK_Gaming_keyboard-event-kbd";
    int tone_index;
    int startTone = (MAX_VOLUME / NUM_OF_TONES) / 10;

    if (argc != 2) {
        printf("Program takes string argument for path to keyboard device");
        return -1;
    }

    // Check the keyboard device arg
    if (argv[1] == NULL) {
        printf("Specify the path to the keyboard device ex. /dev/input/by-id/HP-KEYBOARD");
        return -1;
    }
    keyboard = argv[1];
    
    // catch SIGINT from ctrl+c, instead of having it abruptly close this program
  	signal(SIGINT, catchSIGINT);

    //---------- Keyboard ----------------------
    // Open keyboard device
    if ((fd_keyboard = open (keyboard, O_RDONLY | O_NONBLOCK)) == -1){
        printf ("Could not open %s\n", argv[1]);
        return -1;
    }

    //-------------Audio Port---------------------
    // Create virtual memory access 
    if ((fd_mem = open_physical (fd_mem)) == -1) {
        return -1;
    }

    // Set virtual address pointer to audio port
    if ((LW_virtual = map_physical (fd_mem, LW_BRIDGE_BASE, LW_BRIDGE_SPAN)) == NULL) {
        return -1;
    }
    audio_virtual = (unsigned int *) (LW_virtual + AUDIO_BASE);
    
    // To initialize the audio port you first should 
    // clear the Write FIFO by using the CW bit in the Control register
    setupAudioPort();
    //-------------------------------------
    
    // Spawn the audio thread
    if ((err = pthread_create(&tid, NULL, &audio_thread, NULL)) != 0) {
        printf("pthread_create failed:[%s]\n", strerror(err));
    }
    set_processor_affinity(0);

    printf("Please Press keyboard 2-7 and Q-I:\n");
    while (!stop) 
    {
        // Read keyboard and adjust tone_volume[]
        if (read (fd_keyboard, &ev, event_size) < event_size){
            // No event
            continue;
        }

        //if (ev.type == EV_KEY && ev.value == KEY_PRESSED) {
        //    printf("Pressed key: 0x%04x\n", (int)ev.code);
        //} else if (ev.type == EV_KEY && ev.value == KEY_RELEASED) {
        //    printf("Released key: 0x%04x\n", (int)ev.code);
        //}

        //printf("press: %d\n", ev.value); 
		if (ev.type == EV_KEY && ((ev.value == KEY_RELEASED) || (ev.value == KEY_PRESSED))) {
			key = (int) ev.code;
            tone_index = KeyToTone(key);
		}

        if (ev.value == KEY_PRESSED && -1 != tone_index) {
            pthread_mutex_lock(&mutex_tone_volume);
            tone_volumes[tone_index] = startTone;
            pthread_mutex_unlock(&mutex_tone_volume);
            //printf("tone volume = %d", tone_volumes[tone_index]);
        }

        //printf("tone_index = %d\n",tone_index);
        //tone_fade_factor[tone_index] =

        // When a key is pressed, the main program adjusts the volume of the corresponding tone in the array tone_volume
        // audio thread loops to read the array, and creates a sinusoid for tone_volume each tone that is currently being played
    }

    // Cancel the audio thread.
    pthread_cancel(tid);
    // Wait until audio thread terminates.
    pthread_join(tid, NULL);

    // Clean up
    close(fd_keyboard);
    unmap_physical ((void *)LW_virtual, LW_BRIDGE_SPAN);   // release the physical-memory mapping
    close_physical(fd_mem);

    return 0;
}

void * audio_thread() {
    int i, j, nth_sample;
    int chord;
    int volume;

    set_processor_affinity(1);

    while (1) {
        // Check if this thread has been cancelled.
        // If it was, the thread will halt at this point.
        pthread_testcancel();

        // copy in lock tone_volumes

        // Code for writing to the audio port.

        for (nth_sample = 0; nth_sample < 8000; nth_sample++) {
            chord = 0;
            for (i = 0; i < NUM_OF_TONES_SIZE; ++i) {
                if (i == 2) continue; // Skip the empty tone
                
                pthread_mutex_lock(&mutex_tone_volume);
                volume = tone_volumes[i];
                pthread_mutex_unlock(&mutex_tone_volume);

                chord += (volume * sin(nth_sample * PI2 * tones[i] / SAMPLING_RATE));  //radian = 2*PI*HZ
            }

            if (chord > 0xFFFFFFFF) {
                printf("Too large\n");
            }
            write_to_audio_port(chord);
        }        

        for (i = 0; i < NUM_OF_TONES_SIZE; ++i) {
            if (i == 2) continue; // Skip the empty tone
            if (0 < tone_volumes[i]) {
                // if tone not pressed, decrease volume
                pthread_mutex_lock(&mutex_tone_volume);
                tone_volumes[i] *= 0.5; // tone_fade_factor[j]
                pthread_mutex_unlock(&mutex_tone_volume);
                //printf("i= %d, tone volume = %d\n", i, tone_volumes[i]);
            }
        }
    }
}

void setupAudioPort() {
    volatile unsigned int *fifoSize = (audio_virtual + 1);

    // Set control register
    // RE and WE enable interrupts -> set to zero
    // CW set to 1 to clear the Write FIFO, and remains cleared until set back to 0
    *audio_virtual = 0b1000; // enable write
    *audio_virtual = 0; // clear
    // printf("%x\n", *fifoSize); // Outputs 0x80808080 -> 0x80=128 so cleared
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
