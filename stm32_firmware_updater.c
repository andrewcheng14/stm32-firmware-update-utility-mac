#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libserialport.h>
#include <stdbool.h>
#include "stm32_firmware_updater.h"

/* 
 * Private Helper Functions 
*/
// Open and configure port with 8 bits word length, no parity, and 1 stop bits
int openSerialPort(char* port_name, int baud_rate, struct sp_port** port);
// Send OTA start command with given port over UART
int sendOtaStartCommand(struct sp_port* port);
// Send OTA header with given port over UART
int sendOtaHeader(struct sp_port* port, FileInfo file_info);
// Send OTA data with given port over UART
int sendOtaData(struct sp_port* port, uint8_t* data, int size);
// Send OTA end command with given port over UART
int sendOtaEndCommand(struct sp_port* port);
// Helper function for error handling
int check(enum sp_return result);
// Check if ACK response is received from the given port
bool isAckResponseReceived(struct sp_port* port);

/* Global Arrays */
uint8_t DATA_BUF[PACKET_MAX_SIZE];
uint8_t APP_BIN[APP_FW_MAX_SIZE];

int main(int argc, char* argv[]) {
    char bin_path[FILE_NAME_MAX_LEN];
    char port_name[FILE_NAME_MAX_LEN];

    /* get binary file path, port name, and baud rate from CLI */
    if (argc != 4) {
        printf("Usage: ./stm32_firmware_updater <PATH/TO/FIRMWARE/BIN> /dev/tty.usbserial-XXXXX <baud_rate>");
        exit(EXIT_FAILURE);
    }
    strcpy(bin_path, argv[1]);
    strcpy(port_name, argv[2]);
    int baud_rate = atoi(argv[3]);

    /* Open firmware image binary and get metadata */
    printf("Opening firmware image at %s\n", bin_path);
    FILE* file = fopen(bin_path, "rb");
    if (file == NULL) {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    FileInfo file_info;
    fseek(file, 0, SEEK_END);
    file_info.file_size = ftell(file);
    file_info.crc32 = 0;  // TBD: Implement CRC32
    rewind(file);
    if (file_info.file_size > APP_FW_MAX_SIZE) {
        printf("Error: Firmware image size exceeds maximum allowed size of %d bytes.\n", APP_FW_MAX_SIZE);
        fclose(file);
        return EXIT_FAILURE;
    }

    /* Connect to COM port */
    struct sp_port* port;
    if (openSerialPort(port_name, baud_rate, &port)) {
        printf("Error opening serial port!\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    printf("Successfully opened port %s\n", port_name);

    /* send OTA start command */
    if (sendOtaStartCommand(port) < 0) {
        printf("Error sending OTA start command!\n");
        fclose(file);
        check(sp_close(port));
        sp_free_port(port);
        return EXIT_FAILURE;
    }

    /* send OTA Header */
    if (sendOtaHeader(port, file_info) < 0) {
        printf("Error sending OTA header!\n");
        fclose(file);
        check(sp_close(port));
        sp_free_port(port);
        return EXIT_FAILURE;
    }

    /* send OTA data */


    /* send OTA end command */
    if (sendOtaEndCommand(port) < 0) {
        printf("Error sending OTA end command!\n");
        fclose(file);
        check(sp_close(port));
        sp_free_port(port);
        return EXIT_FAILURE;
    }

    // close ports and free resources
    fclose(file);
    check(sp_close(port));
    sp_free_port(port);

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
    cmd_packet->cmd         = OTA_START_CMD;
    cmd_packet->crc32       = 0;  // TBD: Implement CRC32
    cmd_packet->eof         = PACKET_EOF;

    // Send OTA command packet
    int size = sizeof(OtaCommandPacket);
    printf("\n\n\n\n***** Sending OTA start command (%d bytes). *****\n\n", size);
    int res = check(sp_blocking_write(port, (uint8_t*) cmd_packet, size, SEND_RECEIVE_TIMEOUT_MS));
    if (res != size) {
        printf("Timed out, %d/%d bytes sent.\n", res, size);
        return -1;
    }
    printf("Sent OTA start command successfully.\n");

    // Wait for ACK or NACK from MCU
    printf("Waiting for ACK or NACK from MCU...\n");
    if (!isAckResponseReceived(port)) {
        return -1;
    }
    printf("Received ACK from MCU.\n");

    return 0;
}

int sendOtaHeader(struct sp_port* port, FileInfo file_info) {
    OtaHeaderPacket* header_packet = (OtaHeaderPacket*) DATA_BUF;

    // Build header packet to send
    memset(DATA_BUF, 0, PACKET_MAX_SIZE);
    header_packet->sof         = PACKET_SOF;
    header_packet->packet_type = HEADER;
    header_packet->packet_num  = 0;
    header_packet->payload_len = sizeof(FileInfo);
    header_packet->file_info   = file_info;
    header_packet->crc32       = 0;  // TBD: Implement CRC32
    header_packet->eof         = PACKET_EOF;

    // Send OTA header packet
    int size = sizeof(OtaHeaderPacket);
    printf("\n\n\n\n***** Sending OTA header (%d bytes). *****\n\n", size);
    int res = check(sp_blocking_write(port, (uint8_t*) header_packet, size, SEND_RECEIVE_TIMEOUT_MS));
    if (res != size) {
        printf("Timed out, %d/%d bytes sent.\n", res, size);
        return -1;
    }
    printf("Sent OTA header successfully.\n");

    // Wait for ACK or NACK from MCU
    printf("Waiting for ACK or NACK from MCU...\n");
    if (!isAckResponseReceived(port)) {
        return -1;
    }
    printf("Received ACK from MCU.\n");

    return 0;
}

int sendOtaEndCommand(struct sp_port* port) {
    OtaCommandPacket* cmd_packet = (OtaCommandPacket*) DATA_BUF;

    // Build command packet to send
    memset(DATA_BUF, 0, PACKET_MAX_SIZE);
    cmd_packet->sof         = PACKET_SOF;
    cmd_packet->packet_type = COMMAND;
    cmd_packet->packet_num  = 0;
    cmd_packet->payload_len = 1;
    cmd_packet->cmd         = OTA_END_CMD;
    cmd_packet->crc32       = 0;  // TBD: Implement CRC32
    cmd_packet->eof         = PACKET_EOF;

    // Send OTA command packet
    int size = sizeof(OtaCommandPacket);
    printf("\n\n\n\n***** Sending OTA end command (%d bytes). *****\n\n", size);
    int res = check(sp_blocking_write(port, (uint8_t*) cmd_packet, size, SEND_RECEIVE_TIMEOUT_MS));
    if (res != size) {
        printf("Timed out, %d/%d bytes sent.\n", res, size);
        return -1;
    }
    printf("Sent OTA end command successfully.\n");

    // Wait for ACK or NACK from MCU
    printf("Waiting for ACK or NACK from MCU...\n");
    if (!isAckResponseReceived(port)) {
        return -1;
    }
    printf("Received ACK from MCU.\n");

    return 0;
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

bool isAckResponseReceived(struct sp_port* port) {
    uint8_t buf[PACKET_MAX_SIZE];
    int size = sizeof(OtaResponsePacket);
    int res = check(sp_blocking_read(port, buf, size, SEND_RECEIVE_TIMEOUT_MS));
    if (res != size) {
        printf("Timed out while reading OTA response packet, %d/%d expected bytes received.\n", res, size);
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
