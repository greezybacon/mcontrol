#include "mdrive.h"

#include "config.h"

#include "driver.h"
#include "serial.h"

#include <stdio.h>
#include <errno.h>

/**
 * mdrive_config_rollback
 *
 * Rollback any unsaved (uncommitted) changes made to the unit. This will
 * also clear the configured echo and checksum settings, so those will be
 * auto re-discovered after the configuration is reset
 *
 * Returns:
 * (bool) - TRUE upon success, FALSE otherwise
 */
bool
mdrive_config_rollback(mdrive_axis_t * device) {
    // Reset all settings
    if (mdrive_send(device, "IP") != RESPONSE_OK)
        return false;

    // Inspect the reset configuration, but don't set anything new
    if (mdrive_config_inspect(device, true))
        return false;
    
    return true;
}

/**
 * mdrive_config_commit
 *
 * Sends the save command (S) to commit changes to NVRAM
 *
 * Parameters:
 * device - (mdrive_axis_t *) Device to commit config changes
 *
 * Returns:
 * (bool) - TRUE if commit was completed successfully and FALSE otherwise
 */
bool
mdrive_config_commit(mdrive_axis_t * device) {
    // Place comm settings back to user-friendly ones before saving
    mdrive_set_checksum(device, CK_OFF, false);
    mdrive_set_echo(device, EM_ON, false);

    struct timespec timeout = { .tv_nsec = 750e6 };
    struct mdrive_send_opts options = {
        .expect_data = false,
        .waittime = &timeout
    };

    if (mdrive_communicate(device, "S", &options) != RESPONSE_OK)
        return false;

    return mdrive_config_inspect(device, true) == 0;
}

bool
mdrive_set_variable_string(mdrive_axis_t * axis, const char * variable,
        const char * value) {
    char cmd[16];

    snprintf(cmd, sizeof cmd, "%s=%s", variable, value);

    if (RESPONSE_OK != mdrive_send(axis, cmd))
        return false;

    return true;
}

bool
mdrive_set_variable(mdrive_axis_t * axis, const char * variable, int value) {
    char val[13];

    snprintf(val, sizeof val, "%d", value);

    return mdrive_set_variable_string(axis, variable, val);
}

bool
mdrive_set_checksum(mdrive_axis_t * axis, checksum_mode_t mode, bool force) {
    if (!force && axis->checksum == mode)
        // No need to change it on the motor
        return true;

    short old = axis->checksum;

    // The unit only thinks about the response. Therefore, when setting
    // checksum, the unit will immediately respond with ACK; however, when
    // unsetting checksum, the checksum char will need to be sent, because
    // the unit will still be in checksum mode when it processes the
    // request. When it sends the response; however, there will be no ACK.
    if (mode != CK_OFF)
        axis->checksum = mode;

    if (mdrive_set_variable(axis, "CK", mode)) {
        axis->checksum = mode;
        return true;
    }

    axis->checksum = old;
    return false;
}

bool
mdrive_set_echo(mdrive_axis_t * axis, echo_mode_t mode, bool force) {
    if (!force && axis->echo == mode)
        // No need to change it on the motor
        return true;

    short old = axis->echo;

    axis->echo = mode;
    if (mdrive_set_variable(axis, "EM", mode))
        return true;

    axis->echo = old;
    return false;
}

int
mdrive_config_set_baudrate(mdrive_axis_t * axis, int speed) {
    const struct baud_rate * selected;

    for (selected=baud_rates; selected->setting; selected++)
        if (speed == selected->human)
            break;
    if (selected->human == 0)
        // Unsupported baud rate setting
        return ENOTSUP;

    // Make sure we don't save anything unexpected
    mdrive_config_rollback(axis);

    if (!mdrive_set_variable(axis, "BD", selected->setting))
        return EIO;

    if (!mdrive_config_commit(axis)) // Saves the configuration
        return EIO;

    // Reboot the unit for BD to take effect; however we won't be able to
    // talk to the unit because we'll still be at the wrong speed
    mdrive_reboot(axis);            
                                    
    // NOTE: If the mdrive_set_baudrate fails, there will be no automated
    // way to correct it since we cannot set the baudrate and the unit is
    // already rebooted in the new baud setting.
    mdrive_set_baudrate(axis->device, selected->human);

    // Communication with this device should be at the new baudrate.
    // NOTE: That other motors are allowed to be at different baudrates
    axis->speed = selected->human;

    // Re-detect communication settings (reset by reboot)
    mdrive_config_inspect(axis, true);

    return 0;
}

int
mdrive_config_set_address(mdrive_axis_t * axis, char address) {
    char quoted_addr[4];
    snprintf(quoted_addr, sizeof quoted_addr, "\"%c\"", address);

    // XXX: If address == '!' or 0, the unit must be factory defaulted in
    //      order to apply such a setting

    // Reset variables
    mdrive_config_rollback(axis);

    if (!mdrive_set_variable_string(axis, "DN", quoted_addr))
        return -EIO;

    axis->address = address;

    if (!axis->party_mode) {
        if (RESPONSE_OK != mdrive_send(axis, "PY=1\n"))
            return -EIO;

        axis->party_mode = true;
        
        // Send extra CTRL+J (\n)
        if (RESPONSE_OK != mdrive_send(axis, ""))
            return -EIO;
    }

    if (!mdrive_config_commit(axis));
        return -EIO;

    return 0;
}

int
mdrive_config_inspect_checksum(mdrive_axis_t * axis) {
    int value, i, old=axis->checksum;
    static int modes[] = { CK_OFF, CK_ON };
    int * setting;

    for (i=0, setting = modes; i<2; setting++, i++) {
        axis->checksum = *setting;
        if (mdrive_get_integer(axis, "CK", &value) == 0) {
            axis->checksum = value;
            mcTraceF(20, MDRIVE_CHANNEL, "Device CK mode is %d", value);
            return 0;
        }
    }
    axis->checksum = old;
    return EIO;
}

int
mdrive_config_inspect_echo(mdrive_axis_t * axis) {
    static int modes[] = { EM_ON, EM_PROMPT, EM_QUIET };
    int value, i, *setting, old = axis->echo;

    for (i=0, setting = modes; i<3; setting++, i++) {
        axis->echo = *setting;
        if (mdrive_get_integer(axis, "EM", &value) == 0) {
            axis->echo = value;
            mcTraceF(20, MDRIVE_CHANNEL, "Device EM mode is %d", value);
            return 0;
        }
    }
    axis->echo = old;
    return EIO;
}

int
mdrive_config_inspect(mdrive_axis_t * axis, bool set) {
    // The CK and EM settings are sort of interdependent in that until both
    // are figured out, it will be difficult to interpret the response of
    // the unit.

    if (axis->address == '*')
        // By default, the motors will not respond to global commands, and,
        // even if they did, it would likely get clobbered.
        return 0;
    
    // Assume EM=0 which is the most difficult to work with
    axis->echo = EM_ON;

    // Inspect CK setting
    if (mdrive_config_inspect_checksum(axis))
        return EIO;

    // Inspect EM setting
    if (mdrive_config_inspect_echo(axis))
        return EIO;

    // Configure motor in best performance mode for this driver
    if (set) {
        mdrive_set_echo(axis, EM_PROMPT, false);
        mdrive_set_checksum(axis, CK_ON, false);
    }

    // Inspect ES setting (for E-stop)

    return 0;
}
