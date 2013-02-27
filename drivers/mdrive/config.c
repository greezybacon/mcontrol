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
mdrive_config_rollback(mdrive_device_t * device) {
    // Reset all settings
    struct timespec timeout = { .tv_nsec = 750e6 };
    struct mdrive_send_opts options = {
        .expect_data = false,
        .waittime = &timeout
    };
    if (mdrive_communicate(device, "IP", &options) != RESPONSE_OK)
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
 * device - (mdrive_device_t *) Device to commit config changes
 *
 * Returns:
 * (bool) - TRUE if commit was completed successfully and FALSE otherwise
 */
bool
mdrive_config_commit(mdrive_device_t * device,
        struct mdrive_config_flags * preserve) {
    // Place comm settings back to user-friendly ones before saving, unless
    // requested not to
    if (!preserve || !preserve->checksum)
        mdrive_set_checksum(device, CK_OFF, false);
    if (!preserve || !preserve->echo)
        mdrive_set_echo(device, EM_ON, false);

    struct timespec timeout = { .tv_nsec = 750e6 };
    struct mdrive_send_opts options = {
        .expect_data = false,
        .waittime = &timeout
    };

    if (mdrive_communicate(device, "S", &options) != RESPONSE_OK)
        return false;

    if (mdrive_config_inspect(device, true))
        return false;
        
    return true;
}

bool
mdrive_set_variable_string(mdrive_device_t * device, const char * variable,
        const char * value) {
    char cmd[16];

    snprintf(cmd, sizeof cmd, "%s=%s", variable, value);

    if (RESPONSE_OK != mdrive_send(device, cmd))
        return false;

    return true;
}

bool
mdrive_set_variable(mdrive_device_t * device, const char * variable, int value) {
    char val[13];

    snprintf(val, sizeof val, "%d", value);

    return mdrive_set_variable_string(device, variable, val);
}

bool
mdrive_set_checksum(mdrive_device_t * device, checksum_mode_t mode, bool force) {
    if (!force && device->checksum == mode)
        // No need to change it on the motor
        return true;

    short old = device->checksum;

    // The unit only thinks about the response. Therefore, when setting
    // checksum, the unit will immediately respond with ACK; however, when
    // unsetting checksum, the checksum char will need to be sent, because
    // the unit will still be in checksum mode when it processes the
    // request. When it sends the response; however, there will be no ACK.
    if (mode != CK_OFF)
        device->checksum = mode;

    if (mdrive_set_variable(device, "CK", mode)) {
        device->checksum = mode;
        return true;
    }

    device->checksum = old;
    return false;
}

bool
mdrive_set_echo(mdrive_device_t * device, echo_mode_t mode, bool force) {
    if (!force && device->echo == mode)
        // No need to change it on the motor
        return true;

    short old = device->echo;

    device->echo = mode;
    if (mdrive_set_variable(device, "EM", mode))
        return true;

    device->echo = old;
    return false;
}

int
mdrive_config_set_baudrate(mdrive_device_t * device, int speed) {
    const struct baud_rate * selected;

    for (selected=baud_rates; selected->setting; selected++)
        if (speed == selected->human)
            break;
    if (selected->human == 0)
        // Unsupported baud rate setting
        return ENOTSUP;

    // Make sure we don't save anything unexpected
    mdrive_config_rollback(device);

    if (!mdrive_set_variable(device, "BD", selected->setting))
        return EIO;

    if (!mdrive_config_commit(device, NULL)) // Saves the configuration
        return EIO;

    // Reboot the unit for BD to take effect; however we won't be able to
    // talk to the unit because we'll still be at the wrong speed
    mdrive_reboot(device);            
                                    
    // NOTE: If the mdrive_set_baudrate fails, there will be no automated
    // way to correct it since we cannot set the baudrate and the unit is
    // already rebooted in the new baud setting.
    mdrive_set_baudrate(device->comm, selected->human);

    // Communication with this device should be at the new baudrate.
    // NOTE: That other motors are allowed to be at different baudrates
    device->speed = selected->human;

    // Re-detect communication settings (reset by reboot)
    mdrive_config_inspect(device, true);

    // Invalidate driver cache so that a request on the original connection
    // string that hit this motor will not be reused
    mcDriverCacheInvalidate(device->driver);

    return 0;
}

int
mdrive_config_set_address(mdrive_device_t * device, char address) {
    char quoted_addr[4];
    snprintf(quoted_addr, sizeof quoted_addr, "\"%c\"", address);

    // XXX: If address == '!' or 0, the unit must be factory defaulted in
    //      order to apply such a setting

    // Reset variables
    mdrive_config_rollback(device);

    if (!mdrive_set_variable_string(device, "DN", quoted_addr))
        return -EIO;

    device->address = address;

    if (!device->party_mode) {
        if (RESPONSE_OK != mdrive_send(device, "PY=1\n"))
            return -EIO;

        device->party_mode = true;
    }

    if (!mdrive_config_commit(device, NULL))
        return -EIO;

    // Invalidate driver cache so that a request on the original connection
    // string that hit this motor will not be reused
    mcDriverCacheInvalidate(device->driver);

    return 0;
}

int
mdrive_config_inspect_checksum(mdrive_device_t * device) {
    int value, i, old=device->checksum;
    static int modes[] = { CK_OFF, CK_ON };
    int * setting;

    for (i=0, setting = modes; i<2; setting++, i++) {
        device->checksum = *setting;
        if (mdrive_get_integer(device, "CK", &value) == 0) {
            device->checksum = value;
            mcTraceF(20, MDRIVE_CHANNEL, "Device CK mode is %d", value);
            return 0;
        }
    }
    device->checksum = old;
    return EIO;
}

int
mdrive_config_inspect_echo(mdrive_device_t * device) {
    static int modes[] = { EM_ON, EM_PROMPT, EM_QUIET };
    int value, i, *setting, old = device->echo;

    for (i=0, setting = modes; i<3; setting++, i++) {
        device->echo = *setting;
        if (mdrive_get_integer(device, "EM", &value) == 0) {
            device->echo = value;
            mcTraceF(20, MDRIVE_CHANNEL, "Device EM mode is %d", value);
            return 0;
        }
    }
    device->echo = old;
    return EIO;
}

int
mdrive_config_inspect(mdrive_device_t * device, bool set) {
    // The CK and EM settings are sort of interdependent in that until both
    // are figured out, it will be difficult to interpret the response of
    // the unit.

    if (device->address == '*')
        // By default, the motors will not respond to global commands, and,
        // even if they did, it would likely get clobbered.
        return 0;
    
    // Assume EM=0 which is the most difficult to work with
    device->echo = EM_ON;

    // Inspect CK setting
    if (mdrive_config_inspect_checksum(device))
        return EIO;

    // Inspect EM setting
    if (mdrive_config_inspect_echo(device))
        return EIO;

    // Configure motor in best performance mode for this driver
    if (set) {
        mdrive_set_echo(device, EM_PROMPT, false);
        mdrive_set_checksum(device, CK_ON, false);
    }

    // Inspect ES setting (for E-stop)

    // Inspect CE setting (for reset (CTRL+C)

    return 0;
}

/**
 * mdrive_config_after_reboot
 *
 * Should be run after a motor is rebooted. This routine will reset the
 * configuration and motion settings so that the motor can continue to work
 * without the software being reset as well
 */
int
mdrive_config_after_reboot(mdrive_device_t * device) {
    // Assume the motor rebooted
    device->loaded.mask = 0;

    // Keep track of motor reboots
    device->stats.reboots++;

    // Unit is no longer in checksum mode
    if (mdrive_config_inspect(device, true))
        return EIO;

    // Configure the encoder setting again
    char buffer[32];
    snprintf(buffer, sizeof buffer, "EE=%d", (device->encoder) ? 1 : 0);
    if (mdrive_send(device, buffer))
        return EIO;

    return 0;
}
