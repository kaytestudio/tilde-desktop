#ifndef SERIAL_H
#define SERIAL_H

#include <stddef.h>
#include <sys/types.h>

#define MAX_SERIAL_PORTS 16
#define MAX_PORT_NAME    64

typedef enum {
    BAUD_300    = 300,
    BAUD_1200   = 1200,
    BAUD_2400   = 2400,
    BAUD_4800   = 4800,
    BAUD_9600   = 9600,
    BAUD_19200  = 19200,
    BAUD_38400  = 38400,
    BAUD_57600  = 57600,
    BAUD_115200 = 115200,
    BAUD_230400 = 230400,
    BAUD_460800 = 460800
} BaudRate;

typedef enum {
    PARITY_NONE = 0,
    PARITY_ODD  = 1,
    PARITY_EVEN = 2
} Parity;

typedef enum {
    STOPBITS_1 = 1,
    STOPBITS_2 = 2
} StopBits;

typedef struct {
    int       fd;                    /* file descriptor                    */
    char      device[MAX_PORT_NAME]; /* e.g. /dev/ttyUSB0, /dev/cu.usbserial */
    BaudRate  baud;
    int       data_bits;             /* 7 or 8                             */
    Parity    parity;
    StopBits  stop_bits;
    int       flow_control;          /* 1 = hardware (RTS/CTS), 0 = none   */
    int       active;                /* 1 if open                          */
} SerialPort;

/* ── lifecycle ── */

/* Open a serial port with specified settings. Returns 0 on success. */
int  serial_open(SerialPort *port, const char *device, BaudRate baud,
                int data_bits, Parity parity, StopBits stop_bits,
                int flow_control);

void serial_close(SerialPort *port);

/* ── I/O ── */

/* Non-blocking read. Returns number of bytes read, 0 if none available, -1 on error. */
ssize_t serial_read(SerialPort *port, void *buf, size_t count);

/* Write data. Returns number of bytes written, -1 on error. */
ssize_t serial_write(SerialPort *port, const void *buf, size_t count);

/* ── discovery (macOS) ── */

/* Enumerate available serial ports. Returns count, fills `out` array. */
int  serial_list_ports(char out[][MAX_PORT_NAME], int max_count);

/* ── status ── */

/* Get modem control lines status (for RTS/CTS/DTR/DSR monitoring) */
typedef struct {
    int dsr;  /* Data Set Ready   */
    int dcd;  /* Data Carrier Detect */
    int rts;  /* Request To Send (output) */
    int cts;  /* Clear To Send    */
    int dtr;  /* Data Terminal Ready (output) */
} SerialStatus;

int  serial_get_status(SerialPort *port, SerialStatus *status);

#endif /* SERIAL_H */
