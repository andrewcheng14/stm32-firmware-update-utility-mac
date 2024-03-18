#include "ota_fw_update.h"

#define PORT 8080u

/* Globals */
uint8_t APP_BIN[APP_FW_MAX_SIZE];

/* 
 * Private Helper Functions 
*/
int sendBinaryFile(uint32_t size);

int main(int argc, char* argv[]) {
    char bin_path[FILE_NAME_MAX_LEN];

    /* get binary file path from CLI */
    if (argc != 2) {
        printf("Usage: ./stm32_firmware_updater <PATH/TO/FIRMWARE/BIN>");
        exit(EXIT_FAILURE);
    }
    strcpy(bin_path, argv[1]);

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
    if (openOtaConnection(PORT)) {
        printf("Error opening TCP connection!\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    printf("Successfully opened TCP connection with port %d\n", PORT);

    /* send OTA start command */
    if (sendOtaStartCommand()) {
        printf("Error sending OTA start command!\n");
        fclose(file);
        return EXIT_FAILURE;
    }

    /* send OTA Header */
    if (sendOtaHeader(file_info)) {
        printf("Error sending OTA header!\n");
        fclose(file);
        return EXIT_FAILURE;
    }

    /* send OTA data */
    if (fread(APP_BIN, 1, file_info.file_size, file) != file_info.file_size) {
        perror("Error reading file");
        fclose(file);
        return -1;
    }
    if (sendBinaryFile(file_info.file_size)) {
        printf("Error sending binary file!\n");
        fclose(file);
        return EXIT_FAILURE;
    }

    /* send OTA end command */
    if (sendOtaEndCommand() < 0) {
        printf("Error sending OTA end command!\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    printf("Firmware update complete yippie! :D\n");

    // close ports and free resources
    fclose(file);

    return EXIT_SUCCESS;
}

int sendBinaryFile(uint32_t size) {
    int packet_number = 1;
    int bytes_remaining = size;
    int total_bytes_sent = 0;
    int total_data_packets_to_send = (size / PACKET_MAX_PAYLOAD_SIZE) + 1;

    while (bytes_remaining > 0) {
        int bytes_to_send = (bytes_remaining > PACKET_MAX_PAYLOAD_SIZE) ? PACKET_MAX_PAYLOAD_SIZE : bytes_remaining;
        if (sendOtaData(APP_BIN + total_bytes_sent, packet_number, bytes_to_send) < 0) {
            printf("Error sending OTA data packet %d\n", packet_number);
            return -1;
        }
        printf("Sent OTA data packet %d (%d/%d)\n", packet_number, packet_number, total_data_packets_to_send);
        bytes_remaining -= bytes_to_send;
        total_bytes_sent += bytes_to_send;
        packet_number++;
    }

    return 0;
}
