#include <stdio.h>
#include <stdlib.h>
#define YELLOW 33
#define CYAN 36
#define WHITE 37

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

int main(void) {
	char c;
    int i;
	int x0, x1, y0, y1;
    char colors[] = {YELLOW, CYAN, WHITE, 31};

    printf ("\e[2J"); 					// clear the screen
  	printf ("\e[?25l");					// hide the cursor

    // Changing Y
    x0 = 1; y0 = 24;
    x1 = 80;
    for (i=0, y1 = 24; y1 >= 1; y1 -= 7, ++i) {
        draw_line(x0, x1, y0, y1, colors[i % 4]);
    }

    // Changing X
    draw_line(1, 40, 24, 1, 32);
    draw_line(1, 20, 24, 1, 34);
    draw_line(1, 1, 24, 1, 35);

	c = getchar ();						// wait for user to press return
	printf ("\e[2J"); 					// clear the screen
	printf ("\e[%2dm", WHITE);			// reset foreground color
	printf ("\e[%d;%dH", 1, 1);		// move cursor to upper left
	printf ("\e[?25h");					// show the cursor
	fflush (stdout);

    return 0;
}

