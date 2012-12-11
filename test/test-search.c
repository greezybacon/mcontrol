#include "../motor.h"
#include "../lib/client.h"

#include <stdio.h>

int main(int argc, char * argv[]) {
    String results;
    String driver;

    driver.size = snprintf(driver.buffer, sizeof driver.buffer, "%s", "mdrive");

    int items = mcSearch(0, &driver, &results);
    printf("Found %d items\n", items);

    char * buffer = results.buffer;
    while (items--) {
        buffer += printf("Found: %s\n", buffer);
        buffer++;
    }
}
