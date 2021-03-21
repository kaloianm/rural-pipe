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

#pragma once

#include <algorithm>
#include <bcm2835.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h> // Include forva_start, va_arg and va_end strings functions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

enum Representation { BIN, OCT, DEC, HEX, BYTE };

typedef enum { INPUT, OUTPUT } Pinmode;

typedef enum { kLow = 0, kHigh = 1, kRising = 2, kFalling = 3, kBoth = 4 } Digivalue;

/* SerialPi Class
 * Class that provides the functionality of arduino Serial library
 */
class SerialPi {
public:
    SerialPi();

    /**
     * Sets the data rate in bits per second (baud) for serial data transmission
     */
    void begin(int serialSpeed);

    int available();
    char read();
    int readBytes(char message[], int size);
    int readBytesUntil(char character, char buffer[], int length);
    bool find(const char *target);
    bool findUntil(const char *target, const char *terminal);
    long parseInt();
    float parseFloat();
    char peek();
    void print(const char *message);
    void print(char message);
    void print(unsigned char i, Representation rep);
    void print(float f, int precission);
    void println(const char *message);
    void println(char message);
    void println(int i, Representation rep);
    void println(float f, int precission);
    int write(unsigned char message);
    int write(const char *message);
    int write(const char *message, int size);
    void flush();
    void setTimeout(long millis);
    void end();

private:
    int sd, status;
    const char *serialPort;
    unsigned char c;
    struct termios options;
    int speed;
    long timeOut;
    timespec time1, time2;
    timespec timeDiff(timespec start, timespec end);
    char *int2bin(int i);
    char *int2hex(int i);
    char *int2oct(int i);
};

extern SerialPi Serial;

/* Some useful arduino functions */
void pinMode(int pin, Pinmode mode);
void digitalWrite(int pin, int value);
void digitalWriteSoft(int pin, int value);
int digitalRead(int pin);

long millis();

int getBoardRev();
