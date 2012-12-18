#include "mdrive.h"

#include "config.h"
#include "serial.h"

#include <errno.h>
#include <stdio.h>

int
mdrive_microcode_load(Driver * self, const char * filename) {
    mdrive_axis_t * axis = self->internal;

    mcTraceF(10, MDRIVE_CHANNEL, "Loading microcode from: %s", filename);
    
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

    // Reset any unsaved changes (comm configuration, etc)
    if (!mdrive_config_rollback(axis))
        return EIO;

    // Clear stored microcode
    if (mdrive_send(axis, "CP") != RESPONSE_OK)
        return EIO;

    // Read data from the input file
    char ch, buffer[64], *pBuffer;
    bool skipchar = false;
    do {
        // Reset the buffer position (for file reads)
        pBuffer = buffer;
        // New line, don't skip anything until an apostrophe is found
        skipchar = false;

        while (true) {
            ch = getc(file);
            if (ch == '\n' || ch == EOF)
                break;

            // Skip comments (apostrophe to end of line)
            else if (ch == '\x34')
                skipchar = true;

            if (!skipchar)
                *pBuffer++ = ch;
        }

        // Skip blank lines
        if (pBuffer - buffer == 0)
            continue;

        // Null-terminate the command buffer
        *pBuffer = 0;

        // Don't send auto-save because it will save current communication
        // settings. Use mdrive_config_commit() instead
        if (strcmp(buffer, "S") == 0)
            continue;

        // Write microcode to the device
        // XXX: Technically, the unit will send the address of the
        //      next-received command. A nice, extra check would be to make
        //      sure the response received can be processed as an integer
        if (mdrive_send(axis, buffer) != RESPONSE_OK)
            // XXX: Abandon programming mode first!
            return EIO;

    } while (ch != EOF);

    // Commit the new microcode and settings to NVRAM
    if (!mdrive_config_commit(axis))
        return EIO;

    return 0;
}
