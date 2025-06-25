#include "include/data_processor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// 기본 임계값 설정
static sensor_threshold_t defualt_thresholds[] = {
    {1, 80.0, -10.0, 100.0, -20.0},
    {2, 10.0, 0.5, 15.0, 0.0},
    {3, 20.0, 0.0, 50.0, 0.0}};

can_error_t central_init(central_controller_t *controller, const char *interface_name)
{
    if (!controller || !interface_name)
    {
        return CAN_ERROR_INVALID_PARAM;
    }

    memset(controller, 0, sizeof(central_controller_t));

    // CAN 인터페이스 생성
    can_error_t result = can_create_interface(&controller->can_interface, interface_name, 0x001);
    if (result != CAN_SUCCESS)
    {
        printf("[CENTRAL] Faild to create CAN interface\n");
        return result;
    }

    // CAN 연결
    can_set_filter(&controller->can_interface, 0x100, 0x300);

    controller->is_running = false;
    controller->start_time = time(NULL);

    printf("[CENTRAL] Central Controller '%s' initialized\n", interface_name);
    return CAN_SUCCESS;
}

// 모니터링 시작
can_error_t central_start_monitoring(central_controller_t *controller)
{
    if (!controller)
    {
        return CAN_ERROR_INVALID_PARAM;
    }

    controller->is_running = true;
    printf("[CENTRAL] Monitoring started\n");
    return CAN_SUCCESS;
}

// 모니터링 중지
can_error_t central_stop_monitoring(central_controller_t *controller)
{
    if (!controller)
    {
        return CAN_ERROR_INVALID_PARAM;
    }

    controller->is_running = false;
    printf("[CENTRAL] Monitoring stopped\n");
    return CAN_SUCCESS;
}

can_error_t central_process_can_frame(central_controller_t *controller, const can_frame_t *frame)
{
    if (!controller || !frame)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    printf("[CENTRAL] CAN Frame received - ID: 0x%03X, DLC: %d\n", frame->id, frame->dlc);

    // CAN ID 기반으로 메시지 타입 판별
    if (frame->id >= CAN_ID_TEMPERATURE_BASE && frame->id < CAN_ID_PRESSURE_BASE)
    {
        // 온도 센서 데이터 처리
    }
    else if (frame->id >= CAN_ID_PRESSURE_BASE && frame->id < CAN_ID_VIBRATION_BASE)
    {
        // 압력 센서 데이터 처리
    }
    else if (frame->id >= CAN_ID_VIBRATION_BASE && frame->id < CAN_ID_SYSTEM_BASE)
    {
        // 진동 센서 데이터 처리
    }
    else if (frame->id >= CAN_ID_SYSTEM_BASE && frame->id < 0x500)
    {
        // 시스템 데이터
    }
    else
    {
        printf("[CENTRAL] Unknown message type (ID: 0x%03X)", frame->id);
        return CAN_ERROR_INVALID_PARAM;
    }
}
// ------------- static method -------------
