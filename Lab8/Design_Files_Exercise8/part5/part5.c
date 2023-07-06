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

#define BYTES 256

#define KEY_RELEASED 0
#define KEY_PRESSED 1
static const char *const press_type[2] = {"RELEASED", "PRESSED "};

//ln -s /dev/input/by-id/usb-HTLTEK_Gaming_keyboard-event-kbd
void setupAudioPort();
void write_to_audio_port(int);
void * audio_thread();
void * video_thread();

static const char *Note[] = {"C#", "D#", " ", "F#", "G#", "A#", "C", "D", "E", "F", "G", "A", "B", "C"};
static const char ASCII[] = {'2',  '3',  '4', '5',  '6',  '7',  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I'};
static const double tones[] ={277.183, 311.127, 0, 369.994, 415.305, 466.164, //"C#", "D#", " ", "F#", "G#", "A#"
                261.626, 293.665, 329.628, 349.228, 391.995, 440.000, 493.883, 523.251}; //"C", "D", "E", "F", "G", "A", "B", "C"
#define NUM_OF_TONES 13
#define NUM_OF_TONES_SIZE 14
#define START_TONE (MAX_VOLUME / NUM_OF_TONES)
int tone_volumes[NUM_OF_TONES_SIZE];
pthread_mutex_t mutex_tone_volume; // mutex for main and audio_thread

// Video variables
#define video_BYTES 9 // number of characters to read from /dev/video
int screen_x, screen_y;
int video_FD;

#define BLACK 0x0
#define WHITE 0xFFFF

typedef struct Records {
    int time_mm[100];
    int time_ss[100];
    int time_dd[100];
    int tone_order[100];
    int num_record_key; // Size/Number of entires (keys) recorded
    int time_mm_end;
    int time_ss_end;
    int time_dd_end;
} Record;

volatile unsigned int *audio_virtual;  // used to map physical addresses for the audio port
int recording = 0, playback = 0;
volatile sig_atomic_t stop = 0;
static Record record;

void catchSIGINT(int signum){
    stop = 1;
}

int strLenBytes(char *str) {
    return strlen(str) + 1;
}

// Set the current threads affinity to the core specified
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

// audio thread - to write samples 
// audio thread is similar to the code from Part II, because it has to create, and possibly sum together, sinusoids 
//  corresponding to each tone
// slowly reduces the volume of each tone depending on whether or not the key for this tone is still being pressed

// main thread writes to when a piano key is pressed tone_volume
// audio thread gradually reduces the volume of a tone depending on whether the key is still pressed


void setupStopwatch(int stopwatch_fd) {
    char chardev_buffer[BYTES];	// buffer for chardev character data
	int ret_val;			    // number of characters read from chardev

    sprintf(chardev_buffer, "stop");
    ret_val = write (stopwatch_fd, chardev_buffer, strLenBytes(chardev_buffer));
    if (ret_val <= 0) {
        printf("Writing Start time failed\n");
    }

    sprintf(chardev_buffer, "59:59:99");
    ret_val = write (stopwatch_fd, chardev_buffer, strLenBytes(chardev_buffer));
    if (ret_val <= 0) {
        printf("Writing Start time failed\n");
    }

    sprintf(chardev_buffer, "disp");
    ret_val = write (stopwatch_fd, chardev_buffer, strLenBytes(chardev_buffer));
    if (ret_val <= 0) {
        printf("Writing Start time failed\n");
    }
}

void setupLEDR(int ledr_fd) {
    char chardev_buffer[BYTES];	// buffer for chardev character data
	int ret_val;			    // number of characters read from chardev

    //initial the LEDR
    sprintf(chardev_buffer, "0");
	ret_val = write (ledr_fd, chardev_buffer, strLenBytes(chardev_buffer));
    if (ret_val <= 0) {
        printf("Issue writing to /dev/LEDR\n");
    }    
}

void resetStopWatch(int stopwatch_fd) {
    char chardev_buffer[BYTES];	// buffer for chardev character data
	int ret_val;			    // number of characters read from chardev

    // stop
    sprintf(chardev_buffer, "stop"); 
    ret_val = write (stopwatch_fd, chardev_buffer, strLenBytes(chardev_buffer));
    if (ret_val <= 0) {
        printf("Writing to run failed\n");
    }

    //reset
    sprintf(chardev_buffer, "59:59:99"); 
    ret_val = write (stopwatch_fd, chardev_buffer, strLenBytes(chardev_buffer));
    if (ret_val <= 0) {
        printf("Writing Start time failed\n"); 
    }

    // run
    sprintf(chardev_buffer, "run"); 
    ret_val = write (stopwatch_fd, chardev_buffer, strLenBytes(chardev_buffer));
    if (ret_val <= 0) {
        printf("Writing to run failed\n");
    }
}

void startRecording(int stopwatch_fd, int ledr_fd) {
    char chardev_buffer[BYTES];	// buffer for chardev character data
	int ret_val;			    // number of characters read from chardev
    int i;

    record.num_record_key = 0;        
                
    //write to ledr          
    sprintf(chardev_buffer, "1");
    ret_val = write (ledr_fd, chardev_buffer, strLenBytes(chardev_buffer));
    if (ret_val <= 0) {
        printf("Issue writing to /dev/LEDR\n");
    }

    // Clear past tones (from piano slow sound disapate)
    pthread_mutex_lock(&mutex_tone_volume);
    for (i = 0; i < NUM_OF_TONES_SIZE; ++i) {
        tone_volumes[i] = 0;
    }
    pthread_mutex_unlock(&mutex_tone_volume);

    resetStopWatch(stopwatch_fd);
}

void stopRecording(int stopwatch_fd, int ledr_fd) {
    char chardev_buffer[BYTES];	// buffer for chardev character data
	int ret_val;			    // number of characters read from chardev
    int i;

    // stop
    sprintf(chardev_buffer, "stop"); 
    ret_val = write (stopwatch_fd, chardev_buffer, strLenBytes(chardev_buffer));
    if (ret_val <= 0) {
        printf("Writing to run failed\n");
    }

    //clear to ledr          
    sprintf(chardev_buffer, "0");
    ret_val = write (ledr_fd, chardev_buffer, strLenBytes(chardev_buffer));
    if (ret_val <= 0) {
        printf("Issue writing to /dev/LEDR\n");
    }

    // Clear past tones (from piano slow sound disapate)
    pthread_mutex_lock(&mutex_tone_volume);
    for (i = 0; i < NUM_OF_TONES_SIZE; ++i) {
        tone_volumes[i] = 0;
    }
    pthread_mutex_unlock(&mutex_tone_volume);
}

int main(int argc, char *argv[])
{
    int err, i, j;
    pthread_t tid_audio, tid_video;
    int time_cur, time_check, time_cur_end, time_check_end;
    struct input_event ev;
    int fd_keyboard, event_size = sizeof (struct input_event), key;

    int fd_mem = -1;    // used to open /dev/mem for access to physical addresses
    void *LW_virtual;   // used to map physical addresses for the light-weight bridge

    char *keyboard; //= "/dev/input/by-id/usb-HTLTEK_Gaming_keyboard-event-kbd";
    int tone_index;
    unsigned int time_mm, time_ss, time_dd;

    //driver 
    int stopwatch_fd,key_fd,ledr_fd;  	
    char chardev_buffer[BYTES];	// buffer for chardev character data
	int ret_val;			    // number of characters read from chardev
    unsigned int key_value = 0; // Needs to be unsigned for conversion to work
    int key_index = 0;
    int key_tone = 0;

    char buffer[video_BYTES]; // buffer for video char data
    char command[256]; // buffer for commands written to /dev/video

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

  	signal(SIGINT, catchSIGINT);
    //---------- Key ----------------------
    if ((key_fd = open("/dev/KEY", O_RDWR)) == -1)
	{
		printf("Error opening /dev/KEY: %s\n", strerror(errno));
		return -1;
	}

    // Clear any key presses
    while((ret_val = read (key_fd, chardev_buffer, BYTES)) != 0 );
    //---------- LEDR ----------------------    
	if ((ledr_fd = open("/dev/LEDR", O_RDWR)) == -1)
	{
		printf("Error opening /dev/LEDR: %s\n", strerror(errno));
		return -1;
	}
    setupLEDR(ledr_fd);
    //---------- Stopwatch ----------------------
    if ((stopwatch_fd = open("/dev/stopwatch", O_RDWR)) == -1)
	{
		printf("Error opening /dev/stopwatch: %s\n", strerror(errno));
		return -1;
	}
    setupStopwatch(stopwatch_fd);
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
    
    // clear the Write FIFO by using the CW bit in the Control register
    setupAudioPort();

    //------Video-------------------

    // Open the character device driver
    if ((video_FD = open("/dev/IntelFPGAUP/video", O_RDWR)) == -1)
    {
        printf("Error opening /dev/IntelFPGAUP/video: %s\n", strerror(errno));
        return -1;
    }

    // Get screen_x and screen_y by reading from the driver
    while(read (video_FD, buffer, video_BYTES) != 0);
    sscanf(buffer, "%d %d", &screen_x, &screen_y);
    printf("Screen:%d %d\n", screen_x, screen_y);
    
    // Clear Screen
    sprintf(command, "clear");
    write (video_FD, command, strLenBytes(command));
   
    sprintf (command, "sync");
    write (video_FD, command, strLenBytes(command));

    sprintf(command, "clear");
    write (video_FD, command, strLenBytes(command));

    sprintf (command, "sync");
    write (video_FD, command, strLenBytes(command));
    //------------------------------

    // Spawn the audio thread
    if ((err = pthread_create(&tid_audio, NULL, &audio_thread, NULL)) != 0) {
        printf("pthread_create failed:[%s]\n", strerror(err));
    }
    // Spawn the video thread
    if ((err = pthread_create(&tid_video, NULL, &video_thread, NULL)) != 0) {
        printf("pthread_create video failed:[%s]\n", strerror(err));
    }

    set_processor_affinity(0);

    printf("Please Press keyboard 2-7 and Q-I:\n");
    printf("---   Running Part 5, press <Ctrl-C> to quit   ---\n");
    printf("---   Press KEY0 to start/stop recording       ---\n");
    printf("---   Press KEY1 to play back recording        ---\n");
    while (!stop) 
    {
        //-------------check key value---------------------
        while((ret_val = read (key_fd, chardev_buffer, BYTES)) != 0 );
        sscanf(chardev_buffer, "%01x", &key_value);

        if (0x1 == (key_value & 0x1)) {
            recording = (recording == 0) ? 1 : 0; // flip on key0 press

            // If was already recording, then stoping
            if (recording) {
                printf("Begin Recording\n");
                startRecording(stopwatch_fd, ledr_fd);
            }
            else {
                printf("Stop Recording\n");
                stopRecording(stopwatch_fd, ledr_fd);
                //record the end time
                while((ret_val = read (stopwatch_fd, chardev_buffer, BYTES)) != 0 );
                sscanf(chardev_buffer, "%02d:%02d:%02d\n", &record.time_mm_end, &record.time_ss_end, &record.time_dd_end); //read stopwatch value
                time_check_end = (record.time_mm_end * 60 * 100) + (record.time_ss_end * 100) + record.time_dd_end;
                printf("end time: %d:%d:%d %d\n", record.time_mm_end, record.time_ss_end, record.time_dd_end, time_check_end);
            }
            playback = 0;
        }
        else if (!recording && !playback && 0x2 == (key_value & 0x2)) {
            printf("Start Playback\n");
            playback = 1;
            key_index = 0;
            recording = 0;
            
            // Clear past tones (from piano slow sound disapate)
            pthread_mutex_lock(&mutex_tone_volume);
            for (i = 0; i < NUM_OF_TONES_SIZE; ++i) {
                tone_volumes[i] = 0;
            }
            pthread_mutex_unlock(&mutex_tone_volume);

            // write to ledr
            sprintf(chardev_buffer, "2");
		    ret_val = write (ledr_fd, chardev_buffer, strLenBytes(chardev_buffer));
            if (ret_val <= 0) {
                printf("Issue writing to /dev/LEDR\n");
            }

            // Restart stopwatch for playback
            resetStopWatch(stopwatch_fd);
        }
        // Check if playback finished
        else if (playback && key_index >= record.num_record_key){
            while((ret_val = read (stopwatch_fd, chardev_buffer, BYTES)) != 0 );
            sscanf(chardev_buffer, "%02d:%02d:%02d\n", &time_mm, &time_ss, &time_dd); //read stopwatch value

            if (time_cur <= time_check_end){ //if the recording ends, set volume to 0
                for (j=0; j < NUM_OF_TONES_SIZE; j++){
                    pthread_mutex_lock(&mutex_tone_volume);
                    tone_volumes[j] =  0;
                    pthread_mutex_unlock(&mutex_tone_volume);
                }

                printf("Playback end\n");
                playback = 0; 
                key_index = 0; // Reset

                // clear to ledr
                sprintf(chardev_buffer, "0");
                ret_val = write (ledr_fd, chardev_buffer, strLenBytes(chardev_buffer));
                if (ret_val <= 0) {
                    printf("Issue writing to /dev/LEDR\n");
                }

                setupStopwatch(stopwatch_fd);
            }
        }

        if (playback) {
            while((ret_val = read (stopwatch_fd, chardev_buffer, BYTES)) != 0 );
            sscanf(chardev_buffer, "%02d:%02d:%02d\n", &time_mm, &time_ss, &time_dd); //read stopwatch value
            
            time_cur = (time_mm * 60 * 100) + (time_ss * 100) + time_dd; 
            time_check = (record.time_mm[key_index] * 60 * 100) + (record.time_ss[key_index] * 100) + record.time_dd[key_index];
            
            if (key_index < record.num_record_key && time_cur <= time_check) {
                printf("Playing %d record: %d >= %d\n", key_index, time_cur, time_check);
                key_tone = record.tone_order[key_index];
                pthread_mutex_lock(&mutex_tone_volume);
                tone_volumes[key_tone] = START_TONE;
                ++key_index;
                pthread_mutex_unlock(&mutex_tone_volume);
            }
        } 
        else { 
            //-------------check keyboard---------------------
            // Read keyboard and adjust tone_volume[]
            if (read (fd_keyboard, &ev, event_size) < event_size){
                // No event
                continue;
            }
            if (ev.type == EV_KEY && (/*(ev.value == KEY_RELEASED) ||*/ (ev.value == KEY_PRESSED))) {
                key = (int) ev.code;
                tone_index = KeyToTone(key);
            }

            if (ev.value == KEY_PRESSED && -1 != tone_index) { //not playback, play the current key 

                pthread_mutex_lock(&mutex_tone_volume);
                tone_volumes[tone_index] = START_TONE;
                pthread_mutex_unlock(&mutex_tone_volume);
                //printf("tone volume = %d", tone_volumes[tone_index]);

                if (recording) {
                    //read from stopwatch driver
                    while((ret_val = read (stopwatch_fd, chardev_buffer, BYTES)) != 0 );
                    sscanf(chardev_buffer, "%02d:%02d:%02d\n", &time_mm, &time_ss, &time_dd);
                    
                    //write current stopwactch value to *time and the tone_index to tone_order
                    record.time_mm[record.num_record_key] = time_mm;
                    record.time_ss[record.num_record_key] = time_ss;
                    record.time_dd[record.num_record_key] = time_dd;
                    record.tone_order[record.num_record_key] = tone_index;
                    // TODO: maybe add volume
                    printf("Added %d record\n", record.num_record_key);
                    ++record.num_record_key; //the number of recorded key ++
                }   
            }
        }
    }

    // Cancel threads.
    pthread_cancel(tid_video);
    pthread_cancel(tid_audio);
    // Wait until threads terminates.
    pthread_join(tid_video, NULL);
    pthread_join(tid_audio, NULL);

    // Clean VGA
    sprintf(command, "clear");
    write (video_FD, command, strLenBytes(command));
   
    sprintf (command, "sync");
    write (video_FD, command, strLenBytes(command));

    sprintf(command, "clear");
    write (video_FD, command, strLenBytes(command));

    sprintf (command, "sync");
    write (video_FD, command, strLenBytes(command));
   
    close (video_FD);

    // Clean up Stopwatch
    sprintf(chardev_buffer, "nodisp");
    ret_val = write (stopwatch_fd, chardev_buffer, strLenBytes(chardev_buffer));
    if (ret_val <= 0) {
        printf("Writing Start time failed\n");
    }
    sprintf(chardev_buffer, "0");
	ret_val = write (ledr_fd, chardev_buffer, strLenBytes(chardev_buffer));
    if (ret_val <= 0) {
        printf("Issue writing to /dev/LEDR\n");
    }    

    // Clean up
    close(fd_keyboard);
    unmap_physical ((void *)LW_virtual, LW_BRIDGE_SPAN);   // release the physical-memory mapping
    close_physical(fd_mem);

    return 0;
}

void * video_thread() {
    int i, j, cycle, sample;
    double chord;
    int volume;
    char command[256]; // buffer for commands written to /dev/video
    int center_screen = screen_y/2;
    set_processor_affinity(0);

    while (1) {
        // Check if this thread has been cancelled.
        // If it was, the thread will halt at this point.
        pthread_testcancel();

        sprintf(command, "clear");
        write (video_FD, command, strLenBytes(command));
        int x_prev=0, y_prev=center_screen, x_cur, y_cur;

        for (sample = 0; sample < screen_x; ++sample) {
            x_cur = sample;

            chord = 0;
            for (i = 0; i < NUM_OF_TONES_SIZE; ++i) {
                if (i == 2) continue; // Skip the empty tone

                pthread_mutex_lock(&mutex_tone_volume);
                volume = tone_volumes[i];
                pthread_mutex_unlock(&mutex_tone_volume);
                
                chord += volume * (sin(sample * PI2 * tones[i] / SAMPLING_RATE));
            }
            y_cur =  (int)(center_screen + (chord / MAX_VOLUME) * screen_y);

            sprintf (command, "line %d,%d %d,%d %4x", x_prev, y_prev, x_cur, y_cur,
                WHITE & 0xFFFF);
            write (video_FD, command, strLenBytes(command));
            x_prev = x_cur;
            y_prev = y_cur;
        }

        // Push drawn things to screen
        sprintf (command, "sync");
        write (video_FD, command, strLenBytes(command));
    }
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

            pthread_mutex_lock(&mutex_tone_volume);
            if (0 < tone_volumes[i]) {
                // if tone not pressed, decrease volume
                tone_volumes[i] *= 0.5;
            }
            pthread_mutex_unlock(&mutex_tone_volume);
        }
    }
}

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