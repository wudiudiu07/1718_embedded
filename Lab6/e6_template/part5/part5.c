#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define video_BYTES 9 // number of characters to read from /dev/video
#define BOX_SIZE 3
#define NUM_SCREEN_BUFFERS 3
#define MAX_NUM_BOXES 256

static int NUM_BOXES_CURR = 8;
static int NUM_BOXES_PREV1 = 8;
static int NUM_BOXES_PREV2 = 8;
static int BOX_SCREEN_UPDATE_AMOUNT = 1;
static int BOX_SCREEN_UPDATE_AMOUNT_DELAY = 50000000;
static int display_lines; // Bool to show lines
static struct timespec ts; // Speed

#define BLACK 0x0
#define RED 0xF800
#define GREEN 0x7E0
#define BLUE 0x1F
#define YELLOW (GREEN | BLUE)
#define PURPLE 0x551F
#define CYAN 0x551F
#define WHITE 0xFFFF

#define NUM_COLORS 5
static short colors[NUM_COLORS] = {WHITE, RED, YELLOW, GREEN, 0x551F};

volatile sig_atomic_t stop = 0;
void catchSIGINT(int signum){
    stop = 1;
}

int screen_x, screen_y;

typedef enum direction {
    UP_LEFT=0,
    UP_RIGHT,
    DOWN_LEFT,
    DOWN_RIGHT,
    NUM_DIRECTION
} Direction;

typedef struct Box {
    int x1, y1, x2, y2;
    Direction d;
} Box;

int AvgX(Box *box) {
    return (box->x1 + box->x2) / 2;
}

int AvgY(Box *box) {
    return (box->y1 + box->y2) / 2;
}

int strLenBytes(char *str) {
    return strlen(str) + 1;
}

void plot_box(int video_FD, Box *box, short color)
{
    char command[64]; // buffer for commands written to /dev/video

    sprintf (command, "box %d,%d %d,%d %4x", 
        box->x1, box->y1, box->x2, box->y2, color & 0xFFFF);
    write (video_FD, command, strLenBytes(command));
}

void draw_line(int video_FD, Box *box1, Box *box2, short color)
{
    char command[64]; // buffer for commands written to /dev/video

    sprintf (command, "line %d,%d %d,%d %4x", 
        AvgX(box1), AvgY(box1), AvgX(box2), AvgY(box2), color & 0xFFFF);
    //printf("cmd: %s\n", command);
    write (video_FD, command, strLenBytes(command));
}

void draw(int video_FD, Box *boxes, int num_boxes, int clear) {
    int i;
    short color=BLACK;

    if (display_lines || clear) {
        // draw line between boxes
        for (i=0; i < num_boxes - 1; ++i) {
            color = (clear) ? BLACK : colors[i % NUM_COLORS];
            draw_line(video_FD, &(boxes[i]), &(boxes[i + 1]), color);
        }
        // draw line betweem the first and the last objects 
        color = (clear) ? BLACK : colors[i % NUM_COLORS];
        draw_line(video_FD, &(boxes[i]), &(boxes[0]), color);
    }

    // Draw boxes
    for (i=0; i < num_boxes; ++i) {
        color = (clear) ? BLACK : colors[i % NUM_COLORS];
        plot_box(video_FD, &(boxes[i]), color);
    }
}

void RotateCache(Box **boxes_cur, Box **boxes_prev1, Box **boxes_prev2) {
    Box *tmp = *boxes_prev2;
    
    *boxes_prev2 = *boxes_prev1;
    NUM_BOXES_PREV2 = NUM_BOXES_PREV1;

    *boxes_prev1 = *boxes_cur;
    NUM_BOXES_PREV1 = NUM_BOXES_CURR;

    memcpy(tmp, *boxes_cur, sizeof(Box) * NUM_BOXES_CURR);
    *boxes_cur = tmp;
}

void convertCountToTime(int change) {
    ts.tv_nsec += change;
    if (change > 0) { //positive
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_nsec = 0;
            ts.tv_sec += 1;
        }
    }
    else { // negative
        if (ts.tv_nsec < 0) { 
            if (ts.tv_sec > 0) {
                ts.tv_nsec = 1000000000 + change;
                ts.tv_sec -= 1;
            }
            else {
                ts.tv_nsec = 0;
            }
        }
    }
}

void initBox(Box *box) {
    box->x1 = rand() % (screen_x - BOX_SIZE);
    box->x2 = box->x1 + BOX_SIZE;
    box->y1 = rand() % (screen_y - BOX_SIZE);
    box->y2 = box->y1 + BOX_SIZE;
    box->d = rand() % NUM_DIRECTION;
}

void event(unsigned int key, Box *boxes_cur) 
{
    switch (key)
    {
    case 0: //SW
        display_lines = 0;
        break;
    case 1:
        // Speed up
        if (ts.tv_sec > 0 || ts.tv_nsec > 0) {
            convertCountToTime(-1 * BOX_SCREEN_UPDATE_AMOUNT_DELAY);
        }
        else {
            BOX_SCREEN_UPDATE_AMOUNT += 1;
        }
        //printf("Sec:%ld NSec:%ld | Dist:%d\n", ts.tv_sec, ts.tv_nsec, BOX_SCREEN_UPDATE_AMOUNT);
        break;
    case 2:
        if (BOX_SCREEN_UPDATE_AMOUNT > 1) {
            --BOX_SCREEN_UPDATE_AMOUNT;
        }
        else {
            convertCountToTime(BOX_SCREEN_UPDATE_AMOUNT_DELAY);
        }
        //printf("Sec:%ld NSec:%ld | Dist:%d\n", ts.tv_sec, ts.tv_nsec, BOX_SCREEN_UPDATE_AMOUNT);
        break;
    case 3://SW
        display_lines = 1;
        break;
    case 4:
        ++NUM_BOXES_CURR;
        
        if (NUM_BOXES_CURR > MAX_NUM_BOXES) {
            NUM_BOXES_CURR = MAX_NUM_BOXES;
        }
        else {
            initBox(&(boxes_cur[NUM_BOXES_CURR-1]));
        }
        break;
    case 8:
        --NUM_BOXES_CURR;
        if (NUM_BOXES_CURR <= 0) {
            NUM_BOXES_CURR = 1;
        }
        break;
    }
}

int main(int argc, char *argv[]){
    int i, j;
    int video_FD, key_fd, sw_fd; // file descriptor
    char buffer[video_BYTES]; // buffer for video char data
    char command[256]; // buffer for commands written to /dev/video
    char chardev_buffer[256]; // buffer for reading from drivers
    Box boxes[NUM_SCREEN_BUFFERS][MAX_NUM_BOXES];
    Box *boxes_cur = &(boxes[0][0]);
    Box *boxes_prev1 = &(boxes[1][0]);
    Box *boxes_prev2 = &(boxes[2][0]);
    unsigned int key_value;
    unsigned int sw_value;

    // Setup scroll loop delay
    ts.tv_sec = 0; ts.tv_nsec = 0;
    display_lines = 1;

    // Initialize memory
    memset(boxes, 0, sizeof(boxes));

  	// catch SIGINT from ctrl+c, instead of having it abruptly close this program
  	signal(SIGINT, catchSIGINT);

    // Initialize random seed
    srand(time(NULL));

    // Open the character device driver
    if ((video_FD = open("/dev/video", O_RDWR)) == -1)
    {
        printf("Error opening /dev/video: %s\n", strerror(errno));
        return -1;
    }

    // Open the character device driver for read
	if ((key_fd = open("/dev/IntelFPGAUP/KEY", O_RDWR)) == -1)
	{
		printf("Error opening /dev/IntelFPGAUP/KEY: %s\n", strerror(errno));
		return -1;
	}

    if ((sw_fd = open("/dev/IntelFPGAUP/SW", O_RDWR)) == -1)
	{
		printf("Error opening /dev/IntelFPGAUP/SW: %s\n", strerror(errno));
		return -1;
	}

    // Get screen_x and screen_y by reading from the driver
    while(read (video_FD, buffer, video_BYTES) != 0);
    sscanf(buffer, "%d %d", &screen_x, &screen_y);

    // Generate Initial positions for boxes
    for (i=0; i < NUM_BOXES_CURR; ++i) {
        initBox(&(boxes_cur[i]));
    }

    // Clear Screen
    sprintf(command, "clear");
    write (video_FD, command, strLenBytes(command));
   
    sprintf (command, "sync");
    write (video_FD, command, strLenBytes(command));

    sprintf(command, "clear");
    write (video_FD, command, strLenBytes(command));

    sprintf (command, "sync");
    write (video_FD, command, strLenBytes(command));

    while (!stop) {
        while(read (key_fd, chardev_buffer, sizeof(chardev_buffer)) != 0 )
        sscanf(chardev_buffer, "%01x", &key_value);
        if (key_value != 0){
            event(key_value, boxes_cur);
        }

        // Read SW
        while(read (sw_fd, chardev_buffer, sizeof(chardev_buffer)) != 0 );
        sscanf(chardev_buffer, "%03x", &sw_value);
        if (sw_value == 0) {
            event(3, boxes_cur);
        }
        else { 
            event(0, boxes_cur);
        }

        // Clear Drawn things from past
        draw(video_FD, boxes_prev2, NUM_BOXES_PREV2, 1);

        // Draw new things
        draw(video_FD, boxes_cur, NUM_BOXES_CURR, 0);

        // Push drawn things to screen
        sprintf (command, "sync");
        write (video_FD, command, strLenBytes(command));

        // Add delay
        nanosleep (&ts, NULL);
        sleep(ts.tv_sec);

        // Backup
        RotateCache(&boxes_cur, &boxes_prev1, &boxes_prev2);

        // Update positions
        for (i=0; i < NUM_BOXES_CURR; ++i) {
            int *x1 = &(boxes_cur[i].x1);
            int *x2 = &(boxes_cur[i].x2);
            int *y1 = &(boxes_cur[i].y1);
            int *y2 = &(boxes_cur[i].y2);
            Direction *d = &(boxes_cur[i].d);

            // Update directions
            for (j=0; j < BOX_SCREEN_UPDATE_AMOUNT; ++j) {
                if (*x1 <= 0) {
                    *d = (*d == UP_LEFT) ? UP_RIGHT : DOWN_RIGHT;                
                }
                else if (*x2 >= (screen_x - 1)) {
                    *d = (*d == UP_RIGHT) ? UP_LEFT : DOWN_LEFT;                
                }
                else if (*y1 <= 0) {
                    *d = (*d == UP_LEFT) ? DOWN_LEFT : DOWN_RIGHT;                
                }
                else if (*y2 >= (screen_y - 1)) {
                    *d = (*d == DOWN_LEFT) ? UP_LEFT : UP_RIGHT;                
                }

                switch (*d)
                {
                case UP_LEFT:
                    *x1 -= 1;*x2 -= 1;
                    *y1 -= 1;*y2 -= 1;
                    break;
                case UP_RIGHT:
                    *x1 += 1;*x2 += 1;
                    *y1 -= 1;*y2 -= 1;
                    break;
                case DOWN_LEFT:
                    *x1 -= 1;*x2 -= 1;
                    *y1 += 1;*y2 += 1;
                    break;
                case DOWN_RIGHT:
                    *x1 += 1;*x2 += 1;
                    *y1 += 1;*y2 += 1;
                    break;
                default:
                    break;
                }
            }
        }
    }

    sprintf(command, "clear");
    write (video_FD, command, strLenBytes(command));
   
    sprintf (command, "sync");
    write (video_FD, command, strLenBytes(command));

    sprintf(command, "clear");
    write (video_FD, command, strLenBytes(command));

    sprintf (command, "sync");
    write (video_FD, command, strLenBytes(command));

    close(sw_fd);
    close(key_fd);
    close (video_FD);
    return 0;
}