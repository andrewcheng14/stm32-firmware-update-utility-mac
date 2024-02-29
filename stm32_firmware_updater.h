#include <stdint.h>

#define SEND_RECEIVE_TIMEOUT_MS 1000

#define APP_FLASH_ADDR 0x08040000  // Application's flash address

#define PACKET_SOF 0x02  // Start of Frame
#define PACKET_EOF 0x03  // End of Frame
#define PACKET_ACK 0x00  // Acknowledge
#define PACKET_NACK 0x01 // Not Acknowledge

#define PACKET_MAX_PAYLOAD_SIZE 256u  // Maximum payload size for a OTA packet in bytes
#define PACKET_OVERHEAD 11 // 11 bytes used for a OTA packet's metadata (all fields except payload)
#define PACKET_MAX_SIZE (PACKET_MAX_PAYLOAD_SIZE + PACKET_OVERHEAD)  // Max size of OTA packet in bytes
#define APP_FW_MAX_SIZE (0x0807FFFF - 0x08008000)  // Sector 7 end - Sector 2 start

/* Enum definitions */
typedef enum ota_packet_type {
    COMMAND = 0,
    HEADER,
    DATA,
    RESPONSE,
} ota_packet_type_t;

typedef enum ota_command {
    OTA_START_CMD = 0,    // OTA Start command
    OTA_END_CMD   = 1,    // OTA End command
    OTA_ABORT_CMD = 2,    // OTA Abort command
} ota_command_t;

/* Struct definitions */
typedef struct OtaDataPacket {
    uint8_t sof;
    uint8_t packet_type;
    uint16_t packet_num;
    uint16_t payload_len;
    uint8_t* payload;
    uint32_t crc32;
    uint8_t eof;
}__attribute__((packed)) OtaDataPacket;

typedef struct OtaCommandPacket {
    uint8_t sof;
    uint8_t packet_type;
    uint16_t packet_num;  // "don't care" for command packet
    uint16_t payload_len;
    uint8_t cmd;
    uint32_t crc32;
    uint8_t eof;
}__attribute__((packed)) OtaCommandPacket;

typedef struct OtaResponsePacket {
    uint8_t sof;
    uint8_t packet_type;
    uint16_t packet_num;  // "don't care" for response packet
    uint16_t payload_len;
    uint8_t status;
    uint32_t crc32;
    uint8_t eof;
}__attribute__((packed)) OtaResponsePacket;
