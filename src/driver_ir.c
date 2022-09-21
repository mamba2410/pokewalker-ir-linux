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

    int total_read=0, bytes_read, bytes_available, bytes_left;

    do {
        ioctl(ir_fd, FIONREAD, &bytes_available);
    } while(bytes_available < max_len);

    //printf("\t%02x bytes available\n", bytes_available);
    total_read = read(ir_fd, buf, max_len);

    /*
    do {
        bytes_left = max_len - total_read;
        ioctl(ir_fd, FIONREAD, &bytes_available);
        if(bytes_available>0) {
            bytes_read = read(ir_fd, buf+total_read, bytes_left);
            total_read += bytes_read;
        }
    } while(bytes_available < max_len);
    */


    return total_read;
}

int pw_ir_write(uint8_t *buf, size_t len) {
    printf("\twrite: ");
    for(size_t i = 0; i < len; i++)
        printf("%02x", buf[i]^0xaa);
    printf("\n");
    return write(ir_fd, buf, len);
}

int pw_ir_clear_rx() {
    int bytes;
    ioctl(ir_fd, TCFLSH, &bytes);
    //uint8_t buf[256];
    //do {
    //    ioctl(ir_fd, FIONREAD, &bytes);
    //    read(ir_fd, buf, bytes);
    //} while(bytes > 0);
    return 0;
}

int pw_ir_init() {
    ir_fd = open("/dev/ttyUSB1", O_RDWR);

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

    // reads break 1 deci-second after first byte, or after 255 characters, whichever comes first
    // for reference, 136 bytes @115200 baud takes ~10ms, 100ms since first read is more than enough
    tty.c_cc[VTIME] = 1;
    tty.c_cc[VMIN] = 255;

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

