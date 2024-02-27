#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libserialport.h>

#define SEND_RECEIVE_TIMEOUT_MS 1000

// Helper function for error handling
int check(enum sp_return result);

int main(int argc, char* argv[]) {
    // get port name from CLI
    if (argc != 3) {
        printf("Usage: ./stm32_firmware_updater /dev/tty.usbserial-XXXXX <baud_rate>");
        exit(EXIT_FAILURE);
    }
    char* port_name = argv[1];
    int baud_rate = atoi(argv[2]);

    // Open and configure port with 8 bits word length, no parity, and 1 stop bits
    struct sp_port* port;
    printf("Opening port %s with %d baud rate, no parity, no control flow and 1 stop bits\n", port_name, baud_rate);
    check(sp_get_port_by_name(port_name, &port));
    check(sp_open(port, SP_MODE_READ_WRITE));
    check(sp_set_baudrate(port, baud_rate));
    check(sp_set_bits(port, 8));
    check(sp_set_parity(port, SP_PARITY_NONE));
    check(sp_set_stopbits(port, 1));
    check(sp_set_flowcontrol(port, SP_FLOWCONTROL_NONE));

    // Send data
    char* data  = "Hello!";
    int size = strlen(data);

    printf("Sending '%s' (%d bytes) on port %s.\n", data, size, port_name);
    int res = check(sp_blocking_write(port, data, size, SEND_RECEIVE_TIMEOUT_MS));
    
    if (res == size) {
        printf("Sent %d bytes successfully.\n", size);
    } else {
        printf("Timed out, %d/%d bytes sent.\n", res, size);
    }

    // Receive data
    char* buf = malloc(size + 1);
    if (buf == NULL) {
        printf("Malloc failed!\n");
        return EXIT_FAILURE;
    }

    printf("Attemping to receive %d bytes on port %s.\n", size, port_name);
    res = check(sp_blocking_read(port, buf, size, SEND_RECEIVE_TIMEOUT_MS));
    if (res == size) {
        printf("Received %d bytes successfully.\n", size);
    } else {
        printf("Timed out, %d/%d bytes received.\n", res, size);
    }

    /* Check if we received the same data we sent. */
    buf[res] = '\0';
    printf("Received '%s'.\n", buf);

    // close ports and free resources
    check(sp_close(port));
    sp_free_port(port);
    free(buf);

    return EXIT_SUCCESS;
}

int check(enum sp_return result) {
    char *error_message;
    switch (result) {
        case SP_ERR_ARG:
            printf("Error: Invalid argument.\n");
            abort();
        case SP_ERR_FAIL:
            error_message = sp_last_error_message();
            printf("Error: Failed: %s\n", error_message);
            sp_free_error_message(error_message);
            abort();
        case SP_ERR_SUPP:
            printf("Error: Not supported.\n");
            abort();
        case SP_ERR_MEM:
            printf("Error: Couldn't allocate memory.\n");
            abort();
        case SP_OK:
        default:
                return result;
    }
}