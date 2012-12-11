#include "../motor.h"
#include "../lib/client.h"

#include <stdio.h>

int main(int argc, char * argv[]) {
    String str;
    str.size = snprintf(str.buffer, sizeof str.buffer, "%s",
        "mdrive:///dev/ttyS0@115200:a");

    motor_t motor;
    int status = mcConnect(&str, &motor);
    printf("(%d) Connected to motor, id %d\n", status, motor);
}
