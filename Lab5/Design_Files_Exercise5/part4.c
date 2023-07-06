#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

#define NUM_OBJECTS 5

#define X_MIN 1
#define X_MAX 175
#define Y_MIN 1
#define Y_MAX 42

#define RED 31
#define GREEN 32
#define YELLOW 33
#define BLUE 34
#define MAGENTA 35
#define CYAN 36
#define WHITE 37

static char colors[] = {WHITE, RED, YELLOW, GREEN, CYAN, MAGENTA, BLUE};

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
} Object;

int main(void) {
    int i;
    struct timespec ts;
    Object objs[NUM_OBJECTS];

    // Setup scroll loop delay
    ts.tv_sec = 0;										// used to delay
	ts.tv_nsec = 100000000;							// 10^8 ns = 0.1 sec

    printf ("\e[2J"); 					// clear the screen
  	printf ("\e[?25l");					// hide the cursor

	// catch SIGINT from ctrl+c, instead of having it abruptly close this program
    signal(SIGINT, catchSIGINT);

    // Initialize random seed
    srand(time(NULL));

    // Generate Initial positions for objects
    for (i=0; i < NUM_OBJECTS; ++i) {
        objs[i].x = rand() % X_MAX;
        objs[i].y = rand() % Y_MAX;
        objs[i].d = rand() % NUM_DIRECTION;
    }

    while (!stop) {
        for (i=0; i < NUM_OBJECTS; ++i) {
            if (0 < i && NUM_OBJECTS <= 2) {
                break;
            }

            if (i < NUM_OBJECTS - 1) {
                draw_line(objs[i].x, objs[i + 1].x, objs[i].y, objs[i + 1].y, colors[i % 7]);
            }
            else {
                draw_line(objs[i].x, objs[0].x, objs[i].y, objs[0].y, colors[i % 7]);
            }
        }
        for (i=0; i < NUM_OBJECTS; ++i) {
            plot_pixel(objs[i].x, objs[i].y, colors[i % 7], 'X');
        }
        nanosleep (&ts, NULL);

        // Update positions
        for (i=0; i < NUM_OBJECTS; ++i) {
            int *x = &objs[i].x;
            int *y = &objs[i].y;
            Direction *d = &objs[i].d;

            // Update directions
            if (*x <= X_MIN) {
                *d = (*d == UP_LEFT) ? UP_RIGHT : DOWN_RIGHT;                
            }
            else if (*x >= X_MAX) {
                *d = (*d == UP_RIGHT) ? UP_LEFT : DOWN_LEFT;                
            }
            else if (*y <= Y_MIN) {
                *d = (*d == UP_LEFT) ? DOWN_LEFT : DOWN_RIGHT;                
            }
            else if (*y >= Y_MAX) {
                *d = (*d == DOWN_LEFT) ? UP_LEFT : UP_RIGHT;                
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

	printf ("\e[2J"); 			// clear the screen
	printf ("\e[%2dm", WHITE);	// reset foreground color
	printf ("\e[%d;%dH", 1, 1); // move cursor to upper left
	printf ("\e[?25h");			// show the cursor
	fflush (stdout);

    return 0;
}

/* Function to allow clean exit of the program */
void catchSIGINT(int signum)
{
	stop = 1;
}

