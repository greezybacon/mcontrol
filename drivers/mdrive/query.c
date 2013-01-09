#include "mdrive.h"
#include "serial.h"

#include "query.h"
#include "motion.h"
#include "config.h"
#include "profile.h"

#include <stdio.h>
#include <errno.h>

// Define all the internal peek/poke functions so they're in scope for the
// peek/poke table below.
static POKE(mdrive_address_poke);
static POKE(mdrive_bd_poke);
static PEEK(mdrive_bd_peek);
static POKE(mdrive_name_poke);
static POKE(mdrive_checksum_poke);
static PEEK(mdrive_sn_peek);
static PEEK(mdrive_vr_peek);
static PEEK(mdrive_pn_peek);
static PEEK(mdrive_profile_peek);
static POKE(mdrive_ee_poke);
static PEEK(mdrive_var_peek);
static POKE(mdrive_var_poke);
static POKE(mdrive_ex_poke);
static POKE(mdrive_io_poke);

static struct query_variable query_xref[] = {
    { 9, MCPOSITION,        "P",    NULL,   mdrive_write_simple },
    { 9, MCVELOCITY,        "V",    NULL,   NULL },
    { 1, MCACCELERATING,    "VC",   NULL,   NULL },
    { 1, MCMOVING,          "MV",   NULL,   NULL },
    { 1, MCSTALLED,         "ST",   NULL,   mdrive_write_simple },
    { 3, MCINPUT,           "I%d",  NULL,   NULL },
    { 19, MCOUTPUT,         "O%d",  NULL,   mdrive_write_simple },

    // Profile peeks
    { 5, MCACCEL,           NULL,   mdrive_profile_peek, NULL },
    { 5, MCDECEL,           NULL,   mdrive_profile_peek, NULL },
    { 5, MCVMAX,            NULL,   mdrive_profile_peek, NULL },
    { 5, MCVINITIAL,        NULL,   mdrive_profile_peek, NULL },
    { 5, MCDEADBAND,        NULL,   mdrive_profile_peek, NULL },
    { 5, MCRUNCURRENT,      NULL,   mdrive_profile_peek, NULL },
    { 5, MCHOLDCURRENT,     NULL,   mdrive_profile_peek, NULL },
    { 5, MCSLIPMAX,         NULL,   mdrive_profile_peek, NULL },
    { 1, MDRIVE_ENCODER,    "EE",   NULL,   mdrive_ee_poke },

    { 1, MDRIVE_VARIABLE,   NULL,   mdrive_var_peek, mdrive_var_poke },
    { 20, MDRIVE_EXECUTE,   NULL,   NULL, mdrive_ex_poke },

    { 4, MDRIVE_IO_TYPE,    "S%d",  NULL,   mdrive_io_poke },
    { 4, MDRIVE_IO_PARM1,   "S%d",  NULL,   mdrive_io_poke },
    { 4, MDRIVE_IO_PARM2,   "S%d",  NULL,   mdrive_io_poke },

    { 5, MDRIVE_SERIAL,     "SN",   mdrive_sn_peek, NULL },
    { 5, MDRIVE_PART,       "PN",   mdrive_pn_peek, NULL },
    { 5, MDRIVE_FIRMWARE,   "VR",   mdrive_vr_peek, NULL },
    { 2, MDRIVE_MICROCODE,  "AA",   NULL,   NULL },
    { 5, MDRIVE_BAUDRATE,   "BD",   mdrive_bd_peek, mdrive_bd_poke },
    { 1, MDRIVE_CHECKSUM,   "CK",   NULL,   mdrive_checksum_poke },
    { 1, MDRIVE_ECHO,       "EM",   NULL,   NULL },

    { 2, MDRIVE_ADDRESS,    "DN",   NULL,   mdrive_address_poke },
    { 6, MDRIVE_NAME,       NULL,   NULL,   mdrive_name_poke },

    { 6, MDRIVE_RESET,      NULL,   NULL,   NULL },
    { 6, MDRIVE_HARD_RESET, "FD",   NULL,   NULL },

    { 0, 0, NULL, NULL, NULL }
};

int
mdrive_read_variable(Driver * self, struct motor_query * query) {
    struct query_variable * q;
    char variable[4];
    int intval;

    if (query == NULL)
        return EINVAL;

    for (q = query_xref; q->type; q++)
        if (q->query == query->query)
            break;

    mdrive_axis_t * motor = self->internal;
    switch (q->type) {
        case 1:
        case 9:
            if (mdrive_get_integer(motor, q->variable, &intval))
                return EIO;
            if (q->type == 9)
                query->number = mdrive_steps_to_microrevs(motor, intval);
            else
                query->number = intval;
            break;
        case 2:
            return mdrive_get_string(motor, q->variable,
                query->string, sizeof query->string);
        case 3:
            snprintf(variable, sizeof variable, q->variable, query->arg.number);
            if (mdrive_get_integer(motor, variable, &intval))
                return EIO;
            query->number = intval;
            break;
        case 5:
            if (q->read)
                return q->read(motor, query, q);
        default:
            return ENOTSUP;
    }
    return 0;
}

/**
 * mdrive_write_entry
 * Driver-Entry: write
 *
 * Used to poke special values into the unit. The driver is chiefly designed
 * for motion and motion events. For extraneous information about the motor,
 * peeks and pokes are used to get and retrieve characteristics of the
 * motor. This is the generic write implementation.
 */
int
mdrive_write_variable(Driver * self, struct motor_query * query) {
    if (self == NULL || query == NULL)
        return EINVAL;

    struct query_variable * q;
    for (q = query_xref; q->type; q++)
        if (q->query == query->query)
            break;

    if (q->write == NULL)
        return ENOTSUP;

    return q->write(self->internal, query, q);
}

int
mdrive_write_simple(mdrive_axis_t * axis, struct motor_query * query,
        struct query_variable * q) {

    char cmd[16], variable[16];
    int value = query->number;
    switch (q->type & 0x0f) {
        case 9:
            // Convert first, then fall through to set integer
            value = mdrive_microrevs_to_steps(axis, value);
            // If setting the position, keep it internally
            if (query->query == MCPOSITION)
                axis->position = value;
        case 1:
            snprintf(cmd, sizeof cmd, "%s=%d", q->variable, value);
            break;
        case 2:
            snprintf(cmd, sizeof cmd, "%s=%s", q->variable, query->string);
            break;
        case 3:
            // XXX: for MCOUTPUT, ensure the unit supports the output given
            snprintf(variable, sizeof variable, q->variable, query->arg.number);
            snprintf(cmd, sizeof cmd, "%s=%d", variable, query->number);
            break;
        default:
            return ENOTSUP;
    }

    if (RESPONSE_OK != mdrive_send(axis, cmd))
        return EIO;

    return 0;
}

static int
mdrive_address_poke(mdrive_axis_t * axis, struct motor_query * query,
        struct query_variable * q) {
    if (axis == NULL)
        return EINVAL;

    return mdrive_config_set_address(axis, *query->string);
}

static int
mdrive_bd_peek(mdrive_axis_t * axis, struct motor_query * query,
        struct query_variable * q) {
    if (axis == NULL)
        return EINVAL;

    const struct baud_rate * s;
    for (s=baud_rates; s->human; s++)
        if (s->human == axis->speed)
            break;
    if (s->human == 0)
        // Speed is not corrent -- unknown?
        return EIO;

    query->number = s->human;
    return 0;
}

static int
mdrive_bd_poke(mdrive_axis_t * axis, struct motor_query * query,
        struct query_variable * q) {
    if (axis == NULL)
        return EINVAL;

    return mdrive_config_set_baudrate(axis, query->number);
}

static int
mdrive_checksum_poke(mdrive_axis_t * axis, struct motor_query * query,
        struct query_variable * q) {
    if (axis == NULL)
        return EINVAL;
    return mdrive_set_checksum(axis, query->number, false);
}

static int
mdrive_name_poke(mdrive_axis_t * axis, struct motor_query * query,
        struct query_variable * q) {

    // For this mode, we assume that any of the devices to be named are in
    // default modes (non-party), and the serial number is sent as the
    // argument of the query
    if (axis == NULL)
        return EINVAL;

    // Since we're likely talking to more than one unit, turn off
    // unsolicited responses
    // -- Assume checksum is on
    axis->checksum = CK_ON;
    mdrive_set_checksum(axis, CK_OFF, false);
    mdrive_set_echo(axis, EM_QUIET, true);

    // Upload the naming routine
    char buffer[64];
    char * routine[] = {
        "ER",                       // Clear any current error
        "CP N",                     // Clear any existing 'N' routine
        "PG 100",
        "LB N",
            "BR N2, SN <> %1$s",    // All other motors skip
            "DN = %2$d ' %1$1.1s",  // Set device name (2nd arg, 1st req'd)
            "PY = 1",               // Enable party mode
        "LB N2",
        "E",                        // Program ends here
        "PG",                       // Exit program mode
        NULL
    };

    for (char ** line = routine; *line; line++) {
        snprintf(buffer, sizeof buffer, *line,
            query->arg.string, (unsigned int)query->string[0]);
        mdrive_send(axis, buffer);
    }

    // Wait just a second
    struct timespec waittime = { .tv_nsec = 600e6 };
    nanosleep(&waittime, NULL);

    // Call the naming routine now that we're done
    mdrive_send(axis, "EX N");

    // Now, assume that the axis address is changed.  Leave the axis address
    // as the global one for further naming. To communicate with the renamed
    // axis, a separate connection will be required.
    mdrive_axis_t fake_axis = *axis;
    fake_axis.address = query->string[0];
    fake_axis.party_mode = true;

    // Activate party mode and enable command acceptance on the new axis
    mdrive_set_echo(&fake_axis, EM_PROMPT, false);

    // Attempt to read the serial number
    if (0 > mdrive_get_string(&fake_axis, "SN", buffer, sizeof buffer))
        return EIO;

    if (strcmp(buffer, query->arg.string) != 0)
        return EIO;

    // Everything looks good. Clear the naming routine and save the settings
    // on the newly-named axis
    struct mdrive_send_opts opts = { .waittime = &waittime };
    mdrive_communicate(&fake_axis, "CP N", &opts);
    mdrive_config_commit(&fake_axis);

    return 0;
}

static int
mdrive_sn_peek(mdrive_axis_t * axis, struct motor_query * query,
        struct query_variable * q) {
    if (axis == NULL)
        return EINVAL;

    if (*axis->serial_number == 0)
        mdrive_get_string(axis, q->variable, axis->serial_number,
            sizeof axis->serial_number);

    return snprintf(query->string, sizeof query->string, "%s",
        axis->serial_number);
}

static int
mdrive_pn_peek(mdrive_axis_t * axis, struct motor_query * query,
        struct query_variable * q) {
    if (axis == NULL)
        return EINVAL;

    if (*axis->part_number == 0)
        mdrive_get_string(axis, q->variable, axis->part_number,
            sizeof axis->part_number);

    return snprintf(query->string, sizeof query->string, "%s",
        axis->part_number);
}

static int
mdrive_vr_peek(mdrive_axis_t * axis, struct motor_query * query,
        struct query_variable * q) {
    if (axis == NULL)
        return EINVAL;

    if (*axis->firmware_version == 0)
        mdrive_get_string(axis, q->variable, axis->firmware_version,
            sizeof axis->firmware_version);

    return snprintf(query->string, sizeof query->string, "%s",
        axis->firmware_version);
}

static int
mdrive_ee_poke(mdrive_axis_t * axis, struct motor_query * query,
        struct query_variable * q) {
    if (axis == NULL)
        return EINVAL;

    if (axis->encoder == query->number)
        return 0;

    int status = mdrive_write_simple(axis, query, q);
    if (status)
        return status;

    // Reload motion configuration from the unit
    axis->loaded.encoder = false;
    return 0;
}

static int
mdrive_profile_peek(mdrive_axis_t * axis, struct motor_query * query,
        struct query_variable * q) {
    if (axis == NULL)
        return EINVAL;

    mdrive_lazyload_profile(axis);

    switch (query->query) {
        case MCACCEL:
            query->number = axis->profile.accel.value;
            break;
        case MCDECEL:
            query->number = axis->profile.decel.value;
            break;
        case MCVINITIAL:
            query->number = axis->profile.vstart.value;
            break;
        case MCVMAX:
            query->number = axis->profile.vmax.value;
            break;
        case MCRUNCURRENT:
            query->number = axis->profile.current_run;
            break;
        case MCHOLDCURRENT:
            query->number = axis->profile.current_hold;
            break;
        case MCSLIPMAX:
            query->number = axis->profile.slip_max.value;
            break;
        default:
            return EINVAL;
    }
    return 0;

}

static int
mdrive_ex_poke(mdrive_axis_t * axis, struct motor_query * query,
        struct query_variable * q) {

    char buffer[64];
    snprintf(buffer, sizeof buffer, "EX %2.2s", query->string);

    return mdrive_send(axis, buffer);
}

static int
mdrive_var_peek(mdrive_axis_t * axis, struct motor_query * query,
        struct query_variable * q) {

    int value;
    if (mdrive_get_integer(axis, query->arg.string, &value))
        return EIO;

    query->number = value;
    return 0;
}

static int
mdrive_var_poke(mdrive_axis_t * axis, struct motor_query * query,
        struct query_variable * q) {

    char buffer[64];
    snprintf(buffer, sizeof buffer, "%2.2s=%lld", query->string,
        query->arg.number);

    if (mdrive_send(axis, buffer))
        return EIO;

    return 0;
}

/**
 * mdrive_lazyload_io
 *
 * Lazily loads the device's configuration information concerning IO ports
 * from the unit. The first time the function is called after a device's
 * reboot or this driver is loaded, the configuration will be loaded from
 * the device. Otherwise, the function will return immediately.
 *
 * Returns:
 * 0 upon success. EIO if unable to communicate with the device.
 */
int
mdrive_lazyload_io(mdrive_axis_t * device) {
    if (device->loaded.io)
        return 0;

    struct mdrive_response result;
    struct mdrive_send_opts opts = {
        .expect_data = true,
        .result = &result
    };

    if (mdrive_communicate(device,
            "PR S1,\":\",S2,\":\",S3,\":\",S4,\":\",S5", &opts))
        return EIO;

    // TODO: Process response and set local info into device->io[]

    return 0;
}

/**
 * mdrive_io_poke
 *
 * Used to facilitate IO configuration setup and changes. Changes are synced
 * to the device immediately if the type has been set for the IO port. In
 * other words, to minimize communication with the device, set the type of
 * the port last.
 *
 * Returns:
 * EINVAL if IO number is less than "1". ENOTSUP if the device does not
 * support the specified IO port, EINVAL if the type specified is not
 * supported by the specified IO port, EINVAL if setting parm2 for an analog
 * input, EIO if unable to send configuration to the device. 0 is returned
 * upon success.
 */
static int
mdrive_io_poke(mdrive_axis_t * axis, struct motor_query * query,
        struct query_variable * q) {

    // XXX: Some units sport more than 5 IOs. Check the model number of the
    //      unit here
    int port = query->arg.number;
    if (port < 1)
        return EINVAL;
    else if (port > 5)
        return ENOTSUP;

    mdrive_lazyload_io(axis);
    struct mdrive_io_config io = axis->io[port-1];

    switch (query->query) {
        case MDRIVE_IO_TYPE:
            if (port == 5) {
                switch (query->number) {
                    case IO_ANALOG_VOLTAGE:
                    case IO_ANALOG_CURRENT:
                        io.type = query->number;
                        break;
                    default:
                        return EINVAL;
                }
            } else {
                switch (query->number) {
                    case IO_OUTPUT:
                    case IO_MOVING:
                    case IO_FAULT:
                    case IO_STALL:
                    case IO_DELTA_V:
                    case IO_MOVING_ABS:
                        io.output = true;
                        // Fall through to general configuration
                    case IO_INPUT:
                    case IO_HOME:
                    case IO_LIMIT_POS:
                    case IO_LIMIT_NEG:
                    case IO_G0:
                    case IO_SOFT_STOP:
                    case IO_PAUSE:
                    case IO_JOG_POS:
                    case IO_JOG_NEG:
                    case IO_RESET:
                        io.type = query->number;
                        break;
                    default:
                        return EINVAL;
                }
            }
            break;

        case MDRIVE_IO_PARM1:
            if (query->number == 5)
                io.wide_range = (bool) query->number;
            else
                io.active_high = (bool) query->number;
            break;

        case MDRIVE_IO_PARM2:
            if (query->number == 5)
                return EINVAL;
            else
                io.source = (bool) query->number;
            break;

        default:
            return EINVAL;
    }
    // Send configuration to the device
    char buffer[64];
    if (port == 5) {
        // IO type must be set in order to configure the analog input
        if (!io.type)
            return 0;
        snprintf(buffer, sizeof buffer, "S%d=%d,%d",
            port, io.type, io.wide_range ? 1 : 0);
    } else
        snprintf(buffer, sizeof buffer, "S%d=%d,%d,%d",
            port, io.type, io.active_high ? 1 : 0,
            io.source ? 1 : 0);

    if (mdrive_send(axis, buffer))
        return EIO;

    return 0;
}
