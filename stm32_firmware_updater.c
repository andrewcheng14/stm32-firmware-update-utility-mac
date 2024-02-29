#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libserialport.h>
#include "stm32_firmware_updater.h"

/* Private Helper Functions */
// Open and configure port with 8 bits word length, no parity, and 1 stop bits
int openSerialPort(char* port_name, int baud_rate, struct sp_port** port);
// Send OTA start command with given port over UART
int sendOtaStartCommand(struct sp_port* port);
// Helper function for error handling
int check(enum sp_return result);


/* Global Arrays */
uint8_t DATA_BUF[PACKET_MAX_SIZE];
uint8_t APP_BIN[APP_FW_MAX_SIZE];

int main(int argc, char* argv[]) {
    // get port name from CLI
    if (argc != 3) {
        printf("Usage: ./stm32_firmware_updater /dev/tty.usbserial-XXXXX <baud_rate>");
        exit(EXIT_FAILURE);
    }
    char* port_name = argv[1];
    int baud_rate = atoi(argv[2]);

    /* Connect to COM port */
    struct sp_port* port;
    if (openSerialPort(port_name, baud_rate, &port)) {
        printf("Error opening serial port!\n");
        return EXIT_FAILURE;
    }
    printf("Successfully opened port %s\n", port_name);

    /* send OTA start command */
    sendOtaStartCommand(port);

    /* send OTA Header */


    /* send OTA end command */


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

int openSerialPort(char* port_name, int baud_rate, struct sp_port** port) {
    printf("Opening port %s with %d baud rate, no parity, no control flow and 1 stop bits\n", port_name, baud_rate);

    check(sp_get_port_by_name(port_name, port));
    check(sp_open(*port, SP_MODE_READ_WRITE));
    check(sp_set_baudrate(*port, baud_rate));
    check(sp_set_bits(*port, 8));
    check(sp_set_parity(*port, SP_PARITY_NONE));
    check(sp_set_stopbits(*port, 1));
    check(sp_set_flowcontrol(*port, SP_FLOWCONTROL_NONE));

    return 0;
}

int sendOtaStartCommand(struct sp_port* port) {
    OtaCommandPacket* cmd_packet = (OtaCommandPacket*) DATA_BUF;
    
    // Build command packet to send
    memset(DATA_BUF, 0, PACKET_MAX_SIZE);
    cmd_packet->sof         = PACKET_SOF;
    cmd_packet->packet_type = COMMAND;
    cmd_packet->packet_num  = 0;
    cmd_packet->payload_len = 1;
    cmd_packet->crc32       = 0;  // TBD: Implement CRC32
    cmd_packet->eof         = PACKET_EOF;

    // Send OTA command packet
    int size = sizeof(OtaCommandPacket);
    printf("Sending OTA start command (%d bytes).\n", size);
    int res = check(sp_blocking_write(port, (uint8_t*) cmd_packet, size, SEND_RECEIVE_TIMEOUT_MS));
    if (res != size) {
        printf("Timed out, %d/%d bytes sent.\n", res, size);
        return -1;
    }
    printf("Sent OTA start command successfully.\n");

    // Wait for ACK or NACK from MCU
    printf("Waiting for ACK or NACK from MCU...\n") 
    if (!isAckResponseReceived(port)) {
        return -1;
    }
    printf("Received ACK from MCU.\n");

    return 0;
}




// /* Build the OTA START command */
//   uint16_t len;
//   ETX_OTA_COMMAND_ *ota_start = (ETX_OTA_COMMAND_*)DATA_BUF;
//   int ex = 0;

//   memset(DATA_BUF, 0, ETX_OTA_PACKET_MAX_SIZE);

//   ota_start->sof          = ETX_OTA_SOF;
//   ota_start->packet_type  = ETX_OTA_PACKET_TYPE_CMD;
//   ota_start->data_len     = 1;
//   ota_start->cmd          = ETX_OTA_CMD_START;
//   ota_start->crc          = 0x00;               //TODO: Add CRC
//   ota_start->eof          = ETX_OTA_EOF;

//   len = sizeof(ETX_OTA_COMMAND_);

//   //send OTA START
//   for(int i = 0; i < len; i++)
//   {
//     delay(1);
//     if( RS232_SendByte(comport, DATA_BUF[i]) )
//     {
//       //some data missed.
//       printf("OTA START : Send Err\n");
//       ex = -1;
//       break;
//     }
//   }

//   if( ex >= 0 )
//   {
//     if( !is_ack_resp_received( comport ) )
//     {
//       //Received NACK
//       printf("OTA START : NACK\n");
//       ex = -1;
//     }
//   }
//   printf("OTA START [ex = %d]\n", ex);
//   return ex;

// }

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

bool isAckResponseReceived(struct sp_port* port) {
    uint8_t buf[PACKET_MAX_SIZE];
    int size = sizeof(OtaResponsePacket);
    int res = check(sp_blocking_read(port, buf, size, SEND_RECEIVE_TIMEOUT_MS));
    if (res != size) {
        printf("Timed out while reading OTA response packet, %d/%d bytes received.\n", res, size);
        return false;
    }
    OtaResponsePacket* response = (OtaResponsePacket*) buf;
    // TODO: Check for CRC32
    if (response->status == PACKET_ACK) {
        return true;
    } else {
        printf("Received NACK.\n");
        return false;
    }
}
