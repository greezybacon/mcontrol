
extern int
mdrive_subscribe(Driver * self, driver_event_callback_t callback);

extern int
mdrive_unsubscribe(Driver * self, driver_event_callback_t callback);

extern int
mdrive_signal_event(mdrive_axis_t * axis, int event);

extern int
mdrive_signal_event_device(mdrive_axis_device_t * device, char address,
    int event);
