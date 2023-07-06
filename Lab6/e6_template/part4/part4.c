#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define video_BYTES 9 // number of characters to read from /dev/video
#define NUM_BOXES 8
#define BOX_SIZE 3
#define NUM_SCREEN_BUFFERS 3

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

    sprintf (command, "box %d,%d %d,%d %x", 
        box->x1, box->y1, box->x2, box->y2, color);
    write (video_FD, command, strLenBytes(command));
}

void draw_line(int video_FD, Box *box1, Box *box2, short color)
{
    char command[64]; // buffer for commands written to /dev/video

    sprintf (command, "line %d,%d %d,%d %x", 
        AvgX(box1), AvgY(box1), AvgX(box2), AvgY(box2), color);
    //printf("cmd: %s\n", command);
    write (video_FD, command, strLenBytes(command));
}

void draw(int video_FD, Box *boxes, int clear) {
    int i;
    short color=BLACK;

    // draw line between boxes
    for (i=0; i < NUM_BOXES - 1; ++i) {
        color = (clear) ? BLACK : colors[i % NUM_COLORS];
        draw_line(video_FD, &(boxes[i]), &(boxes[i + 1]), color);
    }
    // draw line betweem the first and the last objects 
    color = (clear) ? BLACK : colors[i % NUM_COLORS];
    draw_line(video_FD, &(boxes[i]), &(boxes[0]), color);

    // Draw boxes
    for (i=0; i < NUM_BOXES; ++i) {
        color = (clear) ? BLACK : colors[i % NUM_COLORS];
        plot_box(video_FD, &(boxes[i]), color);
    }
}

void RotateCache(Box **boxes_cur, Box **boxes_prev1, Box **boxes_prev2) {
    Box *tmp = *boxes_prev2;
    *boxes_prev2 = *boxes_prev1;
    *boxes_prev1 = *boxes_cur;
    memcpy(tmp, *boxes_cur, sizeof(Box) * NUM_BOXES);
    *boxes_cur = tmp;
}

int main(int argc, char *argv[]){
    int i;
    int video_FD; // file descriptor
    char buffer[video_BYTES]; // buffer for video char data
    char command[64]; // buffer for commands written to /dev/video
    Box boxes[NUM_SCREEN_BUFFERS][NUM_BOXES];
    Box *boxes_cur = &(boxes[0][0]);
    Box *boxes_prev1 = &(boxes[1][0]);
    Box *boxes_prev2 = &(boxes[2][0]);

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

    // Get screen_x and screen_y by reading from the driver
    while(read (video_FD, buffer, video_BYTES) != 0);
    sscanf(buffer, "%d %d", &screen_x, &screen_y);

    // Generate Initial positions for boxes
    for (i=0; i < NUM_BOXES; ++i) {
        boxes_cur[i].x1 = rand() % (screen_x - BOX_SIZE);
        boxes_cur[i].x2 = boxes_cur[i].x1 + BOX_SIZE;
        
        boxes_cur[i].y1 = rand() % (screen_y - BOX_SIZE);
        boxes_cur[i].y2 = boxes_cur[i].y1 + BOX_SIZE;

        boxes_cur[i].d = rand() % NUM_DIRECTION;
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
        // Clear Drawn things from past
        draw(video_FD, boxes_prev2, 1);

        // Draw new things
        draw(video_FD, boxes_cur, 0);

        // Push drawn things to screen
        sprintf (command, "sync");
        write (video_FD, command, strLenBytes(command));

        // Backup
        RotateCache(&boxes_cur, &boxes_prev1, &boxes_prev2);

        // Update positions
        for (i=0; i < NUM_BOXES; ++i) {
            int *x1 = &(boxes_cur[i].x1);
            int *x2 = &(boxes_cur[i].x2);
            int *y1 = &(boxes_cur[i].y1);
            int *y2 = &(boxes_cur[i].y2);
            Direction *d = &(boxes_cur[i].d);

            // Update directions
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

    sprintf(command, "clear");
    write (video_FD, command, strLenBytes(command));
   
    sprintf (command, "sync");
    write (video_FD, command, strLenBytes(command));

    sprintf(command, "clear");
    write (video_FD, command, strLenBytes(command));

    sprintf (command, "sync");
    write (video_FD, command, strLenBytes(command));

    close (video_FD);
    return 0;
}