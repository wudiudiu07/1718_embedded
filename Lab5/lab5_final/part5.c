#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
//#include <pthrd.h> //TODO: remove when using HW as need to swap with HW
#include <fcntl.h>
#include <unistd.h>


#define X_MIN 1
#define X_MAX 80
#define Y_MIN 1
#define Y_MAX 24

#define RED 31
#define GREEN 32
#define YELLOW 33
#define BLUE 34
#define MAGENTA 35
#define CYAN 36
#define WHITE 37

#define BYTES 256				// max number of characters to read from /dev/chardev

typedef enum direction {
    UP_LEFT,
    UP_RIGHT,
    DOWN_LEFT,
    DOWN_RIGHT,
    NUM_DIRECTION
} Direction;

typedef struct Object {
    int x, y;
    Direction d;
    int num;
} Object;


static char colors[] = {WHITE, RED, YELLOW, GREEN, CYAN, MAGENTA, BLUE};
static char symbols[] = {'A','B','C','D','E','F','G'};
static int num_objects = 2;
static Object objs[256]; //TODO: Make dynamic memory 

static struct timespec ts; // Speed
static int display_lines; // Bool to show lines

// used to exit the program cleanly
volatile sig_atomic_t stop;
void catchSIGINT(int);

void swap(int *a, int *b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

void plot_pixel(int x, int y, char color, char c)
{
  	printf ("\e[%2dm\e[%d;%dH%c", color, y, x, c);
  	fflush (stdout);
}

void draw_line(int x0, int x1, int y0, int y1, char color) {
    int deltax, deltay;
    int error;
    int y, y_step;
    int is_steep = (abs(y1 - y0) > abs(x1 - x0));
    int x;
    
    if (is_steep) {
        swap(&x0, &y0);
        swap(&x1, &y1);    
    }
    if (x0 > x1) {
        swap(&x0, &x1);
        swap(&y0, &y1);
    }

    deltax = x1 - x0;
    deltay = abs(y1 - y0);
    error = -(deltax / 2);
    y = y0;
    y_step = (y0 < y1) ? 1 : -1;

    for (x=x0; x <= x1; ++x) {
        if (is_steep) {
            plot_pixel(y,x, color, '*');
        }
        else {
            plot_pixel(x,y, color, '*');
        }
        error = error + deltay;
        if (error >= 0) {
            y = y + y_step;
            error = error - deltax;
        }
    }
}

void event(key) {

        switch (key)
        {
        case 0: //SW
            display_lines = 0;
            break;
        case 1:
            ts.tv_nsec -= 25000000;
            break;
        case 2:
            ts.tv_nsec += 25000000;
            break;
        case 3://SW
            display_lines = 1;
            break;
        case 4:
            num_objects ++;
            
            if (num_objects > 50) {
                num_objects = 50;
            }
            else {
                objs[num_objects-1].x = rand() % X_MAX;
                objs[num_objects-1].y = rand() % Y_MAX;
                objs[num_objects-1].d = rand() % NUM_DIRECTION;
                objs[num_objects-1].num = 0;
            }
            break;
        case 8:
            num_objects--;
            if (num_objects <= 0) {
                num_objects = 1;
            }
            break;
        }

        nanosleep (&ts, NULL);

    return NULL;
}

int main(void) {
    int i;
    int key_fd, sw_fd;          // file descriptor - for reading
    //pthread_t tid; // TODO: remove when using HW
    int ret_val;			      // number of characters read from chardev
    unsigned int key_value;
    unsigned int sw_value;
    char chardev_buffer[BYTES];			// buffer for chardev character data

    // Setup scroll loop delay
    ts.tv_sec = 0;										// used to delay
	ts.tv_nsec = 100000000;							// 10^8 ns = 0.1 sec

    printf ("\e[2J"); 					// clear the screen
  	printf ("\e[?25l");					// hide the cursor
	// catch SIGINT from ctrl+c, instead of having it abruptly close this program
    signal(SIGINT, catchSIGINT);
    // Initialize random seed
    srand(time(NULL));

    // Open the character device driver for read
	if ((key_fd = open("/dev/KEY", O_RDWR)) == -1)
	{
		printf("Error opening /dev/KEY: %s\n", strerror(errno));
		return -1;
	}

    if ((sw_fd = open("/dev/SW", O_RDWR)) == -1)
	{
		printf("Error opening /dev/SW: %s\n", strerror(errno));
		return -1;
	}

    // Generate Initial positions for objects
    for (i=0; i < num_objects; ++i) {
        objs[i].x = rand() % X_MAX;
        objs[i].y = rand() % Y_MAX;
        objs[i].d = rand() % NUM_DIRECTION;
        objs[i].num = 0;
    }

    display_lines = 1;
    //pthread_create(&tid, NULL, inputThread, NULL); 

    while (!stop) {
        // Read KEY
        
		while((ret_val = read (key_fd, chardev_buffer, BYTES)) != 0 )
        // Convert Key to HEX
        sscanf(chardev_buffer, "%02d", &key_value);
        
        // Read SW
        while((ret_val = read (sw_fd, chardev_buffer, BYTES)) != 0 );
        sscanf(chardev_buffer, "%03d", &sw_value);

        
        if (sw_value == 0) 
            event(0);
        else 
            event(3);
        if (key_value != 0){
            event(key_value);
        }
        for (i=0; i < num_objects && display_lines; ++i) {
            
            if (0 < i && num_objects <= 2) {
                break;
            }
            if (i < num_objects - 1) {
                draw_line(objs[i].x, objs[i + 1].x, objs[i].y, objs[i + 1].y, colors[i % 7]);
            }
            else {
                draw_line(objs[i].x, objs[0].x, objs[i].y, objs[0].y, colors[i % 7]);
            }
        }
        for (i=0; i < num_objects; ++i) {
            plot_pixel(objs[i].x, objs[i].y, colors[i % 7], symbols[objs[i].num % 7]);
        }
        nanosleep (&ts, NULL);

        // Update positions
        for (i=0; i < num_objects; ++i) {
            int *x = &objs[i].x;
            int *y = &objs[i].y;
            Direction *d = &objs[i].d;

            // Update directions
            if (*x <= X_MIN) {
                *d = (*d == UP_LEFT) ? UP_RIGHT : DOWN_RIGHT;  
                objs[i].num = 7 ? objs[i].num+1 : 0;             
            }
            else if (*x >= X_MAX) {
                *d = (*d == UP_RIGHT) ? UP_LEFT : DOWN_LEFT;  
                objs[i].num = 7 ? objs[i].num+1 : 0;                 
            }
            else if (*y <= Y_MIN) {
                *d = (*d == UP_LEFT) ? DOWN_LEFT : DOWN_RIGHT;    
                objs[i].num = 7 ? objs[i].num+1 : 0;               
            }
            else if (*y >= Y_MAX) {
                *d = (*d == DOWN_LEFT) ? UP_LEFT : UP_RIGHT;
                objs[i].num = 7 ? objs[i].num+1 : 0;                 
            }

            switch (*d)
            {
            case UP_LEFT:
                *x -= 1;
                *y -= 1;
                break;
            case UP_RIGHT:
                *x += 1;
                *y -= 1;
                break;
            case DOWN_LEFT:
                *x -= 1;
                *y += 1;
                break;
            case DOWN_RIGHT:
                *x += 1;
                *y += 1;
                break;
            default:
                break;
            }
        }

	    printf ("\e[2J"); 					// clear the screen
    }

    // cancel the input thread
    //pthread_cancel(tid); 

	printf ("\e[2J"); 			// clear the screen
	printf ("\e[%2dm", WHITE);	// reset foreground color
	printf ("\e[%d;%dH", 1, 1); // move cursor to upper left
	printf ("\e[?25h");			// show the cursor
	fflush (stdout);


    close (sw_fd);
    close (key_fd);

    return 0;
}

/* Function to allow clean exit of the program */
void catchSIGINT(int signum)
{
	stop = 1;
}
