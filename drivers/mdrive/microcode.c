/*
 * microcode.c
 *
 * Includes routines needed to (re)load microcode onto devices as well as
 * inspect installed microcode for microcode-assisted routines available on
 * the unit.
 */
#include "mdrive.h"

#include "config.h"
#include "serial.h"

#include "lib/trace.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>

/**
 * mdrive_microcode_load
 * Driver-Entry: load_microcode
 *
 * Installs microcode from the given file onto the device specified.
 * Basically, the commands from the MCode file are reliably sent to the
 * device.
 *
 * Variables, if already in existance on the motor, cannot be cleared by the
 * CP command. Therefore, to alleviate a hard reset of the motor, variable
 * replacement is auto-sensed by this routine, and if a variable being
 * defined already exists on the device, the default value specified in the
 * new microcode is assigned as the default of the already-existing
 * variable.
 *
 * Caveats:
 * CP is executed prior to microcode loading. Any microcode previously on
 * the device will be cleared. This approach will also make split microcode
 * files impossible, because microcode is cleared for every call to this
 * routine.
 *
 * No validation is performed on the file-to-be-loaded. Error code raised by
 * the unit will be returned if the file contains bad or unsupported
 * microcode.
 *
 * Whitespace and comments are stripped from the microcode file and are not
 * sent to the unit. This routine is intented to install production code.
 *
 * This routine will take a while. IP, CP, and S are all issued to install
 * the microcode, and comm settings are re-inspected/reset after the ending
 * mdrive_config_commit() call.
 * 
 * 'S' instructions in the microcode are ignored. mdrive_config_commit() is
 * used internally after installing the entire microcode file to ensure
 * communication settings are not accidently saved incorrectly.
 *
 * Side-Effects:
 * Regardless of exit condition, device should be cleared from program mode.
 *
 * Parameters:
 * self - (Driver *) driver instance data
 * filename - (const char *) microcode file to load
 *
 * Returns:
 * (int) 0 upon success, ER_BAD_FILE if the specified filename could not
 * possibly be a microcode text file, EIO if unable to communicate with the
 * device, MDRIVE_ECLOBBER if unable to install a label or variable defined
 * in the microcode file.
 */
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
            if (ch == '\n' || ch == '\r' || ch == EOF)
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

        // XXX: Check line length. Anything over 64 chars will not be
        //      accepted by the unit

        // Write microcode to the device
        tries = 2;
        while (tries--) {
            result.code = 0;
            if (mdrive_communicate(device, bol, &opts) == RESPONSE_OK)
                break;
            else if (result.code == MDRIVE_ECLOBBER) {
                // Variable/label already exists on the device
                // See if the line starts with a 'VA ' declaration
                if (strncmp("VA ", bol, 3) != 0) {
                    status = MDRIVE_ECLOBBER;
                    goto safe_bail;
                }
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
                // TODO: Trace the offending line
                // Return actual MDrive error code if available
                status = (result.code) ? result.code : EIO;
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
            programming = (errno == 0) && (address > 0);
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

/**
 * mdrive_microcode_inspect
 *
 * Inspects the microcode loaded on the motor to sense the types of features
 * supporting microcode-assistance (for homing, moving, extra data
 * retrieval, etc.), and what the names of those labels and variables are.
 *
 * Parameters:
 * device - (mdrive_device_t *) device to inspect firmware information
 *
 * Returns:
 * (int) 0 upon success, EIO if unable to communicate with the device,
 * ENOTSUP if the microcode on the device doesn't support any extended
 * features of this driver.
 */
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
