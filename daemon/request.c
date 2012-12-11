#include "../drivers/driver.h"
#include "../lib/message.h"
#include "../lib/driver.h"
#include "../lib/locks.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <mqueue.h>

int
handle_move_request(request_message_t * message) {
    
    // Find motor by the motor id
    Driver * driver = driver_for_motor(message->motor_id);
    if (driver == NULL)
        return EINVAL;

    //if (mcDriverIsLocked(driver))
    //    return EBUSY; 

    // TODO: Some conversion might be necessary for the motion instruction
    // based on the configuration of the motor

    if (driver->class->move == NULL)
        return ENOTSUP;

    // TODO: Add tracing
    
    motion_instruction_t * command = (void *) message->payload;
    return driver->class->move(driver, command);
}

int
handle_profile_get_request(request_message_t * message) {
    // Find motor by the motor id
    Driver * driver = driver_for_motor(message->motor_id);
    if (driver == NULL)
        return EINVAL;
    
    //Profile p = {
    //    .accel = driver->class->query_int(driver, Q_ACCEL)
    //};

    //if (driver->class->query_int(driver, Q_MOTOR_TYPE) == MOTOR_STEPPER) {
    //    // Add items for stepper motors
    //}
}

int
handle_connect_request(request_message_t * message) {

    // Unpack the message
    struct connect_request * msg = (void *) message->payload;

    printf("Received a connection request to: %s\n", msg->connection_string);

    // Connect to the motor on behalf of the client
    Driver * driver = mcDriverConnect(msg->connection_string);

    struct connect_response pay = {
        .motor_id = (driver == NULL) ? 0 : driver->id
    };

    int size = sizeof(struct response_message) + sizeof pay;
    struct response_message * resp = calloc(1, size);

    *resp = (struct response_message) {
        .status = (driver == NULL) ? 1 : 0,
        .id = message->id,
        .size = size,
        .payload_size = sizeof pay
    };
    memcpy(resp->payload, &pay, sizeof pay);

    char boxname[64];
    snprintf(boxname, sizeof boxname, CLIENT_QUEUE_FMT, message->pid, "wait");

    mqd_t outbox = mq_open(boxname, O_WRONLY);
    if (outbox == -1)
        printf("Unable to open client queue: %s, (%d)\n", boxname, errno);
    
    else
        if (mq_send(outbox, (void *) resp, size, 0) == -1)
            printf("Unable to send response message: %d\n", errno);
    mq_close(outbox);
}
