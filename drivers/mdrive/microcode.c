#include "mdrive.h"

#include "config.h"
#include "serial.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>

int
mdrive_microcode_load(Driver * self, const char * filename) {
    mdrive_device_t * device = self->internal;

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
    if (!mdrive_config_rollback(device))
        return EIO;

    // Clear stored microcode
    struct timespec longtime = { .tv_nsec = 900e6 };
    struct mdrive_send_opts opts = { .waittime = &longtime };
    if (mdrive_communicate(device, "CP", &opts) != RESPONSE_OK)
        return EIO;

    // Read data from the input file
    char ch, buffer[64], *bol, *eol;
    bool skipchar = false;

    // Handle errors (like 28 -- ECLOBBER) here
    int tries;
    struct mdrive_response result;
    opts.result = &result;

    // Config vars not to be reset at [S]ave time
    struct mdrive_config_flags preserve = {0};

    // Keep track of entry- and exit from program mode
    bool programming = false;

    // Exit status
    int status = 0;

    do {
        // Reset the buffer position (for file reads)
        eol = buffer;
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
                *eol++ = ch;
        }
        // Null-terminate
        *eol = 0;

        // Advance line past initial whitespace
        bol = buffer;
        while (bol < eol && isspace(*bol))
            bol++;

        // Skip blank lines
        if (*bol == 0)
            continue;

        // Trim trailing whitespace
        while (eol > bol && isspace(*(eol-1)))
            eol--;

        // Null-terminate the command buffer (again)
        *eol = 0;

        // Don't send auto-save because it will save current communication
        // settings. Use mdrive_config_commit() instead
        if (strncmp("S", bol, 2) == 0)
            continue;

        // Write microcode to the device
        // XXX: Technically, the unit will send the address of the
        //      next-received command. A nice, extra check would be to make
        //      sure the response received can be processed as an integer
        tries = 2;
        while (tries--) {
            result.code = 0;
            if (mdrive_communicate(device, bol, &opts) == RESPONSE_OK)
                break;
            else if (result.code == MDRIVE_ECLOBBER) {
                // Variable/label already exists on the device
                // See if the line starts with a 'VA ' declaration
                if (strncmp("VA ", bol, 3) != 0)
                    return MDRIVE_ECLOBBER;
                // See if microcode has a default value set
                else if (strchr(bol, '=') == NULL)
                    // No default value set in microcode
                    break;
                // Send default value from microcode instead
                else if (mdrive_communicate(device, bol+3, &opts) == RESPONSE_OK)
                    break;
                else {
                    status = EIO;
                    goto safe_bail;
                }
            }
            if (tries == 0) {
                // XXX: Abandon programming mode first!
                status = EIO;
                goto safe_bail;
            }
        }
        if (strncmp("EM", bol, 2) == 0)
            preserve.echo = true;
        else if (strncmp("CK", bol, 2) == 0)
            preserve.checksum = true;
        else if (strncmp("PG", bol, 2) == 0) {
            // See if there is an address after the 'PG'
            errno = 0;
            int address = strtol(bol+2, NULL, 10);
            if (errno == 0)
                programming = address > 0;
        }

    } while (ch != EOF);

    if (programming)
        // Bogus microcode -- it entered program mode but didn't exit.
        // TODO: Come up with some nicely-worded error code to return
        if (RESPONSE_OK == mdrive_send(device, "PG"))
            programming = false;

    // Commit the new microcode and settings to NVRAM
    if (mdrive_config_commit(device, &preserve)) 
        goto exit;
    else
        status = EIO;

safe_bail:
    if (programming)
        mdrive_send(device, "PG");

exit:
    return status;
}

int
mdrive_microcode_inspect(mdrive_device_t * device) {
    mdrive_response_t resp;
    struct mdrive_send_opts opts = {
        .result = &resp,
        .expect_data = true,
        .expect_err = true      // Error 30 handled here
    };

    // No need to inspect global connections
    if (device->address == '*')
        return 0;

    if (mdrive_communicate(device, "EX CF", &opts))
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

    device->microcode.version = version;
    mcTraceF(50, MDRIVE_CHANNEL, "Unit has microcode version %d", version);
    
    if (head == NULL)
        return EIO;

    head++;     // Skip the space char following the version
    char * next = strtok_r(head, " ", &head);
    if (version >= 0x0001) {
        // Move label
        if (next) {
            snprintf(device->microcode.labels.move,
                sizeof device->microcode.labels.move, "%s", next);
            if (*device->microcode.labels.move != '-')
                device->microcode.features.move = true;
        }

        // Following error variable
        next = strtok_r(head, " ", &head);
        if (next) {
            snprintf(device->microcode.labels.following_error,
                sizeof device->microcode.labels.following_error, "%s", next);
            if (*device->microcode.labels.following_error != '-')
                device->microcode.features.following_error = true;
        }
    }
    mcTraceF(50, MDRIVE_CHANNEL, "Unit uses move label %s",
        device->microcode.labels.move);
    mcTraceF(50, MDRIVE_CHANNEL, "Unit uses fe var %s",
        device->microcode.labels.following_error);

    return 0;
}
