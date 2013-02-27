extern int
mdrive_firmware_write(mdrive_device_t * device, const char * filename);

extern int
mdrive_load_firmware(Driver * self, const char * filename);

extern int
mdrive_check_ug_mode(mdrive_device_t * device, char * serial, int size);
