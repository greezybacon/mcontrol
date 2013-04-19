
extern int
mdrive_subscribe(Driver * self, event_t code, driver_event_callback_t callback);

extern int
mdrive_notify(Driver * self, event_t code, int cond, driver_event_callback_t callback);

extern int
mdrive_unsubscribe(Driver * self, event_t code, driver_event_callback_t callback);

extern int
mdrive_signal_error_event(mdrive_device_t * device, int error);

extern int
mdrive_signal_event(mdrive_device_t * device, int code, union event_data *);

extern int
mdrive_signal_event_device(mdrive_comm_device_t * comm, char address,
    int event);
