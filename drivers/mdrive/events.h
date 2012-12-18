
extern int
mdrive_notify(Driver * self, event_t code, int cond, driver_event_callback_t callback);

extern int
mdrive_unsubscribe(Driver * self, driver_event_callback_t callback);

extern int
mdrive_signal_error_event(mdrive_axis_t * axis, int error);

extern int
mdrive_signal_event(mdrive_axis_t * axis, int code, union event_data *);

extern int
mdrive_signal_event_device(mdrive_axis_device_t * device, char address,
    int event);
