#include <stdio.h>
#include <stdint.h>
#include <pw-ir-linux.h>

#include <string.h>

#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

int main(int argc, char** argv){

    int fd = open("/dev/ttyUSB1", O_RDWR);

    while(10)
        serial_test(fd);

    close(fd);

	return 0;
}

int serial_test(int fd) {

    //int fd = open("/dev/ttyACM0", O_RDWR);

    struct termios tty;

    if(tcgetattr(fd, &tty) != 0) {
        printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
        return 1;
    }

    tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
    tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
    tty.c_cflag &= ~CSIZE; // Clear all bits that set the data size
    tty.c_cflag |= CS8; // 8 bits per byte (most common)
    //tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
    tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO; // Disable echo
    tty.c_lflag &= ~ECHOE; // Disable erasure
    tty.c_lflag &= ~ECHONL; // Disable new-line echo
    tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
    // tty.c_oflag &= ~OXTABS; // Prevent conversion of tabs to spaces (NOT PRESENT ON LINUX)
    // tty.c_oflag &= ~ONOEOT; // Prevent removal of C-d chars (0x004) in output (NOT PRESENT ON LINUX)

    tty.c_cc[VTIME] = 10;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
    tty.c_cc[VMIN] = 0;

    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
      printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
      return 1;
    }

    char msg[] = "Hello";
    write(fd, msg, sizeof(msg));

    char read_buf[256];
    memset(read_buf, 0, 256);

    int bytes_available;

    //do {
    //    ioctl(fd, FIONREAD, &bytes_available);
    //} while(bytes_available>0);


    int num_bytes = read(fd, read_buf, 64);

    if (num_bytes < 0) {
        printf("Error reading: %s", strerror(errno));
        return 1;
    }

    printf("Read %i bytes. Received message: ", num_bytes);
    for(int i = 0; i < num_bytes; i++)
        printf("%02x", read_buf[i]^0xAA);
    printf("\n");

    return 0;
}

