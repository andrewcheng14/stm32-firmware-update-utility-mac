#include "ota_fw_update.h"

/* Global Arrays */
uint8_t DATA_BUF[PACKET_MAX_SIZE];

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

int sendOtaData(struct sp_port* port, uint8_t* payload, uint16_t packet_num, uint16_t size) {
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
    int res = check(sp_blocking_write(port, (uint8_t*) data_packet, packet_header_size, SEND_RECEIVE_TIMEOUT_MS));
    if (res != packet_header_size) {
        printf("Timed out while sending packet header, %d/%d bytes sent.\n", res, packet_header_size);
        return -1;
    }

    // send packet payload
    res = check(sp_blocking_write(port, data_packet->payload, data_packet->payload_len, SEND_RECEIVE_TIMEOUT_MS));
    if (res != size) {
        printf("Timed out while sending packet payload, %d/%d bytes sent.\n", res, size);
        return -1;
    }

    // send packet footer
    int packet_footer_size = sizeof(data_packet->crc32) + sizeof(data_packet->eof);
    res = check(sp_blocking_write(port, (uint8_t*) &data_packet->crc32, packet_footer_size, SEND_RECEIVE_TIMEOUT_MS));
    if (res != packet_footer_size) {
        printf("Timed out while sending packet footer, %d/%d bytes sent.\n", res, packet_footer_size);
        return -1;
    }
    printf("Sent OTA data packet %d successfully.\n", packet_num);

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
