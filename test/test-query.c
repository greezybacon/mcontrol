#include "../motor.h"
#include "../lib/message.h"
#include "../lib/client.h"

#include "../drivers/mdrive/mdrive.h"

#include <stdio.h>

int main(int argc, char * argv[]) {
    motor_t motor;
    int status;

    String str;
    str.size = snprintf(str.buffer, sizeof str.buffer, "%s",
        "mdrive:///dev/ttyS0@9600");

    mcConnect(&str, &motor);
    printf("Connected to motor, id %d\n", motor);

    // Motor is 200 revolutions per inch
    status = mcUnitScaleSet(motor, INCH, 200);
    printf("(%d) Motor scale configured\n", status);

    int value;
    mcQueryIntegerUnits(motor, MCPOSITION, &value, MILLI_METER);
    printf("Motor is located at %d\n", value);

    String string;
    mcQueryString(motor, MDRIVE_SERIAL, &string);
    printf("Motor serial number is %s\n", string.buffer);
}
