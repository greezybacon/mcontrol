extern int
mdrive_move(Driver * self, motion_instruction_t * command);

extern int
mdrive_move_assisted(mdrive_device_t * device, motion_instruction_t * command,
        int steps);

extern int
mdrive_stop(Driver * self, enum stop_type);

extern int
mdrive_home(Driver * self, enum home_type, enum home_direction);

extern long long
mdrive_steps_to_microrevs(mdrive_device_t * device, int position);

extern int
mdrive_microrevs_to_steps(mdrive_device_t * device, long long microrevs);

extern int
mdrive_lazyload_motion_config(mdrive_device_t * device);

extern int
mdrive_drive_enable(mdrive_device_t * device);

extern int
mdrive_drive_disable(mdrive_device_t * device);
