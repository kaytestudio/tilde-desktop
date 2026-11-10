#include "serial.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <dirent.h>

/* ─── baud rate conversion ──────────────────────────────────────────── */

static speed_t baud_to_speed(BaudRate baud) {
    switch (baud) {
        case BAUD_300:    return B300;
        case BAUD_1200:   return B1200;
        case BAUD_2400:   return B2400;
        case BAUD_4800:   return B4800;
        case BAUD_9600:   return B9600;
        case BAUD_19200:  return B19200;
        case BAUD_38400:  return B38400;
        case BAUD_57600:  return B57600;
        case BAUD_115200: return B115200;
        case BAUD_230400: return B230400;
#ifdef B460800
        case BAUD_460800: return B460800;
#endif
        default:          return B9600;
    }
}

/* ─── open ───────────────────────────────────────────────────────────── */

int serial_open(SerialPort *port, const char *device, BaudRate baud,
                int data_bits, Parity parity, StopBits stop_bits,
                int flow_control) {
    memset(port, 0, sizeof(SerialPort));
    strncpy(port->device, device, MAX_PORT_NAME - 1);
    port->baud         = baud;
    port->data_bits    = data_bits;
    port->parity       = parity;
    port->stop_bits    = stop_bits;
    port->flow_control = flow_control;

    /* open device in non-blocking mode */
    port->fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (port->fd < 0) {
        perror("serial_open");
        return -1;
    }

    /* get current termios */
    struct termios tty;
    if (tcgetattr(port->fd, &tty) != 0) {
        perror("tcgetattr");
        close(port->fd);
        port->fd = -1;
        return -1;
    }

    /* ── baud rate ── */
    speed_t speed = baud_to_speed(baud);
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    /* ── control flags ── */
    tty.c_cflag &= ~CSIZE;
    if (data_bits == 7)
        tty.c_cflag |= CS7;
    else
        tty.c_cflag |= CS8;

    /* parity */
    if (parity == PARITY_NONE) {
        tty.c_cflag &= ~PARENB;
    } else {
        tty.c_cflag |= PARENB;
        if (parity == PARITY_ODD)
            tty.c_cflag |= PARODD;
        else
            tty.c_cflag &= ~PARODD;
    }

    /* stop bits */
    if (stop_bits == STOPBITS_2)
        tty.c_cflag |= CSTOPB;
    else
        tty.c_cflag &= ~CSTOPB;

    /* flow control */
    if (flow_control) {
        tty.c_cflag |= CRTSCTS;  /* hardware RTS/CTS */
    } else {
        tty.c_cflag &= ~CRTSCTS;
    }

    tty.c_cflag |= (CLOCAL | CREAD); /* enable receiver, ignore modem control */

    /* ── input flags ── */
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);           /* no software flow control */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    /* ── output flags ── */
    tty.c_oflag &= ~OPOST; /* raw output */

    /* ── local flags ── */
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    /* ── read timeout ── */
    tty.c_cc[VMIN]  = 0;  /* non-blocking read */
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(port->fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        close(port->fd);
        port->fd = -1;
        return -1;
    }

    port->active = 1;
    return 0;
}

/* ─── close ──────────────────────────────────────────────────────────── */

void serial_close(SerialPort *port) {
    if (port->fd >= 0) {
        close(port->fd);
        port->fd = -1;
    }
    port->active = 0;
}

/* ─── read ───────────────────────────────────────────────────────────── */

ssize_t serial_read(SerialPort *port, void *buf, size_t count) {
    if (port->fd < 0) return -1;
    return read(port->fd, buf, count);
}

/* ─── write ──────────────────────────────────────────────────────────── */

ssize_t serial_write(SerialPort *port, const void *buf, size_t count) {
    if (port->fd < 0) return -1;
    return write(port->fd, buf, count);
}

/* ─── list ports (macOS) ─────────────────────────────────────────────── */

int serial_list_ports(char out[][MAX_PORT_NAME], int max_count) {
    DIR *d = opendir("/dev");
    if (!d) return 0;

    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && count < max_count) {
        /* macOS serial devices: cu.* or tty.* (USB, Bluetooth)
         * Common patterns: cu.usbserial*, cu.usbmodem*, cu.Bluetooth* */
        if (strncmp(ent->d_name, "cu.", 3) == 0 ||
            strncmp(ent->d_name, "tty.usb", 7) == 0) {
            snprintf(out[count], MAX_PORT_NAME, "/dev/%s", ent->d_name);
            count++;
        }
    }
    closedir(d);
    return count;
}

/* ─── status ─────────────────────────────────────────────────────────── */

int serial_get_status(SerialPort *port, SerialStatus *status) {
    if (port->fd < 0) return -1;

    int lines = 0;
    if (ioctl(port->fd, TIOCMGET, &lines) < 0) {
        perror("ioctl TIOCMGET");
        return -1;
    }

    memset(status, 0, sizeof(SerialStatus));
    status->dsr = (lines & TIOCM_DSR) ? 1 : 0;
    status->dcd = (lines & TIOCM_CAR) ? 1 : 0;
    status->rts = (lines & TIOCM_RTS) ? 1 : 0;
    status->cts = (lines & TIOCM_CTS) ? 1 : 0;
    status->dtr = (lines & TIOCM_DTR) ? 1 : 0;

    return 0;
}
