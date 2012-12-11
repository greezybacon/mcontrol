#include "mdrive.h"
#include "serial.h"

#include <dirent.h>
#include <regex.h>
#include <stdio.h>
#include <termios.h>

static char * addresses[] = {
    "", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
    "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "Z", "Y", "Z",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", NULL };

char **
mdrive_enum_serial_ports(void) {
    struct dirent *ep;
    DIR * dp = opendir("/sys/class/tty");
    if (dp == NULL)
        return NULL;

    // Ignore console ttys (console, tty##)
    regex_t filter;
    int status = regcomp(&filter, 
        "[.]+$|ptmx|console|tty$|tty[0-9]+|ttyM32",
        REG_EXTENDED);
    if (status != 0) {
        mcTraceF(1, MDRIVE_CHANNEL, "FAILED to compile regex");
        return NULL;
    }

    char ** list = calloc(32, sizeof(char *));
    char ** this = list;

    while ((ep = readdir(dp))) {
        // Filter out console ttys
        status = regexec(&filter, ep->d_name, 0, NULL, 0);
        if (status == 0)
            continue;

        *this = malloc(strlen(ep->d_name)+6);
        snprintf(*this, strlen(ep->d_name)+6, "/dev/%s", ep->d_name);
        this++;
    }

    regfree(&filter);
    return list;
}

mdrive_address_t *
mdrive_enum_motors_on_port(const char * port) {
    static char * command = "%sFD%c"; //PR DN%c";
    static struct timespec timeout = { .tv_nsec=18e6 };

    int fd, length;
    char rxbuf[256], txbuf[64];
    char address[4];
    mdrive_address_t * list = calloc(63, sizeof(mdrive_address_t));
    mdrive_address_t * results = list;

    for (baud_rate_t * s = baud_rates; s->human; s++) {
        if (fd) close(fd);
        fd = mdrive_initialize_port(port, s->human, false);
        if (fd < 0)
            continue;
        mcTraceF(20, MDRIVE_CHANNEL, "Search at %d baud", s->human);
        struct termios tty;

        tcgetattr(fd, &tty);

        cfsetispeed(&tty, s->constant);
        cfsetospeed(&tty, s->constant);

        tcsetattr(fd, TCSAFLUSH, &tty);

        for (char ** addr = addresses; *addr; addr++) {
            // Request the motor at this address to give the serial number
            // followed by '$' and its party address
            length = snprintf(txbuf, sizeof txbuf, command, *addr,
                (strlen(*addr) ? '\n' : '\r'));

            // Assume checksum mode
            txbuf[length] = txbuf[length-1];
            txbuf[length-1] = 0;
            txbuf[length-1] = mdrive_calc_checksum(txbuf, length-1);
            txbuf[++length] = 0;

            write(fd, txbuf, length);
            tcdrain(fd);
            nanosleep(&timeout, NULL);
        }
        nanosleep(&timeout, NULL);

        for (;;) {
            // fd is NONBLOCK, so it will return immediately and set errno
            // at EAGAIN if there is no data for the read.
            if ((length = read(fd, rxbuf, sizeof rxbuf)) <= 0)
                break;
            else
                rxbuf[length] = 0;

            strtok(rxbuf, "?>");

            if (strncmp(rxbuf, &txbuf[3], 3) == 0
                    || strncmp(rxbuf, &txbuf[4], 3) == 0)
                // Echo mode is enabled (EM=0)
                continue;

            if (strlen(rxbuf) < 8)
                // Response too short to be valid
                continue;

            if (sscanf(rxbuf, "%*[^%%]%s", address)) {
                snprintf(results->port, sizeof results->port, "%s", port);
                mcTraceF(40, MDRIVE_CHANNEL, "%s: Found %s@%d:%c",
                    rxbuf, port, s->human, address[1]);
                results->address = address[1];
                results->speed = s->human;
                results++;
            }
        }
    }
    if (fd) close(fd);
    // Wait for motors to reboot
    if (results != list) sleep(1);
    return list;
}
