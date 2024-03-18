#include "ota_fw_update.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Private Function Declarations */
int socketSendHelper(int sockfd, uint8_t* data, int size);
int socketReadHelper(int sockfd, uint8_t* buf, int size);

/* Globals */
uint8_t DATA_BUF[PACKET_MAX_SIZE];
int sockfd;

int openOtaConnection(uint16_t port) {
    struct sockaddr_in servaddr;

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr("192.168.4.1");

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Connection to ESP failed");
        return -1;
    }

    return 0;
}

int sendOtaStartCommand() {
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
    int res = socketSendHelper(sockfd, (uint8_t*) cmd_packet, size);
    if (res < 0) {
        close(sockfd);
        return -1;
    }
    printf("Sent OTA start command successfully.\n");

    // Wait for ACK or NACK from MCU
    printf("Waiting for ACK or NACK from MCU...\n");
    if (!isAckResponseReceived()) {
        return -1;
    }
    printf("Received ACK from MCU.\n");

    return 0;
}

int sendOtaHeader(FileInfo file_info) {
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
    int res = socketSendHelper(sockfd, (uint8_t*) header_packet, size);
    if (res < 0) {
        close(sockfd);
        return -1;
    }
    printf("Sent OTA header successfully.\n");

    // Wait for ACK or NACK from MCU
    printf("Waiting for ACK or NACK from MCU...\n");
    if (!isAckResponseReceived()) {
        return -1;
    }
    printf("Received ACK from MCU.\n");

    return 0;
}

int sendOtaData(uint8_t* payload, uint16_t packet_num, uint16_t size) {
    OtaDataPacket* data_packet = (OtaDataPacket*) DATA_BUF;

    // Build data packet to send
    memset(DATA_BUF, 0, PACKET_MAX_SIZE);
    data_packet->sof         = PACKET_SOF;
    data_packet->packet_type = DATA;
    data_packet->packet_num  = packet_num;
    data_packet->payload_len = size;
    data_packet->payload     = payload;
    data_packet->crc32       = 0;  // TBD: Implement CRC32
    data_packet->eof         = PACKET_EOF;
    
    // send packet header
    printf("\n\n\n\n***** Sending OTA data packet number: %d *****\n\n", packet_num);
    int packet_header_size = sizeof(data_packet->sof) + sizeof(data_packet->packet_type) + sizeof(data_packet->packet_num) + sizeof(data_packet->payload_len);
    int res = socketSendHelper(sockfd, (uint8_t*) data_packet, packet_header_size);
    if (res < 0) {
        printf("Error in sending data packet header.\n");
        close(sockfd);
        return -1;
    }

    // send packet payload
    res = socketSendHelper(sockfd, (uint8_t*) data_packet->payload, data_packet->payload_len);
    if (res < 0) {
        printf("Error in sending data packet payload.\n");
        close(sockfd);
        return -1;
    }

    // send packet footer
    int packet_footer_size = sizeof(data_packet->crc32) + sizeof(data_packet->eof);
    res = socketSendHelper(sockfd, (uint8_t*) &data_packet->crc32, packet_footer_size);
    if (res < 0) {
        printf("Error in sending data packet footer.\n");
        close(sockfd);
        return -1;
    }
    printf("Sent OTA data packet %d successfully.\n", packet_num);

    // Wait for ACK or NACK from MCU
    printf("Waiting for ACK or NACK from MCU...\n");
    if (!isAckResponseReceived()) {
        return -1;
    }
    printf("Received ACK from MCU.\n");

    return 0;
}

int sendOtaEndCommand() {
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
    int res = socketSendHelper(sockfd, (uint8_t*) cmd_packet, size);
    if (res < 0) {
        close(sockfd);
        return -1;
    }
    printf("Sent OTA end command successfully.\n");

    // Wait for ACK or NACK from MCU
    printf("Waiting for ACK or NACK from MCU...\n");
    if (!isAckResponseReceived()) {
        return -1;
    }
    printf("Received ACK from MCU.\n");

    return 0;
}

// int openSerialPort(char* port_name, int baud_rate, struct sp_port** port) {
//     printf("Opening port %s with %d baud rate, no parity, no control flow and 1 stop bits\n", port_name, baud_rate);

//     check(sp_get_port_by_name(port_name, port));
//     check(sp_open(*port, SP_MODE_READ_WRITE));
//     check(sp_set_baudrate(*port, baud_rate));
//     check(sp_set_bits(*port, 8));
//     check(sp_set_parity(*port, SP_PARITY_NONE));
//     check(sp_set_stopbits(*port, 1));
//     check(sp_set_flowcontrol(*port, SP_FLOWCONTROL_NONE));

//     return 0;
// }

// int check(enum sp_return result) {
//     char *error_message;
//     switch (result) {
//         case SP_ERR_ARG:
//             printf("Error: Invalid argument.\n");
//             abort();
//         case SP_ERR_FAIL:
//             error_message = sp_last_error_message();
//             printf("Error: Failed: %s\n", error_message);
//             sp_free_error_message(error_message);
//             abort();
//         case SP_ERR_SUPP:
//             printf("Error: Not supported.\n");
//             abort();
//         case SP_ERR_MEM:
//             printf("Error: Couldn't allocate memory.\n");
//             abort();
//         case SP_OK:
//         default:
//             return result;
//     }
// }

bool isAckResponseReceived() {
    uint8_t buf[PACKET_MAX_SIZE];
    int size = sizeof(OtaResponsePacket);
    int res = socketReadHelper(sockfd, buf, size);
    if (res < 0) {
        printf("Timed out while reading OTA response packet\n");
        close(sockfd);
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

int socketSendHelper(int sockfd, uint8_t* data, int size) {
    int total_sent = 0;
    while (total_sent < size) {
        int res = send(sockfd, data + total_sent, size - total_sent, 0);
        if (res < 0) {
            perror("Send failed");
            return -1;
        }
        total_sent += res;
    }

    if (total_sent == size) {
        return 0;
    } else {
        printf("Error in sending bytes, %d/%d bytes sent.\n", total_sent, size);
        return -1;
    }
}

int socketReadHelper(int sockfd, uint8_t* buf, int size) {
    int total_read = 0;
    while (total_read < size) {
        int res = recv(sockfd, buf + total_read, size - total_read, 0);
        if (res < 0) {
            perror("Receive failed");
            return -1;
        }
        total_read += res;
    }

    if (total_read == size) {
        return 0;
    } else {
        printf("Error in reading bytes, %d/%d bytes read.\n", total_read, size);
        return -1;
    }
}

// void sendUARTPacketOverTCP(const char *uartPacket, size_t packetSize) {
//     int sockfd;
//     struct sockaddr_in servaddr;

//     // Create socket
//     sockfd = socket(AF_INET, SOCK_STREAM, 0);
//     if (sockfd == -1) {
//         perror("Socket creation failed");
//         exit(EXIT_FAILURE);
//     }

//     // Set up server address
//     memset(&servaddr, 0, sizeof(servaddr));
//     servaddr.sin_family = AF_INET;
//     servaddr.sin_port = htons(PORT);
//     servaddr.sin_addr.s_addr = INADDR_ANY;

//     // Connect to server
//     if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
//         perror("Connection to ESP failed");
//         exit(EXIT_FAILURE);
//     }

//     // Send UART packet
//     if (send(sockfd, uartPacket, packetSize, 0) == -1) {
//         perror("Send failed");
//         close(sockfd);
//         return -1;
//     }

//     close(sockfd);
// }


// void receiveUARTResponseOverTCP(char *responseBuffer, size_t bufferSize) {
//     int sockfd;
//     struct sockaddr_in servaddr;
//     int bytesReceived;

//     // Create socket
//     sockfd = socket(AF_INET, SOCK_STREAM, 0);
//     if (sockfd == -1) {
//         perror("Socket creation failed");
//         exit(EXIT_FAILURE);
//     }

//     // Set up server address
//     memset(&servaddr, 0, sizeof(servaddr));
//     servaddr.sin_family = AF_INET;
//     servaddr.sin_port = htons(PORT);
//     servaddr.sin_addr.s_addr = INADDR_ANY;

//     // Connect to server
//     if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
//         perror("Connection to ESP failed");
//         exit(EXIT_FAILURE);
//     }

//     // Receive UART response
//     bytesReceived = recv(sockfd, responseBuffer, bufferSize, 0);
//     if (bytesReceived == -1) {
//         perror("Receive failed");
//         exit(EXIT_FAILURE);
//     }

//     close(sockfd);
// }
