#include "driver.h"
#include "mdrive.h"

#include "serial.h"
#include "config.h"
#include "query.h"
#include "motion.h"
#include "events.h"
#include "firmware.h"
#include "microcode.h"

#include <stdio.h>
#include <time.h>
#include <regex.h>
#include <errno.h>

int MDRIVE_CHANNEL, MDRIVE_CHANNEL_TX, MDRIVE_CHANNEL_RX,
    MDRIVE_CHANNEL_FW;

/**
 * mdrive_init -- (DriverClass::initialize)
 *
 * Connect to and initialize a motor identified by the received connection
 * string.
 *
 * The connection string should be formatted something like
 * [mdrive://]/dev/ttyS0[@115200][:a]
 *
 * where the [mdrive://] portion has already been removed by the
 * higher-level driver controller. The speed will default to 9600 if
 * unspecified, and the address [:a] will default to none if unspecified.
 */
int mdrive_init(Driver * self, const char * cxn) {
    static regex_t re_cxn;
    // XXX: Allow leading / trailing whitespace ?
    static const char * regex = "^([^@:]+)(@[0-9]+)?(:[*!a-zA-Z0-9^])?$";

    regmatch_t matches[4];
    mdrive_address_t address;
    int status;

    self->internal = calloc(1, sizeof(mdrive_device_t));
    if (self->internal == NULL)
        // Indicate out-of-memory condition
        return -ENOMEM;

    // Parse connection string
    if (!re_cxn.re_nsub)
        regcomp(&re_cxn, regex, REG_EXTENDED);

    if ((status = regexec(&re_cxn, cxn, 4, matches, 0) != 0)) {
        // XXX: Set error condition somewhere
        mcTraceF(10, MDRIVE_CHANNEL, "Bad connection string: %d", status);
        return EINVAL;
    }

    snprintf(address.port,
        min(sizeof address.port, matches[1].rm_eo - matches[1].rm_so + 1),
        "%s", cxn + matches[1].rm_so);

    if (matches[3].rm_so > 0)
        address.address = *(cxn + matches[3].rm_so + 1);
    else
        address.address = '!';

    if (matches[2].rm_so > 0)
        address.speed = strtol(cxn + matches[2].rm_so + 1, NULL, 10);
    else
        address.speed = DEFAULT_PORT_SPEED;

    mdrive_device_t * device = self->internal;
    if (mdrive_connect(&address, device) != 0)
        // XXX: Set some error indication (or set it in mdrive_connect)
        return -1;

    // Write the device address into the driver "name"
    snprintf(self->name, 2, "%c", address.address);

    // Link the motor back to the driver (for event callbacks, etc.)
    device->driver = self;

    // XXX: Move to mdrive_connect
    if (mdrive_config_inspect(device, true))
        return ER_COMM_FAIL;

    return 0;
}

void mdrive_uninit(Driver * self) {
    if (self == NULL)
        return;

    mdrive_device_t * motor = self->internal;

    // Configure device for debugging (user usage)
    mdrive_set_checksum(motor, CK_OFF, false);
    mdrive_set_echo(motor, EM_ON, false);

    mdrive_disconnect(motor);

    free(self->internal);
}

int mdrive_reboot(mdrive_device_t * device) {
    static struct cmd_wait {
        const char *    text;
        int             wait;
    } cmds[] = {
        { "\x1b", 0 },
        { "\x03", 950e6 },
        { NULL, 0 }
    };
    mdrive_response_t result;
    struct timespec waittime;
    struct mdrive_send_opts options = {
        .expect_data = false,
        .result = &result,
        .expect_err = true,
        .raw = true
    };
    // Don't retry the reboot in party-mode (the unit won't give any
    // indication of acceptance)
    if (device->party_mode)
        options.tries = 1;

    for (struct cmd_wait * cmd = cmds; cmd->text; cmd++) {
        if (cmd->wait) {
            waittime = (struct timespec) { .tv_nsec = cmd->wait };
            options.waittime = &waittime;
        }
        else
            options.waittime = NULL;

        mdrive_communicate(device, cmd->text, &options);

        // Sense firmware upgrade mode
        if (result.buffer[0] == '$')
            device->upgrade_mode = true;
    }

    return 0;
}

int mdrive_reset(Driver * self) {
    mdrive_device_t * device = self->internal;
    return mdrive_reboot(device);
}

int
mdrive_search(char * cxns, int size) {
    char ** port = mdrive_enum_serial_ports(), ** port_head=port;
    mdrive_address_t * results, * head;
    int count = 0, wr;

    while (*port) {
        head = results = mdrive_enum_motors_on_port(*port);
        while (results->speed) {
            if (results->address)
                wr = snprintf(cxns, size, "%s@%d:%c",
                    results->port, results->speed, results->address);
            else
                wr = snprintf(cxns, size, "%s@%d",
                    results->port, results->speed);
            results++;
            count++;
            size -= ++wr;
            cxns += wr;
        }
        free(head);
        free(*port);
        port++;
    }
    free(port_head);
    return count;
}

DriverClass mdrive_driver = {
    .name = "mdrive",
    .description = "Schneider MDrive / MForce",
    .revision = "A1",

    .initialize = mdrive_init,
    .destroy = mdrive_uninit,
    .search = mdrive_search,

    .move = mdrive_move,
    .stop = mdrive_stop,
    .reset = mdrive_reset,
    .home = mdrive_home,

    .read = mdrive_read_variable,
    .write = mdrive_write_variable,

    .notify = mdrive_notify,
    .unsubscribe = mdrive_unsubscribe,

    .load_firmware = mdrive_load_firmware,
    .load_microcode = mdrive_microcode_load,
};

void __attribute__((constructor))
_mdrive_driver_init(void) {
    mcDriverRegister(&mdrive_driver);

    MDRIVE_CHANNEL = mcTraceChannelInit(CHANNEL);
    MDRIVE_CHANNEL_RX = mcTraceChannelInit(CHANNEL_RX);
    MDRIVE_CHANNEL_TX = mcTraceChannelInit(CHANNEL_TX);
    MDRIVE_CHANNEL_FW = mcTraceChannelInit(CHANNEL_FW);
}
