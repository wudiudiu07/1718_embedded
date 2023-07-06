#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

// write a user-level program called part4.c

#define BYTES 256				// max number of characters to read from /dev/chardev

volatile sig_atomic_t stop;
void catchSIGINT(int signum){
    stop = 1;
}

int StringToHex(char *hexString) {
    int value = 0;

    int i = 0;
    for (i = 0; i < strlen(hexString); ++i) {
        unsigned int tmp_val = 0;
       
        char c = hexString[i];
        if (c >= '0' && c <= '9') {
            tmp_val = c - '0';
        }
        else if (c >= 'A' && c <= 'F') {
            tmp_val = c - 'A' + 10;
        }
        else if (c >= 'a' && c <= 'f') {
            tmp_val = c - 'a' + 10;
        }
        else {
            printf("Invalue Argument, should be HEX string\n");
            return -1;
        } 
        value |= (tmp_val << ( (strlen(hexString) - 1 - i) * 4));
    }

    return value;
}


int main(int argc, char *argv[]) {

	int key_fd, sw_fd;          // file descriptor - for reading
    int ledr_fd, hex_fd;        // file descriptor - for writing
  	char chardev_buffer[BYTES];			// buffer for chardev character data
  	//char convertBuf[BYTES];
	int ret_val,chars_read,key_value,sw_value;			// number of characters read from chardev
    char new_msg[BYTES];
    unsigned int runningCount = 0;

  	// catch SIGINT from ctrl+c, instead of having it abruptly close this program
  	signal(SIGINT, catchSIGINT);
    
    // open the files /dev/KEY and /dev/SW for reading
    // open the files /dev/LEDR and /dev/HEX for writing

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
    
    // Open the character device driver for write
	if ((ledr_fd = open("/dev/LEDR", O_RDWR)) == -1)
	{
		printf("Error opening /dev/LEDR: %s\n", strerror(errno));
		return -1;
	}

    if ((hex_fd = open("/dev/HEX", O_RDWR)) == -1)
	{
		printf("Error opening /dev/HEX: %s\n", strerror(errno));
		return -1;
	}

    // Make a loop
    // Whenever a KEY is pressed capture the values of the SW switches
    // Display these values on the LEDR lights 
    // Also keep a running accumulation of the SW values that have been read, and 
    //  show this sum on the HEX displays, as a decimal value

  	while (!stop)
	{
        chars_read = 0;
        // Read KEY
		while((ret_val = read (key_fd, chardev_buffer, BYTES)) != 0 )
            chars_read += ret_val;
        chardev_buffer[chars_read] = '\0';
        
        
        /*if (ret_val < 0) {
            printf("Did not read anything from /dev/KEY\n");
            break; // Shouldn't happen
        }*/
		//printf ("Key: %s\n", chardev_buffer);

        // Convert Key to HEX
        key_value = strtol(chardev_buffer, NULL, 16);
        //clean buffer
        memset(chardev_buffer, 0, BYTES);
        chars_read = 0;
        // Check if key pressed
        if (key_value <= 0) {
            continue;
        }
        //printf ("Key_value: %d\n", key_value);

        // Read SW
        while((ret_val = read (sw_fd, chardev_buffer, BYTES)) != 0 ){
            chars_read += ret_val;
        }
        chardev_buffer[chars_read] = '\0';
        
        sw_value = strtol(chardev_buffer, NULL, 16);

        //clean buffer
        chars_read = 0;

        /*if (ret_val < 0) {
            printf("Did not read anything from /dev/SW\n");
        }*/
		//printf ("SW: %d\n", sw_value);
        
        sprintf(new_msg,"%x",sw_value);

        //printf ("new msg: %s\n", new_msg);

        // Write SW to LEDR

		write (ledr_fd, new_msg, strlen(new_msg)+1);
        if (ret_val < 0) {
            printf("Issue writing to /dev/LEDR\n");
        }

        // Update Running Count
        ret_val = strtol(chardev_buffer, NULL, 16);
        runningCount += ret_val;
        

        //clean buffer
        memset(chardev_buffer, 0, BYTES);

        // Write Running Count to HEX
        sprintf(new_msg, "%x", runningCount);
        printf ("sum value_2: %s\n", new_msg);
		write (hex_fd, new_msg, strlen(new_msg));
        if (ret_val < 0) {
            printf("Issue writing to /dev/HEX\n");
        }

		sleep (1);
	}

	close (hex_fd);
    close (ledr_fd);
    close (sw_fd);
    close (key_fd);
    
   return 0;
}