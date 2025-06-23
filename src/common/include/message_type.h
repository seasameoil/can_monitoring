#ifndef MESSAGE_TYPES_H
#define MESSAGE_TYPES_H

#include "can_interface.h"

// 산업용 센서 CAN ID 정의
#define CAN_ID_TEMPERATURE_BASE 0X100
#define CAN_ID_PRESSURE_BASE 0X200
#define CAN_ID_VIBRATION_BASE 0X300
#define CAN_ID_SYSTEM_BASE 0X400

// 메시지 타입
typedef enum
{
    MSG_TYPE_SENSOR_DATA = 0x01,
    MSG_TYPE_ALARM = 0x02,
    MSG_TYPE_STATUS = 0x03,
    MSG_TYPE_CONFIG = 0x04,
    MSG_TYPE_HEARTBEAT = 0x05
} message_type_t;

#pragma pack(push, 1)
// 센서 데이터 구조체
typedef struct
{
    uint8_t sensor_id; // 센서 아이디
    uint8_t msg_type;  // 메시지 타입
    int16_t value;     // 센서값 (resolution: 0.01)
    uint8_t unit;      // 단위코드
    uint8_t status;    // 센서 상태
    uint16_t sequence; // 시퀀스 번호
} sensor_data_msg_t;

// 알람 메시지 구조체
typedef struct
{
    uint8_t sensor_id;       // 센서 아이디
    uint8_t msg_type;        // 메시지 타입
    uint8_t alarm_level;     // 알람 레벨 (1-5)
    uint8_t alarm_code;      // 알람 코드
    int16_t current_value;   // 현재 값
    int16_t threshold_value; // 임계 값
} alarm_msg_t;

// 상태 메시지 구조체
typedef struct
{
    uint8_t node_id;       // 센서 아이디
    uint8_t msg_type;      // 메시지 타입
    uint8_t system_status; // 시스템 상태
    uint8_t error_flags;   // 에러 플래그
    uint32_t uptime;       // 가동시간
} status_msg_t;
#pragma pack(pop)

// 단위 코드
typedef enum
{
    UNIT_CELSIUS = 0x01,    // 섭씨
    UNIT_FAHRENHEIT = 0x02, // 화씨
    UNIT_BAR = 0x10,        // 바 (압력)
    UNIT_PSI = 0x11,        // psi (압력)
    UNIT_MM_S = 0x20,       // mm/s (진동)
    UNIT_G = 0x21           // G (진동)
} sensor_unit_t;

// 센서 상태
typedef enum
{
    SENSOR_OK = 0x00,
    SENSOR_WARNING = 0x01,
    SENSOR_ERROR = 0x02,
    SENSOR_OFFLINE = 0x03
} sensor_status_t;
#endif