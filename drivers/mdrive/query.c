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

static struct query_variable query_xref[] = {
    { 9, MCPOSITION,        "P",    NULL,   mdrive_write_simple },
    { 9, MCVELOCITY,        "V",    NULL,   NULL },
    { 1, MCACCELERATING,    "VC",   NULL,   NULL },
    { 1, MCMOVING,          "MV",   NULL,   NULL },
    { 3, MCINPUT,           "I%d",  NULL,   NULL },
    { 6, MCOUTPUT,          "O%d",  NULL,   NULL },

    // Profile peeks
    { 5, MCACCEL,           NULL,   mdrive_profile_peek, NULL },
    { 5, MCDECEL,           NULL,   mdrive_profile_peek, NULL },
    { 5, MCVMAX,            NULL,   mdrive_profile_peek, NULL },
    { 5, MCVINITIAL,        NULL,   mdrive_profile_peek, NULL },
    { 5, MCDEADBAND,        NULL,   mdrive_profile_peek, NULL },
    { 5, MCRUNCURRENT,      NULL,   mdrive_profile_peek, NULL },
    { 5, MCHOLDCURRENT,     NULL,   mdrive_profile_peek, NULL },
    { 5, MCSLIPMAX,         NULL,   mdrive_profile_peek, NULL },

    { 4, MDRIVE_IO_TYPE,    "S%d",  NULL,   NULL },
    { 4, MDRIVE_IO_INVERT,  "S%d",  NULL,   NULL },
    { 4, MDRIVE_IO_DRIVE,   "S%d",  NULL,   NULL },

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
    for (q = query_xref; q->type; q++)
        if (q->query == query->query)
            break;

    mdrive_axis_t * motor = self->internal;
    switch (q->type) {
        case 1:
        case 9:
            if (mdrive_get_integer(motor, q->variable, &intval))
                return -EIO;
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
                return -EIO;
            query->number = intval;
            break;
        case 5:
            if (q->read)
                return q->read(motor, query, q);
        default:
            return -ENOTSUP;
    }
    return 0;
}

int
mdrive_write_variable(Driver * self, struct motor_query * query) {
    struct query_variable * q;
    for (q = query_xref; q->type; q++)
        if (q->query == query->query)
            break;

    if (q->write == NULL)
        return -ENOTSUP;
    
    return q->write(self->internal, query, q);
}

int
mdrive_write_simple(mdrive_axis_t * axis, struct motor_query * query,
        struct query_variable * q) {

    mdrive_response_t resp;
    char cmd[16];
    switch (q->type) {
        case 1:
            snprintf(cmd, sizeof cmd, "%s=%lld", q->variable, query->number);
            break;
        case 2:
            snprintf(cmd, sizeof cmd, "%s=%s", q->variable, query->string);
            break;
        default:
            return -ENOTSUP;
    }

    struct mdrive_send_opts options = {
        .expect_data = false,
        .result = &resp
    };
    if (RESPONSE_OK != mdrive_communicate(axis, cmd, &options))
        return -EIO;

    return 0;
}

static int
mdrive_address_poke(mdrive_axis_t * axis, struct motor_query * query,
        struct query_variable * q) {
    if (axis == NULL)
        return -EINVAL;

    return mdrive_config_set_address(axis, *query->string);
}

static int
mdrive_bd_peek(mdrive_axis_t * axis, struct motor_query * query,
        struct query_variable * q) {
    if (axis == NULL)
        return -EINVAL;
    
    const struct baud_rate * s;
    for (s=baud_rates; s->human; s++)
        if (s->human == axis->speed)
            break;
    if (s->human == 0)
        // Speed is not corrent -- unknown?
        return -EIO;

    query->number = s->human;
    return 0;
}

static int
mdrive_bd_poke(mdrive_axis_t * axis, struct motor_query * query,
        struct query_variable * q) {
    if (axis == NULL)
        return -EINVAL;

    return mdrive_config_set_baudrate(axis, query->number);
}

static int
mdrive_checksum_poke(mdrive_axis_t * axis, struct motor_query * query,
        struct query_variable * q) {
    if (axis == NULL)
        return -EINVAL;
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
        return -EINVAL;

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
        return -EINVAL;

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
        return -EINVAL;

    if (*axis->firmware_version == 0)
        mdrive_get_string(axis, q->variable, axis->firmware_version,
            sizeof axis->firmware_version);

    return snprintf(query->string, sizeof query->string, "%s",
        axis->firmware_version);
}

static int
mdrive_profile_peek(mdrive_axis_t * axis, struct motor_query * query,
        struct query_variable * q) {
    if (axis == NULL)
        return -EINVAL;

    mdrive_lazyload_profile(axis);

    switch (query->query) {
        case MCACCEL:
            query->number = axis->profile.accel.raw;
            break;
        case MCDECEL:
            query->number = axis->profile.decel.raw;
            break;
        case MCVINITIAL:
            query->number = axis->profile.vstart.raw;
            break;
        case MCVMAX:
            query->number = axis->profile.vmax.raw;
            break;
        case MCRUNCURRENT:
            query->number = axis->profile.current_run;
            break;
        case MCHOLDCURRENT:
            query->number = axis->profile.current_hold;
            break;
        case MCSLIPMAX:
            query->number = axis->profile.slip_max.raw;
            break;
        default:
            return -EINVAL;
    }
    return 0;
}
