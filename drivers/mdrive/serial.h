
typedef struct baud_rate baud_rate_t;
struct baud_rate {
    int human;
    int constant;
    int setting;
};

extern const struct baud_rate baud_rates[];

extern int
mdrive_send(mdrive_device_t *, const char *);

struct mdrive_send_opts {
    bool                expect_data;    // Expect data back
    mdrive_response_t * result;         // Summary of transaction
    const struct timespec * waittime;   // Non-standard waittime
    bool                expect_err;     // Expect an error response (used 
                                        // for retrieving the current error)
    bool                raw;            // Don't send EOL char
    unsigned short      tries;          // Number of tries (other than def)
    // After the send, change to this baudrate. This is mostly useful for
    // sending a soft reset command when the unit will switch baudrates
    // after the soft reset.
    unsigned int        baudrate;
};

extern int
mdrive_communicate(mdrive_device_t *, const char *,
    const struct mdrive_send_opts *);

extern int
mdrive_connect(mdrive_address_t *, mdrive_device_t *);

extern void
mdrive_disconnect(mdrive_device_t *);

extern int
mdrive_initialize_port(const char * port, int speed, bool async);

extern void *
mdrive_async_read(void *);

extern int
mdrive_get_string(mdrive_device_t * device, const char * variable,
    char * value, int size);

extern int
mdrive_get_integer(mdrive_device_t * device, const char * variable, int * value);

extern int
mdrive_get_integers(mdrive_device_t * device, const char *[], int *[], int count);

extern int
mdrive_set_baudrate(mdrive_comm_device_t * comm, int speed);

extern char
mdrive_calc_checksum(const char * buffer, int length);

extern int
mdrive_xmit_time(mdrive_comm_device_t * comm, int chars);

extern void
mdrive_clear_error(mdrive_device_t *);
