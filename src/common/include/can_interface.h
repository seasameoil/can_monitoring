#ifndef CAN_INTERFACE_H
#define CAN_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define CAN_MAX_DATA_LENGTH 8
#define CAN_MAX_INTERFACES 10
#define CAN_MESSAGE_QUEUE_SIZE 1000

// CAN message struct
typedef struct
{
    uint32_t id;                       // CAN ID
    uint8_t dlc;                       // Data Length Code
    uint8_t data[CAN_MAX_DATA_LENGTH]; // 데이터 바이트
    bool is_extended;                  // 확장 프레임 여부
    bool is_remote;                    // RTR 여부
    // SYSTEMTIME timestamp;              // timestamp
} can_frame_t;

// CAN interface struct
typedef struct
{
    char interface_name[32]; // 인터페이스 이름
    uint32_t node_id;        // node ID
    bool is_connected;       // 연결 상태
    uint32_t tx_cnt;         // 전송 메시지 카운터
    uint32_t rx_cnt;         // 수신 메시지 카운터
    uint32_t err_cnt;        // 에러 카운터

    uint32_t filter_mask; // 필터 마스크
    uint32_t filter_id;   // 필터 id
    bool filter_enabled;  // 필터 사용 여부
} can_interface_t;

// CAN error code
typedef enum
{
    CAN_SUCCESS = 0,
    CAN_ERROR_INVALID_PARAM = -1,
    CAN_ERROR_NOT_CONNECTED = -2,
    CAN_ERROR_SEND_FAILED = -3,
    CAN_ERROR_RECV_FAILED = -4,
    CAN_ERROR_QUEUE_FULL = -5,
    CAN_ERROR_QUEUE_EMPTY = -6,
    CAN_ERROR_INIT_FAILED = -7
} can_error_t;

// 전역 메시지 큐 (단순한 링 버퍼)
typedef struct
{
    can_frame_t messages[CAN_MESSAGE_QUEUE_SIZE];
    int head; // 쓰기 위치
    int tail; // 읽기 위치
    int cnt;  // 현재 메시지 수
} can_message_queue_t;

// CAN 인터페이스 관리자
typedef struct
{
    can_interface_t interfaces[CAN_MAX_INTERFACES];
    int interface_cnt;
    can_message_queue_t global_queue;
    bool debug_mode;
} can_manager_t;

// function
can_error_t can_init_manager(bool debug_mode);
can_error_t can_create_interface(can_interface_t *can_interface, const char *name, uint32_t node_id);
can_error_t can_connect(can_interface_t *can_interface);
can_error_t can_disconnect(can_interface_t *can_interface);
can_error_t can_send(can_interface_t *can_interface, const can_frame_t *frame);
can_error_t can_receive(can_interface_t *can_interface, can_frame_t *frame, int timeout_ms);
can_error_t can_set_filter(can_interface_t *can_interface, uint32_t id, uint32_t mask);
void can_cleanup_manager(void);
#endif