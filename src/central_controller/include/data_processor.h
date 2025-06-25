#ifndef DATA_PROCESSOR_H
#define DATA_PROCESSOR_H

#include "../../common/include/can_interface.h"
#include "../../common/include/message_type.h"
#include "../../sensor_nodes/include/sensor_common.h"
#include <stdbool.h>
#include <time.h>

#define MAX_SENSORS 32
#define DATA_HISTORY_SIZE 100
#define MAX_ALARMS 50

#pragma pack(push, 1)
// 센서 데이터 히스토리
typedef struct
{
    sensor_data_msg_t data[DATA_HISTORY_SIZE];
    int head;
    int cnt;
    time_t last_update;
} sensor_history_t;

// 중앙 제어 장치
typedef struct
{
    can_interface_t can_interface;
    sensor_history_t sensor_history[MAX_SENSORS];
    int active_sensor_cnt;
    bool is_running;

    // 통계 정보
    uint32_t total_messages_received;
    uint32_t alarm_cnt;
    time_t start_time;
} central_controller_t;

// 임계값 설정
typedef struct
{
    uint8_t sensor_id;
    float warning_high;
    float warning_low;
    float error_high;
    float error_low;
} sensor_threshold_t;
#pragma pack(pop)

// 함수 선언
can_error_t central_init(central_controller_t *controller, const char *interface_name);
can_error_t central_start_monitoring(central_controller_t *controller);
can_error_t central_stop_monitoring(central_controller_t *controller);
can_error_t central_process_can_frame(central_controller_t *controller, const char *frame);
#endif
