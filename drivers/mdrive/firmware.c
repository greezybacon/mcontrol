#include "mdrive.h"

#include "config.h"
#include "driver.h"
#include "serial.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>

int
mdrive_firmware_write(mdrive_device_t * device, const char * filename) {
    static const struct timespec wait = { .tv_sec = 3 };

    mcTraceF(10, MDRIVE_CHANNEL, "Loading firmware from: %s", filename);
    
    // Attempt to open firmware file before switching into upgrade mode
    FILE * file = fopen(filename, "rt");
    if (file == NULL) {
        switch (errno) {
            case ENOENT:
            case EISDIR:
            case ENAMETOOLONG:
            case ENOTDIR:
                return ER_BAD_FILE;
            default:
                return errno;
        }
    }

    // Put device in upgrade mode
    mdrive_send(device, "UG 2956102");

    // The unit may indicate NACKs, but will not respond to PR ER
    device->ignore_errors = true;

    // Unset checksum mode and party mode -- the unit now has an address
    // of ':'
    device->checksum = CK_OFF;
    device->party_mode = false;

    // Reset the motor
    mdrive_reboot(device);

    // Change to 19200 baud
    if (mdrive_set_baudrate(device->comm, 19200) == 0)
       device->speed = 19200;

    // Reset the motor -- (which will allow reading the '$' prompt sent back
    // from the unit when rebooted in upgrade mode)
    mdrive_reboot(device);

    // Ensure the device is in upgrade mode
    if (!device->upgrade_mode)
        return EIO;

    // Prepare options for sending firmware lines
    mdrive_response_t result;
    struct mdrive_send_opts options = {
        .result = &result,      // Capture the received result
        .expect_data = true,
        .expect_err = true,     // Handle error condition here
        .waittime = &wait,      // For magic wait prescribed time
        .tries = 1,             // Don't retry
        .raw = true             // Don't add EOL
    };

    // TODO: Split firmware load into two parts, the first will read the
    // unit information (VR, SN, etc) and send then back for user
    // confirmation. The second call will actually flash the firmware.

    // Send some magic numbers, the unit responds with
    // :v -- Firmware version, hex encoded (03000D for 3.013)
    // :c
    // :p -- device Part number (PN)
    // :s -- device Serial number
    // :e -- Enter into programming mode
    char * magic_codes[] =
        { ":IMSInc\r", "::v\r", "::c\r", "::p\r", "::s\r", "::e\r", NULL };
    struct timespec waittime = { .tv_nsec=15e6 },
        longer = { .tv_nsec=250e6 };
    for (char ** magic = magic_codes; *magic; magic++) {
        do {
            nanosleep(&waittime, NULL);
            result.ack = false;
            mdrive_communicate(device, *magic, &options);
        } while (!result.ack);
    }

    // Use default timeout algorithm when sending firmware lines
    options.waittime = NULL;
    // Read data from the input file
    char ch, buffer[64], *pBuffer;
    int tries;
    do {
        // Reset the buffer position (for file reads)
        pBuffer = buffer;

        while (true) {
            ch = getc(file);
            if (ch == '\n' || ch == EOF)
                break;
            // Only accept ':' and hex chars
            else if (!(ch == ':' || isxdigit(ch)))
                continue;

            *pBuffer++ = ch;
        }

        // Skip blank lines
        if (pBuffer - buffer == 0)
            continue;

        // Data format of the buffer (without '|' chars):
        // :|10|1EB8|00|0C94A54642E00C94C1430E9468452801|51
        // ^ ^^ ^-+^ ^^ ^--------- payload ------------^ ^^
        // |  |   |  record-type                   checksum (?)
        // |  |   address
        // |  byte-count (2-char, hex byte)
        // address (1-char ':')

        // Skip records of type "03"
        else if (buffer[7] == '0' && buffer[8] == '3')
            continue;

        // Add carriage return and null-terminate
        *pBuffer++ = '\r';
        *pBuffer = 0;

        // Retry the send until we get a clear ACK from the unit. Even
        // though the unit is not in checksum mode, it will respond with an
        // ACK or NACK char to indicate receipt of the record.
        tries = 3;    // Three tries max
        while (true) {
            nanosleep(&waittime, NULL);
            result.ack = false;
            mdrive_communicate(device, buffer, &options);
            if (result.ack)
                break;
            else if (tries-- > 0) {
                nanosleep(&longer, NULL);
                waittime.tv_nsec += 1e6;
            }
            else
                return EIO;
        }

    } while (ch != EOF);
    fclose(file);

    // Restore error handling
    device->ignore_errors = false;

    // Wait for the unit to settle
    sleep(1);

    // Clear upgrade mode (mdrive_reboot will set it if the unit is still
    // in upgrade mode)
    device->upgrade_mode = false;

    // Reboot the motor (again)
    mdrive_reboot(device);

    if (device->upgrade_mode)
        return EIO;

    // Wait for the unit to settle
    sleep(1);

    // Unit is now factory defaulted. Change to default speed and re-inspect
    // comm settings
    if (mdrive_set_baudrate(device->comm, DEFAULT_PORT_SPEED) == 0)
        device->speed = DEFAULT_PORT_SPEED;

    mdrive_config_inspect(device, true);
    device->address = '!';

    // Invalidate driver cache so that a request on the original connection
    // string that hit this motor will not be reused
    mcDriverCacheInvalidate(device->driver);

    // Clear cached firmware version
    bzero(device->firmware_version, sizeof device->firmware_version);
    return 0;
}

int
mdrive_load_firmware(Driver * self, const char * filename) {
    if (self == NULL)
        return EINVAL;

    return mdrive_firmware_write(self->internal, filename);
}
