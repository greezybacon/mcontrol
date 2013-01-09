struct query_variable {
    // 1: int, 2: string, 3: int with item, 4: string with item,
    // 5: use (*read), 6: item is write-only
    // 9: int + convert from microrevs (|8)
    // |16: item is write-only
    char type;
    motor_query_t query;
    char * variable;
    int (*read)(mdrive_axis_t *, struct motor_query *, struct query_variable *);
    int (*write)(mdrive_axis_t *, struct motor_query *, struct query_variable *);
};
    

extern int
mdrive_read_variable(Driver * self, struct motor_query * query);

extern int
mdrive_write_variable(Driver * self, struct motor_query * query);

extern int
mdrive_write_simple(mdrive_axis_t * axis, struct motor_query * query,
    struct query_variable * q);

#define PEEK(x) int x(mdrive_axis_t *, struct motor_query *, \
    struct query_variable *)

#define POKE(x) PEEK(x)
