#ifndef CONFIG_H
#define CONFIG_H

struct mdrive_config_flags {
    bool        checksum;           // CK
    bool        echo;               // EM
    bool        escape;             // ES
    bool        reset;              // CE
};

enum checksum_mode {
    CK_OFF=0,
    CK_ON,
    CK_BUSY_NACK
};
typedef enum checksum_mode checksum_mode_t;

enum echo_mode {
    EM_ON=0,
    EM_PROMPT,
    EM_QUIET,
    EM_DELAY
};
typedef enum echo_mode echo_mode_t;

enum variable_persistence {
    TEMPORAL,
    PERSISTENT
};
typedef enum variable_persistence persistence_t;

extern bool
mdrive_set_checksum(mdrive_axis_t * axis, checksum_mode_t mode, bool);

extern bool
mdrive_set_echo(mdrive_axis_t * axis, echo_mode_t mode, bool);

extern int
mdrive_config_inspect(mdrive_axis_t * axis, bool);

extern int
mdrive_config_set_baudrate(mdrive_axis_t * axis, int speed);

extern int
mdrive_config_set_address(mdrive_axis_t * axis, char address);

extern bool
mdrive_set_variable(mdrive_axis_t * axis, const char * variable, int value);

extern bool
mdrive_config_rollback(mdrive_axis_t * device);

extern bool
mdrive_config_commit(mdrive_axis_t * device,
    struct mdrive_config_flags * preserve);

extern int
mdrive_config_after_reboot(mdrive_axis_t * device);

#endif
