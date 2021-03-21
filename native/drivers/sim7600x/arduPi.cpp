/*
 *  Copyright (C) 2012 Libelium Comunicaciones Distribuidas S.L.
 *  http://www.libelium.com
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Version 2.4 (For Raspberry Pi 2)
 *  Author: Sergio Martinez, Ruben Martin
 */

#include "common/base.h"

#include "drivers/sim7600x/arduPi.h"

namespace {

const uint64_t IOBASE = 0x3f000000;
const uint64_t GPIO_BASE2 = IOBASE + 0x200000;

struct bcm2835_peripheral {
    unsigned long addr_p;
    int mem_fd;
    void *map;
    volatile unsigned int *addr;
};

struct bcm2835_peripheral gpio = {GPIO_BASE2};
struct bcm2835_peripheral bsc_rev1 = {IOBASE + 0x205000};
struct bcm2835_peripheral bsc_rev2 = {IOBASE + 0x804000};
struct bcm2835_peripheral bsc0;

#define GPFSEL0 *(gpio.addr + 0)
#define GPFSEL1 *(gpio.addr + 1)
#define GPFSEL2 *(gpio.addr + 2)
#define GPFSEL3 *(gpio.addr + 3)
#define GPFSEL4 *(gpio.addr + 4)
#define GPFSEL5 *(gpio.addr + 5)
// Reserved @ word offset 6
#define GPSET0 *(gpio.addr + 7)
#define GPSET1 *(gpio.addr + 8)
// Reserved @ word offset 9
#define GPCLR0 *(gpio.addr + 10)
#define GPCLR1 *(gpio.addr + 11)
// Reserved @ word offset 12
#define GPLEV0 *(gpio.addr + 13)
#define GPLEV1 *(gpio.addr + 14)

#define BIT_4 (1 << 4)
#define BIT_6 (1 << 6)
#define BIT_8 (1 << 8)
#define BIT_9 (1 << 9)
#define BIT_10 (1 << 10)
#define BIT_11 (1 << 11)
#define BIT_14 (1 << 14)
#define BIT_17 (1 << 17)
#define BIT_18 (1 << 18)
#define BIT_21 (1 << 21)
#define BIT_27 (1 << 27)
#define BIT_22 (1 << 22)
#define BIT_23 (1 << 23)
#define BIT_24 (1 << 24)
#define BIT_25 (1 << 25)

timeval start_program, end_point;
int REV = 0;

namespace ardupi {

// Sleep the specified milliseconds
void delay(long millis) { ::usleep(millis * 1000); }

void delayMicroseconds(long micros) {
    if (micros > 100) {
        struct timespec tim, tim2;
        tim.tv_sec = 0;
        tim.tv_nsec = micros * 1000;

        if (::nanosleep(&tim, &tim2) < 0) {
            fprintf(stderr, "Nano sleep system call failed \n");
            exit(1);
        }
    } else {
        struct timeval tNow, tLong, tEnd;

        ::gettimeofday(&tNow, NULL);
        tLong.tv_sec = micros / 1000000;
        tLong.tv_usec = micros % 1000000;
        timeradd(&tNow, &tLong, &tEnd);

        while (timercmp(&tNow, &tEnd, <))
            ::gettimeofday(&tNow, NULL);
    }
}

} // namespace ardupi

/**
 * Exposes the physical address defined in the passed structure using mmap on /dev/mem
 */
int map_peripheral(struct bcm2835_peripheral *p) {
    if ((p->mem_fd = ::open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
        printf("Failed to open /dev/mem, try checking permissions.\n");
        return -1;
    }

    p->map = ::mmap(NULL, BCM2835_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                    p->mem_fd, // File descriptor to physical memory virtual file '/dev/mem'
                    p->addr_p  // Address in physical map that we want this memory block to
                               // expose
    );

    if (p->map == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    p->addr = (volatile unsigned int *)p->map;

    return 0;
}

void unmap_peripheral(struct bcm2835_peripheral *p) {
    ::munmap(p->map, BCM2835_BLOCK_SIZE);
    ::close(p->mem_fd);
}

// safe read from peripheral
uint32_t ch_peri_read(volatile uint32_t *paddr) {
    uint32_t ret = *paddr;
    ret = *paddr;
    return ret;
}

// safe write to peripheral
void ch_peri_write(volatile uint32_t *paddr, uint32_t value) {
    *paddr = value;
    *paddr = value;
}

// Set/clear only the bits in value covered by the mask
void ch_peri_set_bits(volatile uint32_t *paddr, uint32_t value, uint32_t mask) {
    uint32_t v = ch_peri_read(paddr);
    v = (v & ~mask) | (value & mask);
    ch_peri_write(paddr, v);
}

} // namespace

/*********************************
 *                               *
 * SerialPi Class implementation *
 * ----------------------------- *
 *********************************/

/******************
 * Public methods *
 ******************/

// Constructor
SerialPi::SerialPi() {
    REV = getBoardRev();
    serialPort = "/dev/ttyS0";
    //	serialPort = "/dev/ttyAMA0";
    timeOut = 1000;

    if (map_peripheral(&gpio) == -1) {
        printf("Failed to map the physical GPIO registers into the virtual memory space.\n");
    }

    // start timer
    gettimeofday(&start_program, NULL);
}

void SerialPi::begin(int serialSpeed) {
    switch (serialSpeed) {
    case 50:
        speed = B50;
        break;
    case 75:
        speed = B75;
        break;
    case 110:
        speed = B110;
        break;
    case 134:
        speed = B134;
        break;
    case 150:
        speed = B150;
        break;
    case 200:
        speed = B200;
        break;
    case 300:
        speed = B300;
        break;
    case 600:
        speed = B600;
        break;
    case 1200:
        speed = B1200;
        break;
    case 1800:
        speed = B1800;
        break;
    case 2400:
        speed = B2400;
        break;
    case 9600:
        speed = B9600;
        break;
    case 19200:
        speed = B19200;
        break;
    case 38400:
        speed = B38400;
        break;
    case 57600:
        speed = B57600;
        break;
    case 115200:
        speed = B115200;
        break;
    default:
        speed = B230400;
        break;
    }

    if ((sd = ::open(serialPort, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK)) == -1) {
        fprintf(stderr, "Unable to open the serial port %s - \n", serialPort);
        exit(-1);
    }

    ::fcntl(sd, F_SETFL, O_RDWR);

    tcgetattr(sd, &options);
    cfmakeraw(&options);
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);

    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag &= ~OPOST;

    tcsetattr(sd, TCSANOW, &options);

    ::ioctl(sd, TIOCMGET, &status);

    status |= TIOCM_DTR;
    status |= TIOCM_RTS;

    ::ioctl(sd, TIOCMSET, &status);
    ::usleep(10000);
}

// Prints data to the serial port as human-readable ASCII text.
void SerialPi::print(const char *message) { ::write(sd, message, strlen(message)); }

// Prints data to the serial port as human-readable ASCII text.
void SerialPi::print(char message) { ::write(sd, &message, 1); }

/*Prints data to the serial port as human-readable ASCII text.
 * It can print the message in many format representations such as:
 * Binary, Octal, Decimal, Hexadecimal and as a BYTE. */
void SerialPi::print(unsigned char i, Representation rep) {
    char *message;
    switch (rep) {

    case BIN:
        message = int2bin(i);
        ::write(sd, message, strlen(message));
        break;
    case OCT:
        message = int2oct(i);
        ::write(sd, message, strlen(message));
        break;
    case DEC:
        sprintf(message, "%d", i);
        ::write(sd, message, strlen(message));
        break;
    case HEX:
        message = int2hex(i);
        ::write(sd, message, strlen(message));
        break;
    case BYTE:
        ::write(sd, &i, 1);
        break;
    }
}

/* Prints data to the serial port as human-readable ASCII text.
 * precission is used to limit the number of decimals.
 */
void SerialPi::print(float f, int precission) {
    /*
const char *str1="%.";
char * str2;
char * str3;
char * message;
sprintf(str2,"%df",precission);
asprintf(&str3,"%s%s",str1,str2);
sprintf(message,str3,f);
    */
    char message[10];
    sprintf(message, "%.1f", f);
    ::write(sd, message, strlen(message));
}

/* Prints data to the serial port as human-readable ASCII text followed
 * by a carriage retrun character '\r' and a newline character '\n' */
void SerialPi::println(const char *message) {
    const char *newline = "\r\n";
    char *msg = NULL;
    asprintf(&msg, "%s%s", message, newline);
    ::write(sd, msg, strlen(msg));
}

/* Prints data to the serial port as human-readable ASCII text followed
 * by a carriage retrun character '\r' and a newline character '\n' */
void SerialPi::println(char message) {
    const char *newline = "\r\n";
    char *msg = NULL;
    asprintf(&msg, "%s%s", &message, newline);
    ::write(sd, msg, strlen(msg));
}

/* Prints data to the serial port as human-readable ASCII text followed
 * by a carriage retrun character '\r' and a newline character '\n' */
void SerialPi::println(int i, Representation rep) {
    char *message;
    switch (rep) {

    case BIN:
        message = int2bin(i);
        break;
    case OCT:
        message = int2oct(i);
        break;
    case DEC:
        sprintf(message, "%d", i);
        break;
    case HEX:
        message = int2hex(i);
        break;
    }

    const char *newline = "\r\n";
    char *msg = NULL;
    asprintf(&msg, "%s%s", message, newline);
    ::write(sd, msg, strlen(msg));
}

/* Prints data to the serial port as human-readable ASCII text followed
 * by a carriage retrun character '\r' and a newline character '\n' */
void SerialPi::println(float f, int precission) {
    const char *str1 = "%.";
    char *str2;
    char *str3;
    char *message;
    sprintf(str2, "%df", precission);
    asprintf(&str3, "%s%s", str1, str2);
    sprintf(message, str3, f);

    const char *newline = "\r\n";
    char *msg = NULL;
    asprintf(&msg, "%s%s", message, newline);
    ::write(sd, msg, strlen(msg));
}

/* Writes binary data to the serial port. This data is sent as a byte
 * Returns: number of bytes written */
int SerialPi::write(unsigned char message) {
    ::write(sd, &message, 1);
    return 1;
}

/* Writes binary data to the serial port. This data is sent as a series
 * of bytes
 * Returns: number of bytes written */
int SerialPi::write(const char *message) {
    int len = strlen(message);
    ::write(sd, &message, len);
    return len;
}

/* Writes binary data to the serial port. This data is sent as a series
 * of bytes placed in an buffer. It needs the length of the buffer
 * Returns: number of bytes written */
int SerialPi::write(const char *message, int size) {
    ::write(sd, message, size);
    return size;
}

/* Get the numberof bytes (characters) available for reading from
 * the serial port.
 * Return: number of bytes avalable to read */
int SerialPi::available() {
    int nbytes = 0;
    if (ioctl(sd, FIONREAD, &nbytes) < 0) {
        fprintf(stderr, "Failed to get byte count on serial.\n");
        exit(-1);
    }
    return nbytes;
}

/* Reads 1 byte of incoming serial data
 * Returns: first byte of incoming serial data available */
char SerialPi::read() {
    ::read(sd, &c, 1);
    return c;
}

/* Reads characters from th serial port into a buffer. The function
 * terminates if the determined length has been read, or it times out
 * Returns: number of bytes readed */
int SerialPi::readBytes(char message[], int size) {
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time1);
    int count;
    for (count = 0; count < size; count++) {
        if (available())
            ::read(sd, &message[count], 1);
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time2);
        timespec t = timeDiff(time1, time2);
        if ((t.tv_nsec / 1000) > timeOut)
            break;
    }
    return count;
}

/* Reads characters from the serial buffer into an array.
 * The function terminates if the terminator character is detected,
 * the determined length has been read, or it times out.
 * Returns: number of characters read into the buffer. */
int SerialPi::readBytesUntil(char character, char buffer[], int length) {
    char lastReaded = character + 1; // Just to make lastReaded != character
    int count = 0;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time1);
    while (count != length && lastReaded != character) {
        if (available())
            ::read(sd, &buffer[count], 1);
        lastReaded = buffer[count];
        count++;
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time2);
        timespec t = timeDiff(time1, time2);
        if ((t.tv_nsec / 1000) > timeOut)
            break;
    }

    return count;
}

bool SerialPi::find(const char *target) { findUntil(target, NULL); }

/* Reads data from the serial buffer until a target string of given length
 * or terminator string is found.
 * Returns: true if the target string is found, false if it times out */
bool SerialPi::findUntil(const char *target, const char *terminal) {
    int index = 0;
    int termIndex = 0;
    int targetLen = strlen(target);
    int termLen = strlen(terminal);
    char readed;
    timespec t;

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time1);

    if (*target == 0)
        return true; // return true if target is a null string

    do {
        if (available()) {
            ::read(sd, &readed, 1);
            if (readed != target[index])
                index = 0; // reset index if any char does not match

            if (readed == target[index]) {
                if (++index >= targetLen) { // return true if all chars in the target match
                    return true;
                }
            }

            if (termLen > 0 && c == terminal[termIndex]) {
                if (++termIndex >= termLen)
                    return false; // return false if terminate string found before target string
            } else {
                termIndex = 0;
            }
        }

        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time2);
        t = timeDiff(time1, time2);

    } while ((t.tv_nsec / 1000) <= timeOut);

    return false;
}

/* returns the first valid (long) integer value from the current position.
 * initial characters that are not digits (or the minus sign) are skipped
 * function is terminated by the first character that is not a digit. */
long SerialPi::parseInt() {
    bool isNegative = false;
    long value = 0;
    char c;

    // Skip characters until a number or - sign found
    do {
        c = peek();
        if (c == '-')
            break;
        if (c >= '0' && c <= '9')
            break;
        ::read(sd, &c, 1); // discard non-numeric
    } while (1);

    do {
        if (c == '-')
            isNegative = true;
        else if (c >= '0' && c <= '9') // is c a digit?
            value = value * 10 + c - '0';
        ::read(sd, &c, 1); // consume the character we got with peek
        c = peek();

    } while (c >= '0' && c <= '9');

    if (isNegative)
        value = -value;
    return value;
}

float SerialPi::parseFloat() {
    bool isNegative = false;
    bool isFraction = false;
    long value = 0;
    char c;
    float fraction = 1.0;

    // Skip characters until a number or - sign found
    do {
        c = peek();
        if (c == '-')
            break;
        if (c >= '0' && c <= '9')
            break;
        ::read(sd, &c, 1); // discard non-numeric
    } while (1);

    do {
        if (c == '-')
            isNegative = true;
        else if (c == '.')
            isFraction = true;
        else if (c >= '0' && c <= '9') { // is c a digit?
            value = value * 10 + c - '0';
            if (isFraction)
                fraction *= 0.1;
        }
        ::read(sd, &c, 1); // consume the character we got with peek
        c = peek();
    } while ((c >= '0' && c <= '9') || (c == '.' && isFraction == false));

    if (isNegative)
        value = -value;
    if (isFraction)
        return value * fraction;
    else
        return value;
}

// Returns the next byte (character) of incoming serial data without removing it from the internal
// serial buffer.
char SerialPi::peek() {
    // We obtain a pointer to FILE structure from the file descriptor sd
    FILE *f = fdopen(sd, "r+");
    // With a pointer to FILE we can do getc and ungetc
    c = getc(f);
    ungetc(c, f);
    return c;
}

// Remove any data remaining on the serial buffer
void SerialPi::flush() {
    while (available()) {
        ::read(sd, &c, 1);
    }
}

/* Sets the maximum milliseconds to wait for serial data when using SerialPi::readBytes()
 * The default value is set to 1000 */
void SerialPi::setTimeout(long millis) { timeOut = millis; }

// Disables serial communication
void SerialPi::end() { ::close(sd); }

/*******************
 * Private methods *
 *******************/

// Returns a timespec struct with the time elapsed between start and end timespecs
timespec SerialPi::timeDiff(timespec start, timespec end) {
    timespec temp;
    if ((end.tv_nsec - start.tv_nsec) < 0) {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return temp;
}

// Returns a binary representation of the integer passed as argument
char *SerialPi::int2bin(int i) {
    size_t bits = sizeof(int) * CHAR_BIT;
    char *str = (char *)malloc(bits + 1);
    int firstCeros = 0;
    int size = bits;
    if (!str)
        return NULL;
    str[bits] = 0;

    // type punning because signed shift is implementation-defined
    unsigned u = *(unsigned *)&i;
    for (; bits--; u >>= 1)
        str[bits] = u & 1 ? '1' : '0';

    // Delete first 0's
    for (int i = 0; i < bits; i++) {
        if (str[i] == '0') {
            firstCeros++;
        } else {
            break;
        }
    }
    char *str_noceros = (char *)malloc(size - firstCeros + 1);
    for (int i = 0; i < (size - firstCeros); i++) {
        str_noceros[i] = str[firstCeros + i];
    }

    return str_noceros;
}

// Returns an hexadecimal representation of the integer passed as argument
char *SerialPi::int2hex(int i) {
    char buffer[32];
    sprintf(buffer, "%x", i);
    char *hex = (char *)malloc(strlen(buffer) + 1);
    strcpy(hex, buffer);
    return hex;
}

// Returns an octal representation of the integer passed as argument
char *SerialPi::int2oct(int i) {
    char buffer[32];
    sprintf(buffer, "%o", i);
    char *oct = (char *)malloc(strlen(buffer) + 1);
    strcpy(oct, buffer);
    return oct;
}

/********** FUNCTIONS OUTSIDE CLASSES **********/

// Configures the specified pin to behave either as an input or an output
void pinMode(int pin, Pinmode mode) {
    if (mode == OUTPUT) {
        switch (pin) {
        case 4:
            GPFSEL0 &= ~(7 << 12);
            GPFSEL0 |= (1 << 12);
            break;
        case 6:
            GPFSEL0 &= ~(7 << 18);
            GPFSEL0 |= (1 << 18);
            break;
        case 8:
            GPFSEL0 &= ~(7 << 24);
            GPFSEL0 |= (1 << 24);
            break;
        case 9:
            GPFSEL0 &= ~(7 << 27);
            GPFSEL0 |= (1 << 27);
            break;
        case 10:
            GPFSEL1 &= ~(7 << 0);
            GPFSEL1 |= (1 << 0);
            break;
        case 11:
            GPFSEL1 &= ~(7 << 3);
            GPFSEL1 |= (1 << 3);
            break;
        case 14:
            GPFSEL1 &= ~(7 << 12);
            GPFSEL1 |= (1 << 12);
            break;
        case 17:
            GPFSEL1 &= ~(7 << 21);
            GPFSEL1 |= (1 << 21);
            break;
        case 18:
            GPFSEL1 &= ~(7 << 24);
            GPFSEL1 |= (1 << 24);
            break;
        case 21:
            GPFSEL2 &= ~(7 << 3);
            GPFSEL2 |= (1 << 3);
            break;
        case 27:
            GPFSEL2 &= ~(7 << 21);
            GPFSEL2 |= (1 << 21);
            break;
        case 22:
            GPFSEL2 &= ~(7 << 6);
            GPFSEL2 |= (1 << 6);
            break;
        case 23:
            GPFSEL2 &= ~(7 << 9);
            GPFSEL2 |= (1 << 9);
            break;
        case 24:
            GPFSEL2 &= ~(7 << 12);
            GPFSEL2 |= (1 << 12);
            break;
        case 25:
            GPFSEL2 &= ~(7 << 15);
            GPFSEL2 |= (1 << 15);
            break;
        }

    } else if (mode == INPUT) {
        switch (pin) {
        case 4:
            GPFSEL0 &= ~(7 << 12);
            break;
        case 6:
            GPFSEL0 &= ~(7 << 18);
            break;
        case 8:
            GPFSEL0 &= ~(7 << 24);
            break;
        case 9:
            GPFSEL0 &= ~(7 << 27);
            break;
        case 10:
            GPFSEL1 &= ~(7 << 0);
            break;
        case 11:
            GPFSEL1 &= ~(7 << 3);
            break;
        case 14:
            GPFSEL1 &= ~(7 << 12);
            break;
        case 17:
            GPFSEL1 &= ~(7 << 21);
            break;
        case 18:
            GPFSEL1 &= ~(7 << 24);
            break;
        case 21:
            GPFSEL2 &= ~(7 << 3);
            break;
        case 27:
            GPFSEL2 &= ~(7 << 3);
            break;
        case 22:
            GPFSEL2 &= ~(7 << 6);
            break;
        case 23:
            GPFSEL2 &= ~(7 << 9);
            break;
        case 24:
            GPFSEL2 &= ~(7 << 12);
            break;
        case 25:
            GPFSEL2 &= ~(7 << 15);
            break;
        }
    }
}

// Write a HIGH or a LOW value to a digital pin
void digitalWrite(int pin, int value) {
    if (value == HIGH) {
        switch (pin) {
        case 4:
            GPSET0 = BIT_4;
            break;
        case 6:
            GPSET0 = BIT_6;
            break;
        case 8:
            GPSET0 = BIT_8;
            break;
        case 9:
            GPSET0 = BIT_9;
            break;
        case 10:
            GPSET0 = BIT_10;
            break;
        case 11:
            GPSET0 = BIT_11;
            break;
        case 14:
            GPSET0 = BIT_14;
            break;
        case 17:
            GPSET0 = BIT_17;
            break;
        case 18:
            GPSET0 = BIT_18;
            break;
        case 21:
            GPSET0 = BIT_21;
            break;
        case 27:
            GPSET0 = BIT_27;
            break;
        case 22:
            GPSET0 = BIT_22;
            break;
        case 23:
            GPSET0 = BIT_23;
            break;
        case 24:
            GPSET0 = BIT_24;
            break;
        case 25:
            GPSET0 = BIT_25;
            break;
        }
    } else if (value == LOW) {
        switch (pin) {
        case 4:
            GPCLR0 = BIT_4;
            break;
        case 6:
            GPCLR0 = BIT_6;
            break;
        case 8:
            GPCLR0 = BIT_8;
            break;
        case 9:
            GPCLR0 = BIT_9;
            break;
        case 10:
            GPCLR0 = BIT_10;
            break;
        case 11:
            GPCLR0 = BIT_11;
            break;
        case 14:
            GPCLR0 = BIT_14;
            break;
        case 17:
            GPCLR0 = BIT_17;
            break;
        case 18:
            GPCLR0 = BIT_18;
            break;
        case 21:
            GPCLR0 = BIT_21;
            break;
        case 27:
            GPCLR0 = BIT_27;
            break;
        case 22:
            GPCLR0 = BIT_22;
            break;
        case 23:
            GPCLR0 = BIT_23;
            break;
        case 24:
            GPCLR0 = BIT_24;
            break;
        case 25:
            GPCLR0 = BIT_25;
            break;
        }
    }

    delayMicroseconds(1);
    // Delay to allow any change in state to be reflected in the LEVn, register bit.
}

// Soft digitalWrite to avoid spureous Reset in Socket Power ON
void digitalWriteSoft(int pin, int value) {
    uint frame[32];

    for (int z = 0; z < 7; z++) {
        uint V = 0xFFFFFFFF;

        for (int v = 0; v < 32; v++) {
            uint T = 0xFFFFFFFF;

            V = V << 1;
            for (int t = 0; t < 32; t++) {
                if (value == HIGH) {
                    if (T > V)
                        frame[t] = HIGH;
                    else
                        frame[t] = LOW;
                } else {
                    if (T > V)
                        frame[t] = LOW;
                    else
                        frame[t] = HIGH;
                }
                T = T << 1;
            }

            for (int i = 0; i < 32; i++) {
                digitalWrite(pin, frame[i]);
            }
        }
    }

    if (value == HIGH)
        digitalWrite(pin, HIGH);
    else
        digitalWrite(pin, LOW);
}

// Reads the value from a specified digital pin, either kHigh or kLow.
int digitalRead(int pin) {
    Digivalue value;
    switch (pin) {
    case 4:
        if (GPLEV0 & BIT_4) {
            value = kHigh;
        } else {
            value = kLow;
        };
        break;
    case 6:
        if (GPLEV0 & BIT_6) {
            value = kHigh;
        } else {
            value = kLow;
        };
        break;
    case 8:
        if (GPLEV0 & BIT_8) {
            value = kHigh;
        } else {
            value = kLow;
        };
        break;
    case 9:
        if (GPLEV0 & BIT_9) {
            value = kHigh;
        } else {
            value = kLow;
        };
        break;
    case 10:
        if (GPLEV0 & BIT_10) {
            value = kHigh;
        } else {
            value = kLow;
        };
        break;
    case 11:
        if (GPLEV0 & BIT_11) {
            value = kHigh;
        } else {
            value = kLow;
        };
        break;
    case 17:
        if (GPLEV0 & BIT_17) {
            value = kHigh;
        } else {
            value = kLow;
        };
        break;
    case 18:
        if (GPLEV0 & BIT_18) {
            value = kHigh;
        } else {
            value = kLow;
        };
        break;
    case 21:
        if (GPLEV0 & BIT_21) {
            value = kHigh;
        } else {
            value = kLow;
        };
        break;
    case 27:
        if (GPLEV0 & BIT_27) {
            value = kHigh;
        } else {
            value = kLow;
        };
        break;
    case 22:
        if (GPLEV0 & BIT_22) {
            value = kHigh;
        } else {
            value = kLow;
        };
        break;
    case 23:
        if (GPLEV0 & BIT_23) {
            value = kHigh;
        } else {
            value = kLow;
        };
        break;
    case 24:
        if (GPLEV0 & BIT_24) {
            value = kHigh;
        } else {
            value = kLow;
        };
        break;
    case 25:
        if (GPLEV0 & BIT_25) {
            value = kHigh;
        } else {
            value = kLow;
        };
        break;
    }
    return value;
}

long millis() {
    long elapsedTime;
    // stop timer
    gettimeofday(&end_point, NULL);

    // compute and print the elapsed time in millisec
    elapsedTime = (end_point.tv_sec - start_program.tv_sec) * 1000.0;    // sec to ms
    elapsedTime += (end_point.tv_usec - start_program.tv_usec) / 1000.0; // us to ms
    return elapsedTime;
}

int getBoardRev() {
    FILE *cpu_info;
    char line[120];
    char *c, finalChar;
    static int rev = 0;

    if (REV != 0)
        return REV;

    if ((cpu_info = ::fopen("/proc/cpuinfo", "r")) == NULL) {
        fprintf(stderr, "Unable to open /proc/cpuinfo. Cannot determine board reivision.\n");
        exit(1);
    }

    while (::fgets(line, 120, cpu_info) != NULL) {
        if (strncmp(line, "Revision", 8) == 0)
            break;
    }

    ::fclose(cpu_info);

    if (line == NULL) {
        fprintf(stderr, "Unable to determine board revision from /proc/cpuinfo.\n");
        exit(1);
    }

    for (c = line; *c; ++c)
        if (isdigit(*c))
            break;

    if (!isdigit(*c)) {
        fprintf(stderr, "Unable to determine board revision from /proc/cpuinfo\n");
        fprintf(stderr, "  (Info not found in: %s\n", line);
        exit(1);
    }

    finalChar = c[strlen(c) - 2];

    if ((finalChar == '2') || (finalChar == '3')) {
        bsc0 = bsc_rev1;
        return 1;
    } else {
        bsc0 = bsc_rev2;
        return 2;
    }
}

void ch_gpio_fsel(uint8_t pin, uint8_t mode) {
    // Function selects are 10 pins per 32 bit word, 3 bits per pin
    volatile uint32_t *paddr = (volatile uint32_t *)gpio.map + BCM2835_GPFSEL0 / 4 + (pin / 10);
    uint8_t shift = (pin % 10) * 3;
    uint32_t mask = BCM2835_GPIO_FSEL_MASK << shift;
    uint32_t value = mode << shift;
    ch_peri_set_bits(paddr, value, mask);
}

SerialPi Serial = SerialPi();
