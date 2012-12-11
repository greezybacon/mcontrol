
typedef struct baud_rate baud_rate_t;
struct baud_rate {
    int human;
    int constant;
    int setting;
};

extern const struct baud_rate baud_rates[];

extern int
mdrive_send(mdrive_axis_t *, const char *);

struct mdrive_send_opts {
    const char *        command;        // What to send
    bool                expect_data;    // Expect data back
    mdrive_response_t * result;         // Summary of transaction
    const struct timespec * waittime;   // Non-standard waittime
    bool                expect_err;     // Expect an error response (used 
                                        // for retrieving the current error)
    bool                raw;            // Don't send EOL char
    short               tries;          // Number of tries (other than def)
};

extern int
mdrive_send_get_response(mdrive_axis_t *, const struct mdrive_send_opts *);

extern int
mdrive_connect(mdrive_address_t *, mdrive_axis_t *);

extern void
mdrive_disconnect(mdrive_axis_t *);

extern char **
mdrive_enum_serial_ports(void);

extern mdrive_address_t *
mdrive_enum_motors_on_port(const char *);

extern int
mdrive_initialize_port(const char * port, int speed, bool async);

extern void *
mdrive_async_read(void *);

extern int
mdrive_get_string(mdrive_axis_t * axis, const char * variable,
    char * value, int size);

extern int
mdrive_get_integer(mdrive_axis_t * axis, const char * variable, int * value);

extern int
mdrive_get_integers(mdrive_axis_t * axis, const char *[], int *[], int count);

extern int
mdrive_set_baudrate(mdrive_axis_device_t * axis, int speed);

extern char
mdrive_calc_checksum(const char * buffer, int length);

extern int
mdrive_xmit_time(mdrive_axis_device_t * axis, int chars);
