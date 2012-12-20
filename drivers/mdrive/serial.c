#include "mdrive.h"
#include "serial.h"

#include "queue.h"
#include "events.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>

// Maximum number of times to retry sending a command
static const int MAX_RETRIES = 1;

// Wait at least this long between transactions. Ie. ensure that there is a
// gap of this duration between the last tx or rx and the tx of the
// following transaction on the same comm channel
static const int MIN_TX_GAP_NSEC = 0e6;

// XXX: Use a stinkin' header file include
extern void tsAdd(const struct timespec *, const struct timespec *,
    struct timespec *);
extern long long nsecDiff(struct timespec *, struct timespec *);

static mdrive_axis_device_t * all_port_infos = NULL;

const struct baud_rate baud_rates[] = {
    { 4800,     B4800,      48 },
    { 9600,     B9600,      96 },
    { 19200,    B19200,     19 },
    { 38400,    B38400,     38 },
    { 115200,   B115200,    11 },
    { 0, 0, 0 }
};

int
mdrive_get_string(mdrive_axis_t * axis, const char * variable,
        char * value, int size) {
    char buffer[48];
    mdrive_response_t result = { .txid = 0 };

    snprintf(buffer, sizeof buffer, "PR %s", variable);

    struct mdrive_send_opts options = {
        .expect_data = true,
        .result = &result
    };
    if (mdrive_communicate(axis, buffer, &options) != RESPONSE_OK)
        return -EIO;

    return snprintf(value, size, "%s", result.buffer);
}

int
mdrive_get_integer(mdrive_axis_t * axis, const char * variable, int * value) {
    char buffer[16];
    mdrive_response_t result = { .txid = 0 };

    snprintf(buffer, sizeof buffer, "PR %s", variable);

    struct mdrive_send_opts options = {
        .expect_data = true,
        .result = &result
    };

    if (mdrive_communicate(axis, buffer, &options) != RESPONSE_OK)
        return EIO;

    errno = 0;
    char * nondigit;
    *value = strtol(result.buffer, &nondigit, 10);

    if (*nondigit != 0)
        // Garbage in the resulting buffer
        return EIO;

    // TODO: Return some error indication
    return errno;
}

/**
 * mdrive_get_integers
 *
 * Receives several variables from the unit with one command. The return
 * type for each value is assumed to be (int).
 */
int
mdrive_get_integers(mdrive_axis_t * axis, const char * vars[],
    int * values[], int count) {

    char buffer[64], * pbuf = buffer;
    int cValues = count;
    mdrive_response_t result = { .txid = 0 };

    pbuf += snprintf(buffer, sizeof buffer, "PR %s", *vars++);
    while (--count)
        pbuf += snprintf(pbuf, sizeof buffer + pbuf - buffer,
            ",\" \",%s", *vars++);

    struct mdrive_send_opts options = {
        .expect_data = true,
        .result = &result
    };
    if (mdrive_communicate(axis, buffer, &options) != RESPONSE_OK)
        return EIO;

    errno = 0;
    pbuf = result.buffer;
    while (cValues--) {
        // Load the next number into the next int * and increment the int *
        // array index. Use strtol() to calculate the position of the
        // end-of-this-/beginning-of-the-next number.
        **values++ = strtol(pbuf, &pbuf, 10);
        if (errno == EINVAL)
            return EIO;
    }
    return 0;
}

/**
 * mdrive_get_error
 *
 * Basically identical to mdrive_get_integer, except that the response
 * classification from the unit is not considered. When the error flag is
 * set on the unit, it will send NACKs back on all commands -- including the
 * request to retrieve the current ERror code. Therefore, when retrieving
 * the error code, we will have to ignore the error flag or the error flag
 * in the response or we'll (likely) recurse back through here or not
 * properly consider the response of the command.
 *
 * Returns:
 * (int) error code currently set on the unit
 */
int
mdrive_get_error(mdrive_axis_t * axis) {
    int code;
    mdrive_response_t result = { .txid = 0 };

    // This will also clear the error flag, if set
    struct mdrive_send_opts options = {
        .expect_data = true,
        .result = &result,
        .expect_err = true      // Don't retry on error condition
    };
    mdrive_communicate(axis, "PR ER", &options);
    errno = 0;
    code = strtol(result.buffer, NULL, 10);
    if (errno != EINVAL)
        return code;

    return -EIO;
}

/**
 * mdrive_clear_error
 *
 * Clears error flag on the device by sending "ER" to the device.
 */
void
mdrive_clear_error(mdrive_axis_t * axis) {
    mdrive_send(axis, "ER");
}

/**
 * mdrive_calc_checksum
 *
 * According to the book, the checksum of the Mdrive serial data is defined
 *
 * When [CK is] enabled, all communications with the device require a Check
 * Sum to follow the commands. The Check Sum is the 2’s complement of the 7
 * bit sum of the ASCII value of all the characters in the command  “OR”ed
 * with 128 (hex = 0x80)
 *
 * Returns:
 * (char) the checksum character (8-bit value)
 */
char
mdrive_calc_checksum(const char * buffer, int length) {
    unsigned char checksum = 0;
    while (length--)
        checksum += *buffer++;
    return (~checksum + 1) | 128;
}

/**
 * mdrive_check_checksum
 *
 * Checks the checksum in the received buffer against the calculated
 * checksum for the buffer
 * 
 * Returns:
 * (bool) TRUE if the checksum is correct, FALSE otherwise
 */
bool
mdrive_check_checksum(const char * buffer, int length, char checksum) {
    if (length < 1)
        return false;
    else if ((checksum & 128) != 128)
        return false;

    return checksum == mdrive_calc_checksum(buffer, length);
}

/**
 * mdrive_classify_response
 *
 * Interprets the response from a completed command from the unit. If an
 * error condition is detected, but no code was indicated by the unit in the
 * transmission, the error will be read automatically from the unit.
 * Furthermore, if an error condition exists, it will always be cleared and
 * signaled via the mdrive_signal_error() routine.
 *
 * Returns:
 * (enum mdrive_response_class) OK if the response was clean, ERROR if the
 * response was clean but indicated an error on the unit, RETRY if the error
 * of the unit indicated it was unable to process the command, NACK if the
 * unit refused the command (probably because of comm noise).
 *
 * UNKNOWN is returned if the unit's response doesn't fit into any
 * yet-classified response.
 */
int
mdrive_classify_response(mdrive_axis_t * axis, mdrive_response_t * response) {
    // If the unit is in checksum mode, a good checksum in the response with
    // an ACK will indicate clear success. If a NACK is present with a good
    // checksum, an error exists on the unit. Otherwise, NACK indicates a
    // clear failure in processing the command.
    if (axis->checksum) {
        // Require no data returned or good_checksum with the returned data
        if ((response->length > 0) == response->checksum_good) {
            if (response->ack)
                return RESPONSE_OK;

            // Error exists on the unit. This one is tricky. If a checksum
            // is receved, then an error definitely exists. Otherwise, it is
            // unclear if the command was not processed by the unit or if an
            // error exists on the unit after the command was received.
            // Either way, we need to query the device for the error.
            else if (response->nack)
                response->error = true;
                // Fall through to [if (response->error)] case

            // If a prompt is received here, then the unit is not in
            // checksum mode (anymore). This will happen when the unit is
            // transitioning out of checksum mode. Fall through and search
            // through the non-checksum cases below
        }
        else if (response->nack)
            return RESPONSE_NACK;
        else if (!response->checksum_good)
            return RESPONSE_BAD_CHECKSUM;
    }

    // If an error is indicated by the response, clear the error and signal
    // the error to create an event
    if (response->error) {
        if (!response->code) {
            // Don't recurse again to find this
            if (axis->txnest == 1 && !axis->ignore_errors)
                response->code = mdrive_get_error(axis);
        }
        else
            // Unit sent the error code, so clear the error on the unit
            mdrive_clear_error(axis);

        if (response->code) {
            mdrive_signal_error_event(axis, response->error);
            if (response->code == MDRIVE_EOVERRUN)
                return RESPONSE_RETRY;
            else
                return RESPONSE_ERROR;
        }
        else if (response->nack)
            return RESPONSE_NACK;

        // Else: erroneous error indication
    }

    // If the unit sends a standard prompt (>), then the request was
    // correctly received and processed. On Manchac custom firmware, EM=1
    // will still send the prompt; however, on stock firmware, only the CRLF
    // will be transmitted.
    else if (response->prompt || response->crlf)
        return RESPONSE_OK;

    else if (response->nack && response->length == 0
            && axis->checksum == CK_OFF)
        // Device is _really_ in checksum mode, and this is still an
        // unexpected response
        axis->checksum = CK_ON;

    return RESPONSE_UNKNOWN;
}

/**
 * mdrive_process_response
 *
 * Processes the partial response from a unit. This function can/should be
 * called multiple times with different buffers and the same response. Each
 * time, the response will be processed to either the perceived end of
 * transmission indicated in the response, or a null character, whichever is
 * found first. If the response is believed to be complete, the ->processed
 * member of the response will be set.
 *
 * Returns:
 * (int) number of characters processed in the received buffer.
 */
int
mdrive_process_response(char * buffer, mdrive_response_t * response,
        int length) {
    if (length < 1)
        return 0;

    // TODO: Detect if line is an event indication
    // Which will be !["address"]?[event_code] with no brackets
    // TODO: Handle solitary ACK followed by event ('!') ...
    // Setup a pointer to the end of the response buffer
    char * target = response->buffer + response->length;
    char * bufc = buffer, * start;
    while (length-- && !response->processed) {
        switch (*bufc) {
            case '\n':
                // End of transmission marker -- the motor will send CRLF,
                // so the LF part will mark the absolute end of transmission
                // NOTE: In some cases, the unit will send the prompt char
                //       ('>') after the trailing newline (when EM=0 for
                //       instance). If this is the case, we need to consume
                //       the prompt char too). The same is true for the
                //       error prompt char ('?')
                if (*(bufc-1) == '\r') {
                    response->processed = true;
                    if (*(bufc+1) == '>') {
                        response->prompt = true;
                        bufc++;
                    }
                    else if (*(bufc+1) == '?') {
                        response->error = true;
                        bufc++;
                    }
                    else if (*(bufc+1) == '\x06') {
                        // If a procedure as EX'd and it printed something,
                        // in checksum mode, the ACK will follow the CRLF
                        bufc++;
                    }
                    // Stock firmwares will not send the prompt in EM=1;
                    // however, CRLF (\r\n) indicates command accepted.
                    // TODO: Check firmware version
                    response->crlf = true;
                    break;
                }
                // Doesn't seem to be a control char -- add it to the
                // received buffer
                goto normal_char;
            case '\r':
                // Marks the (almost) end of transmission. The significance
                // of the CR char is that only the chars that preceed it
                // should be considered in the checksum validation.
                // Furthermore, the previous char is the checksum and is
                // also not considered in the checksum.
                //
                // So the transmission started with an [N]ACK. Then the
                // actual data followed, then the checksum char, and then
                // the CR char. So we need to calculate the checksum from
                // after the ACK to before the checksum char.
                break;
            case '\x06':
                response->ack = true;
                response->ack_location = bufc - buffer + response->received;
            case '\x15':
                if (*bufc == '\x15')
                    response->nack = true;

                // The ACK/NACK char was not the first byte received. If
                // echo mode is enabled for the device sending this data,
                // the previous byte would be the checksum char and the
                // buffer to the left of that should match the checksum. If
                // this is true, then echo mode is very likely enabled
                // (EM=0) for the device and the data received is the echo.
                //
                // This case is also encountered when the unit is in
                // checksum mode and an extraneous PR statement is run from
                // within the motor firmware code (as for events).
                if (response->event) {
                    // Ack will mark the end of the event
                    response->processed = true;
                    if (response->checksum_good) {
                        // Drop the (good) checksum from the received buffer
                        target--;
                        response->length--;
                    }
                } else if (response->length) {
                    if (*(bufc-1) == '\n') {
                        // Checksum was before ACK/NACK -- echo mode
                        // enabled.  Drop the received buffer.
                        target = response->buffer;
                        response->length = 0;
                        response->echo = true;
                    }
                }
                // XXX: Else we have a problem -- an ACK/NACK was received
                //      neither at the beginning of the tranmission nor
                //      after the checksum of the echo-ed response
                break;
            case '?':
                // Error code follows
                response->in_error = true;
                response->error = true;
                break;
            case '>':
                // Prompts only occur immediately before the \r or
                // immediately after the \n. The latter case is handled in
                // the '\n' block.  NOTE: This case only occurs in the
                // Manchac custom firmware with EM=1
                if (*(bufc+1) == '\r') {
                    response->prompt = true;
                    break;
                }
                // in EM=0, the prompt can carry over from the previous
                // response
                else if (response->length == 0)
                    break;
                goto normal_char;
            case '!':
                // If received as the first char, this is an event. The
                // address follows (quoted), then '?' followed by the event
                // code.
                if (response->length == 0) {
                    response->event = true;
                    break;
                }
                goto normal_char;
            case '"':
                // Marks the start/end of the device name string for events
                if (response->event && !response->address && !response->length) {
                    response->address = *(++bufc);
                    bufc++; // Skip the trailing "
                    break;
                }
            default:
                // Detect and parse checksum chars
                if ((*bufc & 128) && !response->checksum_good) {
                    if (response->ack_location)
                        start = buffer - response->received
                            + response->ack_location;
                    else
                        start = buffer - response->received
                            + (response->ack | response->nack ? 1 : 0);
                    if (mdrive_check_checksum(start, bufc - start, *bufc)) {
                        // Checksum matches. Don't add it to the receive 
                        // buffer
                        response->checksum_good = true;
                        break;
                    }
                }

normal_char:
                // Continue with response error code processing
                if (response->in_error) {
                    if (*bufc >= '0' && *bufc <= '9') {
                        // Thanks, http://stackoverflow.com/a/868508
                        response->code =
                            response->code * 10 + (*bufc - '0');
                        break;
                    } else {
                        response->in_error = false;
                    }
                }
                
                // Copy the char into the response buffer
                *target++ = *bufc;
                response->length++;

                // Detect overflow of receive buffer (garbage input most
                // likely)
                if (response->length == sizeof response->buffer - 1)
                    response->processed = true;
        }
        bufc++;
    }
    // Handle strange responses when in upgrade mode
    if (response->length == 1 && response->buffer[0] == '$')
        // Reboot sends an single '$' char
        response->processed = true;

    // Return number of chars processed
    response->received += bufc - buffer;
    return bufc - buffer;
}

/**
 * mdrive_async_read
 *
 * Should be executed in a separate thread for each port. The procedure will
 * received data from all units on the port and will signal the device's
 * ->has_data condition member when a response has been placed on the queue
 * for that device.
 *
 * Should more than one response be received for one transaction id, the
 * best attempt will be made to queue all the items related to the
 * transaction before signaling the has_data condition.
 */
void *
mdrive_async_read(void * arg) {
    mdrive_axis_device_t * dev = arg;
    mdrive_response_t * response = calloc(1, sizeof *response);

    // Wait time for a single char at the device speed
    // XXX: dev->speed is allowed to be changed on the fly
    struct timespec now,
        onechartime = { .tv_nsec = mdrive_xmit_time(dev, 4) };

    int length;
    char buffer[512];
    // Where the load and process pieces are in the buffer space
    char * load = buffer, * process = buffer;
    int txid = 0;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(dev->fd, &fds);

    while (true) {

        // Try to read more than one char at a time
        nanosleep(&onechartime, NULL);
        length = read(dev->fd, load, sizeof buffer - (int)(load - buffer));

        // If the txid's changed while this thread was asleep, scratch the
        // received data
        if (txid && txid != dev->txid)
            bzero(response, sizeof *response);
        txid = dev->txid;

        // Don't acquire txlock because it would create a deadlock. Just
        // make sure writes to the dev members are atomic.
        clock_gettime(CLOCK_REALTIME, &now);
        dev->lastActivity = now;

        if (length == -1) {
            mcTraceF(1, MDRIVE_CHANNEL_RX, "Recieved error: %d", errno);
            if (errno == 9)
                // Bad file number -- fd is not opened properly
                break;
            else
                continue;
        }

        // TODO: Keep track of the time the first byte was received for this
        // response. After some amount of time (say 1 sec), we can close the
        // response because there's no way the bytes go with the same
        // transmission.

        mcTraceBuffer(50, MDRIVE_CHANNEL_RX, load, length);

        load += length;     // Advance load pointer to end of input
        *load = 0;          // Null-terminate

        while (process < load) {
            // (Continue to) process the response, advance the process pointer
            process += mdrive_process_response(process, response, load-process);

            // If the message was acked, wait for the rest of the message to
            // roll in.
            if (response->ack || response->nack || response->processed) {
                response->buffer[response->length] = 0;

                if (response->event) {
                    // TODO: Signal the event
                    mdrive_signal_event_device(dev, response->address, response->code);
                // If the process and load pointers match up, then we've
                // processed all the current input and can reset the
                // pointers
                } else if (process == load) {
                    process = load = buffer;

                    pthread_mutex_lock(&dev->rxlock);
                    // Record the transaction id
                    response->txid = dev->txid;
                    queue_push(&dev->queue, response);
                    // Signal that data is ready
                    pthread_cond_signal(&dev->has_data);
                    pthread_mutex_unlock(&dev->rxlock);
                } else {
                    free(response);
                }
                response = calloc(1, sizeof *response);
            }
        }
    }
    return NULL;
}

/**
 * mdrive_combine_response
 *
 * Used where a command expects a response, but the unit is slow to respond.
 * In such a case, the unit will generally ACK the receipt of the command
 * and send the response sometime later. Since the receive thread doesn't
 * currently know about the transmit thread's activity, it will submit two
 * separate responses -- one with the ack and the second with the payload.
 * This routine will simply combine the vital pieces of the two responses
 * into the first response.
 */
int
mdrive_combine_response(mdrive_response_t * a, mdrive_response_t * b) {
    if (a->txid != b->txid)
        // Problem
        return -1;

    // Combine status flags
    a->ack = a->ack | b->ack;
    a->nack = a->nack | b->nack;
    a->prompt = a->prompt | b->prompt;
    a->crlf = a->crlf | b->crlf;
    a->error = a->error | b->error;
    a->code = (b->code) ? b->code : a->code;
    a->checksum_good = a->checksum_good | b->checksum_good;

    // Combine buffers and counters
    a->received += b->received;
    a->length += snprintf(a->buffer + a->length, sizeof a->buffer - a->length,
        "%s", b->buffer);

    return 0;
}

/**
 * mdrive_write_buffer
 *
 * Sends the requested buffer to the requested device and updates device
 * statistics concerning the device timings and statistics. The routine will
 * block until the data has bee flushed to the device in order to eliminate
 * the transmission tx timing from the rx timeout.
 *
 * This routine will ensure that a minimum transmission space is maintained
 * for the device to assist in keeping reboots and overflows to a minimum. 
 *
 * Returns:
 * (int) - 0 upon success
 *
 */
int
mdrive_write_buffer(mdrive_axis_t * axis, const char * buffer, int length) {
    struct timespec txwait, now,
        txspacetime = { .tv_nsec = MIN_TX_GAP_NSEC };

    mcTraceBuffer(50, MDRIVE_CHANNEL_TX, buffer, length);

    // NOTE: There needs to be at least a 10-20ms space between sends on
    // the wire. We need to wait here until it's safe to send more data.
    // The device maintains a time of last send in the lasttx member of
    // the device structure.
    clock_gettime(CLOCK_REALTIME, &now);
    tsAdd(&axis->device->lastActivity, &txspacetime, &txwait);

    // Set baudrate speed on axis->device to axis->speed. This allows motors
    // on the same comm channel to be at different speeds. This doesn't make
    // great sense for production units; however, it make diagnostics and
    // motor setup much easier.
    mdrive_set_baudrate(axis->device, axis->speed);

    // Don't bother checking if [txwait] is in the past, because
    // clock_nanosleep() will too. No need checking twice
    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &txwait, NULL);

    int pos = 0, written;
    // Write data to the file descriptor. If interrupted and not all data
    // was written out, loop and continue writing the rest of the
    // transmission
    do {
        written = write(axis->device->fd, buffer + pos, length - pos);
        if (written == -1)
            return errno;
        pos += written;
    } while (pos < length);

    // There's no point in considering the transmission time in the
    // receive timeout
    tcdrain(axis->device->fd);

    // Log the time of last transmission
    clock_gettime(CLOCK_REALTIME, &axis->device->lasttx);
    axis->device->lastActivity = axis->device->lasttx;

    // Update device statistics
    axis->stats.tx++;
    axis->stats.txbytes += length;

    return 0;
}

/**
 * mdrive_communicate
 *
 * Send a message to the device and wait for the response. If unable to
 * communicate with the unit, the transmission will be automatically retried
 * up to MAX_RETRIES times.
 *
 * The timeout period for the received response is autosensed at 55ms. This
 * value will float as the current latency of the unit plus 40ms. If an
 * incomplete response is received, an additional 25ms + the time to receive
 * the rest of the buffer (62 chars) will be added to the timeout time. In
 * order to make use of the delayed response detection, set the
 * <expects_data> flag.
 *
 * Parameters:
 * axis - Mdrive device to communicate with
 * command - (const char *) Command to send to the device. The command
 *     should be considered raw -- that is, without the device address
 *     prefixed or the CR or LF suffixed. Null termination is also assumed
 * options - (struct mdrive_send_opts) a struct is used to make adding
 *          additional, optional parameters easier {
 *     expect_data - (bool) If the driver should expect a response from the
 *         device (other that the usual prompt, ACK, or NACK). Refer to the
 *         paragraph above concerning the transmission timeouts.
 *     result - (mdrive_response_t *) to receive the details of the
 *         transmission, including the received data (buffer) and meta data
 *         (ACK, NACK, error code, error flag, etc.)
 *     waittime - (struct timespec *) non-standard waittime to be used with
 *         awaiting a response from a unit. This can be used for commands
 *         with an unusual waittime such as [S], [FD], or soft reset [^C].
 *         If NULL, the default waiting algorithm will be used to determine
 *         the device timeout
 *     expect_err - (bool) error indication by the unit is considered ok
 *     raw - (bool) omit the trailing \r or \n char
 *     tries - (short) attempt the transmission this number of times (rather
 *          than the default value of 1 + MAX_RETRIES)
 * }
 *
 * Returns:
 * (enum mdrive_response_class) classification of the response. See the
 * documentation of mdrive_classify_response for details.
 */
int
mdrive_communicate(mdrive_axis_t * axis, const char * command,
        const struct mdrive_send_opts * options) {

    int i, status=0, length, txid;
    char buffer[63];
    mdrive_response_t * response = NULL;

    // Split the receive timeout in half. The first timeout will await the
    // first char from the device (ACK if in checksum mode), and the second
    // timeout will be for the rest of the data payload. This will allow
    // short commands to timeout early and longer commands can have the
    // additional wait time (requires checksum mode and responds == true)
    int onechartime = mdrive_xmit_time(axis->device, 1);
    struct timespec now, timeout, first_waittime = { .tv_sec = 0 },
        more_waittime = { .tv_nsec = (int)25e6 + onechartime * 62 };

    if (axis->party_mode)
        // NOTE: Dec the buffer size to save room for the checksum byte
        length = snprintf(buffer, sizeof buffer-1, "%c%s%s", axis->address,
            command, (options->raw) ? "" : "\n");
    else
        length = snprintf(buffer, sizeof buffer-1, "%s%s", command,
            (options->raw) ? "" : "\r");

    if (axis->checksum) {
        if (options->raw) {
            buffer[length] = mdrive_calc_checksum(buffer, length);
        } else {
            // Move the CR or LF over one char and insert the checksum char.
            // Note that the length includes doubles of the CR or LF char,
            // neither of which should be included in the checksum calc
            buffer[length] = buffer[length-1];
            buffer[length-1] = mdrive_calc_checksum(buffer, length-1);
        }
        buffer[++length] = 0;
    }

    // Use 55ms waittime if no waittime is specified in the options
    if (axis->stats.latency == 0 || axis->echo == EM_QUIET)
        // Start with a reasonable value (15ms)
        axis->stats.latency = (int)15e6;
    if (options->waittime)
        first_waittime = *options->waittime;
    else
        // Allow for a 40ms deviation in latency
        first_waittime.tv_nsec = axis->stats.latency + (int)40e6;

    // The [txlock] is really more of a transaction lock, so it should be
    // held the entire time communicating to the device or waiting or
    // processing a response from the device. Ultimately, no other devices
    // on this comm channel should be communicated with while the command is
    // issued to this device
    pthread_mutex_lock(&axis->device->txlock);

    // If txnest == 0, flush the device queue since there are no other
    // expected responses (?)
    if (axis->txnest == 0) {
        pthread_mutex_lock(&axis->device->rxlock);
        queue_flush(&axis->device->queue);
        pthread_mutex_unlock(&axis->device->rxlock);
    }

    // Detect send/receive recursions -- used by automatic error detection
    axis->txnest++;

    // Use prescribed number of tries or the default
    i = (options->tries) ? options->tries : 1 + MAX_RETRIES;
    while (i--) {

        // Store in current stack frame for distinction against recursive
        // calls to mdrive_communicate...(). txid is incremented for every
        // transmit to incidate that any previously-received data is now
        // invalid and does not belong to this transaction.
        txid = ++axis->device->txid;

        if (mdrive_write_buffer(axis, buffer, length))
            return RESPONSE_IOERROR;

        clock_gettime(CLOCK_REALTIME, &now);
        tsAdd(&now, &first_waittime, &timeout);

        // If axis is not in checksum mode, add the more_waittime to the
        // timeout to since the unit will not send the early ACK of the
        // command receipt.
        if (!axis->checksum)
            tsAdd(&timeout, &more_waittime, &timeout);

wait_longer:
        pthread_mutex_lock(&axis->device->rxlock);

        while (!queue_length(&axis->device->queue)) {
            if (ETIMEDOUT == pthread_cond_timedwait(&axis->device->has_data,
                    &axis->device->rxlock, &timeout)) {

                pthread_mutex_unlock(&axis->device->rxlock);
                if (axis->echo == EM_QUIET && !options->expect_data)
                    // No response from unit. If the unit is EM=2
                    // (EM_QUIET), this is likely just a command with no
                    // response, which indicates success -- even if checksum
                    // is enabled
                    return RESPONSE_OK;

                // Non-responsive unit
                axis->stats.timeouts++;
                mcTraceF(30, MDRIVE_CHANNEL_RX, "Timed out: %d", axis->echo);
                // XXX: response if existing is not classified and status is
                //      left at default value of 0 (RESPONSE_OK)
                status = RESPONSE_TIMEOUT;
                goto resend;
            }
        }

        // Combine previously-received response
        if (response) {
            mdrive_response_t * response2 = queue_pop(&axis->device->queue);
            mdrive_combine_response(response, response2);
            free(response2);
        } else {
            response = queue_pop(&axis->device->queue);
            // Record latency of the first response. Average over 32 xmits
            clock_gettime(CLOCK_REALTIME, &now);
            axis->stats.latency = 
                  ((31 * axis->stats.latency) >> 5)
                + ((
                       // Don't consider receive time in latency
                       nsecDiff(&now, &axis->device->lasttx)
                     - response->received * onechartime
                  ) >> 5);
        }

        pthread_mutex_unlock(&axis->device->rxlock);

        if (!response)
            goto resend;

        mcTraceF(40, MDRIVE_CHANNEL_RX,
            "Received: (%s%s%s%s%s%s:%d) %d:%s",
                response->ack ? "A" : "",
                response->nack ? "N" : "",
                response->checksum_good ? "C" : "",
                response->echo ? "E" : "",
                (response->prompt || response->crlf) ? ">" : "",
                response->error ? "?" : "",
                response->code,
                response->length,
                response->buffer);

        if (response->txid != txid) {
            // Response is not for the transaction expected
            mcTraceF(10, MDRIVE_CHANNEL, "Invalid TXID received: %d / %d",
                txid, response->txid);
            goto resend;
        }

        if (options->expect_data && !response->length && 
                (response->ack | response->nack | response->crlf)) {
            // Handle embedded error condition if one exists
            mdrive_classify_response(axis, response);

            // A response was expected but not received -- wait longer. Add
            // the additional wait time to the timeout and wait longer.
            // However, if the unit indicated an error already, then we're
            // finished.
            if (response->code)
                break;
            mcTrace(30, MDRIVE_CHANNEL_RX, "Waiting longer ...");
            tsAdd(&timeout, &more_waittime, &timeout);
            goto wait_longer;
        }
        else if (options->expect_data && axis->echo == EM_ON
                && response->length) {
            // In echo mode, then unit will send the request back in the
            // response. On [swift OSes], the response will be closed after
            // the echo is received, but before the real data is received.
            // Be careful not to consider the \r, \n, >, ? or checksum
            // chars, as none of them will exist in the response buffer
            if (strncmp(response->buffer, buffer, response->length) == 0) {
                // Clear the buffer and set the echo flag
                *response->buffer = 0;
                response->length = 0;
                response->echo = true;
                tsAdd(&timeout, &more_waittime, &timeout);
                goto wait_longer;
            }
        }
            
        if (response->ack) axis->stats.acks++;
        if (response->nack) axis->stats.nacks++;

        status = mdrive_classify_response(axis, response);
        
        if (status == RESPONSE_RETRY) {
            axis->stats.overflows++;
        } else if (status == RESPONSE_OK || options->expect_err) {
            // Error was handled in the response_classify routine (if there
            // was one)
            break;
        } else {
            if (status == RESPONSE_BAD_CHECKSUM)
                axis->stats.bad_checksums++;
            else if (status == RESPONSE_UNKNOWN)
                mcTrace(20, MDRIVE_CHANNEL_RX, "UNKNOWN response received");
            // Wait and retry the transmission
        }
        // Sleep to timeout
        // clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &timeout, NULL);
resend:
        // Clear previous response if retrying
        if (response) {
            axis->stats.rx++;
            axis->stats.rxbytes += response->received;

            free(response);
            response = NULL;
        }
        axis->stats.resends++;
    } // end while (i--)

    if (response) {
        axis->stats.rx++;
        axis->stats.rxbytes += response->received;

        // Copy out response to the caller
        if (options->result)
            *options->result = *response;
        free(response);
    }
    else if (i == 0) {
        // Motor refused to ACK the command (no response)
        mcTraceF(10, MDRIVE_CHANNEL, "Out of retries: %d, %d", i, status);
        status = -EIO;
    }

    axis->txnest--;

    pthread_mutex_unlock(&axis->device->txlock);

    mcTraceF(50, MDRIVE_CHANNEL_RX, "Status is %d", status);
    return status;
}

int mdrive_send(mdrive_axis_t * axis, const char * command) {
    struct mdrive_send_opts opts = {
        .expect_data = false,
        .result = NULL
    };
    return mdrive_communicate(axis, command, &opts);
}

int
mdrive_connect(mdrive_address_t * address, mdrive_axis_t * axis) {
    // Transfer the motor's address ('a' for instance)
    axis->address = address->address;
    // Address '!' can't be set, so unit is not in party mode if set.
    axis->party_mode = address->address != '!';

    // Axis is declared to operate at ->speed. This is important to set here
    // even if the axis is reusing a connected comm channel, because
    // axes sharing a comm channel can operate at different speeds. This
    // will definitely be the case when naming or reinitializing motors
    axis->speed = address->speed;

    // Search for axis sharing an in-use port
    mdrive_axis_device_t * current_port = all_port_infos;

    while (current_port) {
        if (strncmp(current_port->name, address->port,
                sizeof(current_port->name)) == 0) {
            // This device is already initialized. Just link it to the axis
            axis->device = current_port;
            current_port->active_axes++;
            return 0;
        }
        current_port = current_port->next;
    }

    // New axis and new port. Set things up
    mdrive_axis_device_t * new_port = calloc(1, sizeof *all_port_infos);
    if (current_port)
        current_port->next = new_port;
    new_port->active_axes++;
    snprintf(new_port->name, sizeof new_port->name, "%s", address->port);

    if (!all_port_infos) all_port_infos = new_port;

    new_port->fd = mdrive_initialize_port(new_port->name, address->speed, true);
    mdrive_set_baudrate(new_port, address->speed);

    if (new_port->fd < 0)
        return new_port->fd;
    axis->device = new_port;

    pthread_mutex_init(&new_port->rxlock, NULL);
    pthread_cond_init(&new_port->has_data, NULL);

    // Allow reentry on the txlock
    pthread_mutexattr_t attrs;
    pthread_mutexattr_init(&attrs);
    pthread_mutexattr_settype(&attrs, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&new_port->txlock, &attrs);

    // Setup the device RX queue (well, stack really)
    mdrive_response_queue_init(&new_port->queue);

    // Start async receive on the device
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&new_port->read_thread, &attr, mdrive_async_read,
        (void *) new_port);

    // Mark now as the time of last transmission -- just so it isn't zero
    clock_gettime(CLOCK_REALTIME, &new_port->lasttx);

    return 0;
}

void
mdrive_disconnect(mdrive_axis_t * axis) {
    mdrive_axis_device_t * device = axis->device;

    // Decrement active_axes counter for this device and cleanup the device
    // itself if there are no more active motors on this device
    if (--device->active_axes != 0)
        return;

    pthread_cancel(device->read_thread);

    pthread_mutex_destroy(&device->rxlock);
    pthread_mutex_destroy(&device->txlock);
    pthread_cond_destroy(&device->has_data);

    // tcsetattr(fd, TCSAFLUSH, &device->termios);
    close(device->fd);

    // XXX: Free items retrieved from the queue
    while (queue_length(&device->queue))
        queue_pop(&device->queue);

    // Drop device from the list of port infos
    if (device == all_port_infos) {
        // device is the head of the all_port_infos list, so advance
        // all_port_infos
        all_port_infos = device->next;
    }
    else {
        // Find where device falls in the list and relink the list so that
        // device isn't in it
        mdrive_axis_device_t * current_port = all_port_infos;
        while(current_port) {
            if (current_port->next == device) {
                current_port->next = device->next;
                break;
            }
            current_port = current_port->next;
        }
    }
    free(device);
}

int
mdrive_initialize_port(const char * port, int speed, bool async) {
    int fd;
    struct termios tty;

    fd = open(port, O_RDWR | O_NOCTTY | (async ? 0 : O_NONBLOCK));

    if (fd < 0) {
        mcTraceF(1, MDRIVE_CHANNEL, "Unable to open tty: %s", port);
        return fd;
    }

    tcgetattr(fd, &tty);
    // TODO: Save tty settings in device for reset at closure

    tty.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
    tty.c_cflag |= CS8 | CREAD | CLOCAL;
    tty.c_iflag &= ~ISTRIP & ~ICRNL;
    tty.c_iflag |= IGNPAR | IGNBRK;
    tty.c_oflag &= ~OPOST;                          // Raw output processing
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // Raw input processing

    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSAFLUSH, &tty)) {
        mcTraceF(1, MDRIVE_CHANNEL, "Unable to configure tty: %d", errno);
        return -1;
    }

    return fd;
}

/**
 * mdrive_set_baudrate
 *
 * Configures the comm channel for a motor device to the (human) speed
 * requested. If the channel is already thought to be at the requested
 * speed, the function will return without issuing the new baudrate.
 *
 * Parameters:
 * device - (mdrive_axis_device_t *) communication channel to receive the
 *      new baud rate
 * speed - (int) baud rate, human readable (9600 for B9600 baud rate)
 *
 * Returns:
 * (int) ENOTSUP for invalid speed setting, 0 upon successful transition
 */
int
mdrive_set_baudrate(mdrive_axis_device_t * device, int speed) {
    const struct baud_rate * s;

    for (s=baud_rates; s->human; s++)
        if (s->human == speed)
            break;
    if (s->human == 0)
        // Bad speed specified
        return ENOTSUP;

    // If device is operating at the requested speed already, just bail
    if (device->speed == speed)
        return 0;

    struct termios tty;

    tcgetattr(device->fd, &tty);

    cfsetispeed(&tty, s->constant);
    cfsetospeed(&tty, s->constant);

    int status = tcsetattr(device->fd, TCSAFLUSH, &tty);

    if (status == 0)
        device->speed = speed;

    return status;
}

/**
 * mdrive_xmit_time
 *
 * Returns:
 * (int) the time in nanoseconds to transmit or receive one character at the
 * current device speed.
 */
inline int
mdrive_xmit_time(mdrive_axis_device_t * device, int chars) {
    // The Mdrive units only support 8N1 mode, so 1 start-bit and 1 stop-bit
    // means a total of 10-bit bytes.
    return chars * ((int)1e9 / (device->speed / 10));
}
