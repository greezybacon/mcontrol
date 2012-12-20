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
    struct timespec longtime = { .tv_nsec = 900e6 };
    struct mdrive_send_opts opts = { .waittime = &longtime };
    if (mdrive_communicate(axis, "CP", &opts) != RESPONSE_OK)
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
            else if (ch == '\x27')
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
        if (strncmp("S", buffer, 2) == 0)
            continue;

        // Write microcode to the device
        // XXX: Technically, the unit will send the address of the
        //      next-received command. A nice, extra check would be to make
        //      sure the response received can be processed as an integer
        if (mdrive_communicate(axis, buffer, &opts) != RESPONSE_OK)
            // XXX: Abandon programming mode first!
            return EIO;

    } while (ch != EOF);

    // Commit the new microcode and settings to NVRAM
    if (!mdrive_config_commit(axis))
        return EIO;

    return 0;
}

int
mdrive_microcode_inspect(mdrive_axis_t * axis) {
    mdrive_response_t resp;
    struct mdrive_send_opts opts = {
        .result = &resp,
        .expect_data = true,
        .expect_err = true      // Error 30 handled here
    };

    if (mdrive_communicate(axis, "EX CF", &opts))
        return EIO;

    else if (resp.code == MDRIVE_ENOLABEL)
        return ENOTSUP;

    // Response string is formatted as follows
    //
    // <ver> ... <move> <fe>
    //
    // Where:
    // ver - (int) microcode interface version number
    // move - (label) of the move routine
    // fe - (variable) of the following error if computed
    //
    // Any of the variables and labels can be set to "-" to indicate that
    // the microcode doesn't support such a feature

    int version;
    char * head;
    version = strtol(resp.buffer, &head, 10);

    if (version == -1)
        return EIO;

    axis->microcode.version = version;
    mcTraceF(50, MDRIVE_CHANNEL, "Unit has microcode version %d", version);
    
    if (head == NULL)
        return EIO;

    head++;     // Skip the space char following the version
    char * next = strtok_r(head, " ", &head);
    if (version >= 0x0001) {
        // Move label
        if (next) {
            snprintf(axis->microcode.labels.move,
                sizeof axis->microcode.labels.move, "%s", next);
            if (*axis->microcode.labels.move != '-')
                axis->microcode.features.move = true;
        }

        // Following error variable
        next = strtok_r(head, " ", &head);
        if (next) {
            snprintf(axis->microcode.labels.following_error,
                sizeof axis->microcode.labels.following_error, "%s", next);
            if (*axis->microcode.labels.following_error != '-')
                axis->microcode.features.following_error = true;
        }
    }
    mcTraceF(50, MDRIVE_CHANNEL, "Unit uses move label %s",
        axis->microcode.labels.move);
    mcTraceF(50, MDRIVE_CHANNEL, "Unit uses fe var %s",
        axis->microcode.labels.following_error);

    return 0;
}
