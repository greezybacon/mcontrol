#include "../motor.h"
#include "../lib/message.h"
#include "../lib/client.h"

#include "../drivers/mdrive/mdrive.h"

#include <stdio.h>

int main(int argc, char * argv[]) {
    motor_t motor;

    String str;
    str.size = snprintf(str.buffer, sizeof str.buffer, "%s",
        "mdrive:///dev/ttyS0@115200:a");

    int status = mcConnect(&str, &motor);
    printf("(%d) Connected to motor, id %d\n", status, motor);

    // Motor is 1.33 revolutions per inch
    status = mcUnitScaleSet(motor, MILLI_INCH, 1333);
    printf("(%d) Motor scale configured\n", status);

    status = mcMoveRelativeUnits(motor, 237, MILLI_INCH);
    printf("(%d) Motor is moving\n", status);
}
