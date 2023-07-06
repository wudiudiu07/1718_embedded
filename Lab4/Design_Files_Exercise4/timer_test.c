#include <stdio.h>

int time_mm = 59; //mins
int time_ss = 59; //second
int time_dd = 99; //hundrendths

void timer() {
    while (time_dd >= 0 || time_ss >= 0 || time_mm >= 0) {
        printf ("%02d:%02d:%02d\n", time_mm, time_ss, time_dd);
        
        if (time_dd > 0) {
            --time_dd;
        }
        else {
            if (time_ss > 0) {
                --time_ss;
            }
            else {
                if (time_mm > 0) {
                   --time_mm;
                }
                time_ss = 59;
            }
            time_dd = 99; // Reset
        }
    }
}

int main()
{
    timer();
    return 0;
}
