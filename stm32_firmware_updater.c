#include "ota_fw_update.h"

/* Globals */
uint8_t APP_BIN[APP_FW_MAX_SIZE];

/* 
 * Private Helper Functions 
*/
int sendBinaryFile(struct sp_port* port, uint32_t size);

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
    printf("Opened firmware image successfully. Size: %d bytes\n", file_info.file_size);

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
    if (fread(APP_BIN, 1, file_info.file_size, file) != file_info.file_size) {
        perror("Error reading file");
        fclose(file);
        check(sp_close(port));
        sp_free_port(port);
        return -1;
    }
    if (sendBinaryFile(port, file_info.file_size) < 0) {
        printf("Error sending binary file!\n");
        fclose(file);
        check(sp_close(port));
        sp_free_port(port);
        return EXIT_FAILURE;
    }

    /* send OTA end command */
    if (sendOtaEndCommand(port) < 0) {
        printf("Error sending OTA end command!\n");
        fclose(file);
        check(sp_close(port));
        sp_free_port(port);
        return EXIT_FAILURE;
    }
    printf("Firmware update complete yippie! :D\n");

    // close ports and free resources
    fclose(file);
    check(sp_close(port));
    sp_free_port(port);

    return EXIT_SUCCESS;
}

int sendBinaryFile(struct sp_port* port, uint32_t size) {
    int packet_number = 1;
    int bytes_remaining = size;
    int total_data_packets_to_send = (size / PACKET_MAX_PAYLOAD_SIZE) + 1;

    while (bytes_remaining > 0) {
        int bytes_to_send = (bytes_remaining > PACKET_MAX_PAYLOAD_SIZE) ? PACKET_MAX_PAYLOAD_SIZE : bytes_remaining;
        if (sendOtaData(port, APP_BIN, packet_number, bytes_to_send) < 0) {
            printf("Error sending OTA data packet %d\n", packet_number);
            return -1;
        }
        printf("Sent OTA data packet %d (%d/%d)\n", packet_number, packet_number, total_data_packets_to_send);
        bytes_remaining -= bytes_to_send;
        packet_number++;
    }

    return 0;
}
