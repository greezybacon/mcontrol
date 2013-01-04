#include "mdrive.h"
#include "profile.h"

#include "serial.h"
#include "motion.h"
#include "config.h"

#include <errno.h>
#include <stdio.h>

/**
 * mdrive_lazyload_profile
 *
 * Retrieves the motion profile from the unit. The first time this function
 * is called for a device, it will load the motion parameters from the
 * device. Subsequently, it will return immedately. The parameters
 * retrieved from the device are loaded into the profile structure in the
 * device information so that diff comparisions between current settings and
 * requested settings can be made to reduce I/O traffic to the device
 *
 * Returns:
 * (int) EIO if unable to retrieve settings from motor, 0 otherwise
 */
int
mdrive_lazyload_profile(mdrive_axis_t * device) {
    if (device->loaded.profile)
        return 0;
    
    int A, D, VM, VI, SF, RC, HC, P;
    const char * vars[] = { "A", "D", "VM", "VI", "SF", "RC", "HC", "P" };
    int * vals[] = { &A, &D, &VM, &VI, &SF, &RC, &HC, &P };

    if (mdrive_get_integers(device, vars, vals, 8))
        return EIO;

    // Convert from steps to microrevs
    device->profile.accel = (struct measurement) {
        .value = mdrive_steps_to_microrevs(device, A),
        .units = MICRO_REVS
    };
    device->profile.decel = (struct measurement) {
        .value = mdrive_steps_to_microrevs(device, D),
        .units = MICRO_REVS
    };
    device->profile.vmax = (struct measurement) {
        .value = mdrive_steps_to_microrevs(device, VM),
        .units = MICRO_REVS
    };
    device->profile.vstart = (struct measurement) {
        .value = mdrive_steps_to_microrevs(device, VI),
        .units = MICRO_REVS
    };
    device->profile.slip_max = (struct measurement) {
        .value = mdrive_steps_to_microrevs(device, SF),
        .units = MICRO_REVS
    };

    device->profile.current_run = RC;
    device->profile.current_hold = HC;

    device->position = P;

    device->loaded.profile = true;
    return 0;
}

int
mdrive_profile_accel(mdrive_axis_t * device, long long accel) {
    mdrive_lazyload_profile(device);

    if (device->profile.accel.value == accel)
        return 0;

    if (accel < 1)
        return EINVAL;

    if (!mdrive_set_variable(device, "A",
            mdrive_microrevs_to_steps(device, accel)))
        return EIO;

    device->profile.accel.value = accel;
    return 0;
}

int
mdrive_profile_decel(mdrive_axis_t * device, long long decel) {
    mdrive_lazyload_profile(device);

    if (device->profile.decel.value == decel)
        return 0;

    if (decel < 1)
        return EINVAL;

    if (!mdrive_set_variable(device, "D",
            mdrive_microrevs_to_steps(device, decel)))
        return EIO;

    device->profile.decel.value = decel;
    return 0;
}

int
mdrive_profile_vmax(mdrive_axis_t * device, long long vmax) {
    mdrive_lazyload_profile(device);

    if (device->profile.vmax.value == vmax)
        return 0;

    if (vmax < 1)
        return EINVAL;

    if (!mdrive_set_variable(device, "VM",
            mdrive_microrevs_to_steps(device, vmax)))
        return EIO;

    device->profile.vmax.value = vmax;
    return 0;
}

int
mdrive_profile_vstart(mdrive_axis_t * device, long long vstart) {
    mdrive_lazyload_profile(device);

    if (device->profile.vstart.value == vstart)
        return 0;

    if (vstart < 1)
        return EINVAL;

    if (!mdrive_set_variable(device, "VI",
            mdrive_microrevs_to_steps(device, vstart)))
        return EIO;

    device->profile.vstart.value = vstart;
    return 0;
}

int
mdrive_profile_slipmax(mdrive_axis_t * device, long long slipmax) {
    mdrive_lazyload_profile(device);

    if (device->profile.slip_max.value == slipmax)
        return 0;

    mdrive_lazyload_motion_config(device);
    if (!device->encoder)
        // Device will only honor stall factor (SF) in encoder mode
        return ENOTSUP;

    if (slipmax < 1)
        return EINVAL;

    if (!mdrive_set_variable(device, "SF",
            mdrive_microrevs_to_steps(device, slipmax)))
        return EIO;

    device->profile.slip_max.value = slipmax;
    return 0;
}

int
mdrive_profile_irun(mdrive_axis_t * device, int irun) {
    mdrive_lazyload_profile(device);

    if (device->profile.current_run == irun)
        return 0;

    if (irun < 10 || irun > 100)
        return EINVAL;

    if (!mdrive_set_variable(device, "RC", irun))
        return EIO;

    device->profile.current_run = irun;
    return 0;
}

int
mdrive_profile_ihold(mdrive_axis_t * device, int ihold) {
    mdrive_lazyload_profile(device);

    if (device->profile.current_hold == ihold)
        return 0;

    if (ihold < 10 || ihold > 100)
        return EINVAL;

    if (!mdrive_set_variable(device, "HC", ihold))
        return EIO;

    device->profile.current_hold = ihold;
    return 0;
}

int
mdrive_set_profile(mdrive_axis_t * device, struct motion_profile * profile) {
    // TODO: Add error checking
    // XXX: Check units for each measurable item is MICRO_REVS
    mdrive_profile_accel(device, profile->accel.value);
    mdrive_profile_decel(device, profile->decel.value);
    mdrive_profile_vmax(device, profile->vmax.value);
    mdrive_profile_vstart(device, profile->vstart.value);
    mdrive_profile_slipmax(device, profile->slip_max.value);
    mdrive_profile_irun(device, profile->current_run);
    mdrive_profile_ihold(device, profile->current_hold);

    return 0;
}
