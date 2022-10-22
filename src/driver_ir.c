#include <stdio.h>
#include <stdint.h>

#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

#include "driver_ir.h"

static int ir_fd;

int pw_ir_read(uint8_t *buf, size_t max_len) {

    int total_read = read(ir_fd, buf, max_len);

    //printf("\tread:  ");
    //for(size_t i = 0; i < total_read; i++)
    //    printf("%02x", buf[i]^0xaa);
    //printf("\n");

    return total_read;
}

int pw_ir_write(uint8_t *buf, size_t len) {

    int total_written = write(ir_fd, buf, len);

    //printf("\twrite: ");
    //for(size_t i = 0; i < len; i++)
    //    printf("%02x", buf[i]^0xaa);
    //printf("\n");

    return total_written;
}

int pw_ir_clear_rx() {
    int bytes;
    ioctl(ir_fd, TCFLSH, &bytes);
    return 0;
}

int pw_ir_init() {
    const char fname[] = "/dev/ttyUSB1";
    ir_fd = open(fname, O_RDWR);
    if(ir_fd <= 0) {
        //sprintf(stderr, "Error: cannot open %s for serial.\n", fname);
        printf("Error: cannot open %s for serial.\n", fname);
    }

    struct termios tty;

    if(tcgetattr(ir_fd, &tty) != 0) {
        printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
        return 1;
    }

    tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
    tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
    tty.c_cflag &= ~CSIZE; // Clear all bits that set the data size
    tty.c_cflag |= CS8; // 8 bits per byte (most common)
    tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
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

    // see http://unixwiz.net/techtips/termios-vmin-vtime.html
    // for reference, 136 bytes @115200 baud takes ~10ms, 100ms since first read is more than enough

    // ~~reads break 1 deci-second after first byte, or after 255 characters, whichever comes first~~
    //tty.c_cc[VTIME] = 1;    // max time between bytes = 100ms
    //tty.c_cc[VMIN] = 255;   // min number of bytes per read is 255

    // i.e. timeout 200ms after start of read
    // NOTE: behaviour needs testing, I don't know if it will break after the first byte or if it will
    // break if the timer runs out while there is still data incoming
    tty.c_cc[VTIME] = 2;    // max time between bytes = 200ms
    tty.c_cc[VMIN] = 0;     // min number of bytes per read is 0

    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    if (tcsetattr(ir_fd, TCSANOW, &tty) != 0) {
      printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
      return 1;
    }

    tcflush(ir_fd, TCIFLUSH);

    return 0;
}

void pw_ir_flush_rx() {
    tcflush(ir_fd, TCIFLUSH);
    tcflush(ir_fd, TCOFLUSH);
}

void pw_ir_deinit() {

    close(ir_fd);
}

