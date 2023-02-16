#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/select.h>

#include "driver_ir.h"

static int ir_fd;

static uint64_t td_us(struct timeval end, struct timeval start) {
    return (end.tv_sec - start.tv_sec)*1000000 + (end.tv_usec - start.tv_usec);
}

/*
 *  Read from IR
 *  Wait for 500ms, or until RX has something to read.
 *  Read bytes until 3.748ms since the last read byte.
 *
 *  1 byte @115200 ~= 90us read.
 *  136 bytes @115200 ~= 11.8ms read.
 *
 *  I get ~1us/iteration, but `read()` only ever returns
 *  bytes on the first call, or until ~50ms later, by
 *  which time the peer has timed out.
 *
 *  Only a problem with large, consecutive packets.
 *  It's also unreliable, sometimes I can read a packet
 *  over 100 bytes long, then the next packet stops reading
 *  at ~30 bytes in.
 *
 *  Testing with an IrDA 3-click board -> ftdi2232h over usb.
 *
 *  For a log of comms, see `walk_start.log`
 *  Also see `pw_ir_init()` for serial setup.
 *
 */
int pw_ir_read(uint8_t *buf, size_t max_len) {

    struct timeval start, now, read_start, last_rx, tmp;
    int bytes_available, total_read = 0;
    uint64_t t;
    bool timeout = false;

    gettimeofday(&start, NULL);

    // wait til we have bytes in the rx buffer
    do {
        ioctl(ir_fd, FIONREAD, &bytes_available);
        gettimeofday(&now, NULL);
        timeout = td_us(now, start) >= 500*1000;
    } while( (bytes_available <= 0) && !timeout  );


    // start read
    gettimeofday(&read_start, NULL);
    do {
        ioctl(ir_fd, FIONREAD, &bytes_available);
        if(bytes_available > 0) {
            gettimeofday(&tmp, NULL);
            total_read = read(ir_fd, buf, max_len);
            gettimeofday(&last_rx, NULL);
            printf("Read took %lu us\n", td_us(last_rx, tmp));
        }

        gettimeofday(&now, NULL);
        t = td_us(now, last_rx);
    } while( t < 3748 );

    gettimeofday(&now, NULL);
    t = td_us(now, start);

#ifdef DRIVER_IR_DEBUG_READ
    printf("\tread %d bytes: (%lu us)", total_read, t);
    for(size_t i = 0; i < total_read; i++) {
        if(i%8 == 0) printf(" ");
        if(i%16 == 0) printf("\n\t\t");
        printf("%02x", buf[i]^0xaa);
    }
    printf("\n");
#endif /* DRIVER_IR_DEBUG_READ */

    return total_read;
}

int pw_ir_write(uint8_t *buf, size_t len) {
    struct timeval start, now;

    gettimeofday(&start, NULL);
    int total_written = write(ir_fd, buf, len);
    gettimeofday(&now, NULL);
    uint64_t t = (now.tv_sec - start.tv_sec)*1000000 + (now.tv_usec - start.tv_usec);

#ifdef DRIVER_IR_DEBUG_WRITE
    printf("\twrite %lu bytes: (%lu us)", len, t);
    for(size_t i = 0; i < len; i++) {
        if(i%8 == 0) printf(" ");
        if(i%16 == 0) printf("\n\t\t");
        printf("%02x", buf[i]^0xaa);
    }
    printf("\n");
#endif /* DRIVER_IR_DEBUG_WRITE */

    return total_written;
}

int pw_ir_clear_rx() {
    int bytes;
    ioctl(ir_fd, TCFLSH, &bytes);
    return 0;
}

int pw_ir_init() {
    const char fname[] = "/dev/ttyUSB1";
    ir_fd = open(fname, O_RDWR | O_NONBLOCK);
    if(ir_fd <= 0) {
        //sprintf(stderr, "Error: cannot open %s for serial.\n", fname);
        printf("Error: cannot open %s for serial.\n", fname);
    }

    struct termios tty;

    if(tcgetattr(ir_fd, &tty) != 0) {
        printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
        return -1;
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

    // i.e. timeout 200ms after start of read
    // NOTE: behaviour needs testing, I don't know if it will break after the first byte or if it will
    // break if the timer runs out while there is still data incoming
    tty.c_cc[VTIME] = 0;    // max time between bytes = 100ms
    tty.c_cc[VMIN] = 0;     // min number of bytes per read is 0
    // 100ms is way too long to wait for last byte

    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    if (tcsetattr(ir_fd, TCSANOW, &tty) != 0) {
      printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
      return -1;
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

void pw_ir_delay_ms(size_t ms) {
    usleep(ms*1000);
}

