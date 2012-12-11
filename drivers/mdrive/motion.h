extern int
mdrive_move(Driver * self, motion_instruction_t * command);

extern int
mdrive_stop(Driver * self);

extern long long
mdrive_steps_to_microrevs(mdrive_axis_t * device, int position);

extern int
mdrive_microrevs_to_steps(mdrive_axis_t * device, long long microrevs);

extern int
mdrive_lazyload_motion_config(mdrive_axis_t * device);

extern int
mdrive_on_async_complete(mdrive_axis_t * device, bool cancel);
