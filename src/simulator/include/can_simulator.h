#ifndef CAN_SIMULATOR_H
#define CAN_SIMULATOR_H

#include "../../common/include/can_interface.h"
#include "../../common/include/message_type.h"
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define SLEEP_MS(ms) Sleep(ms)
typedef HANDLE thread_handle_t;
typedef CRITICAL_SECTION mutex_t;
#else
#include <pthread.h>
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
typedef pthread_t thread_handle_t;
typedef pthread_mutex_t mutex_t;
#endif

#define MAX_SENSOR_NODES 8
#define SIMULATION_INVERVAL_MS 1000 // 1초마다 데이터 생성
#define HEARTBEAT_INTERVAL_MS 5000  // 5초마다 하트비트

// 센서 노드 타입
typedef enum
{
    SENSOR_TYPE_TEMPERATURE = 0,
    SENSOR_TYPE_PRESSURE = 1,
    SENSOR_TYPE_VIBRATION = 2
} sensor_type_t;

// 센서 시뮬레이션 파라미터
typedef struct
{
    float base_value;  // 기본값
    float amplitude;   // 변동폭
    float frequency;   // 주파수 (Hz)
    float noise_level; // 노이즈 레벨
    float drift_rate;  // 드리프트 비율
} sensor_sim_params_t;

// 가상 센서 노드
typedef struct
{
    uint8_t sensor_id;
    sensor_type_t type;
    uint32_t can_id_base;
    sensor_sim_params_t params;

    // 런타임 상태
    bool is_active;
    uint16_t sequence;
    uint32_t uptime;
    sensor_status_t status;
    float current_value;
    float accumulated_drift;

    // 알람 설정
    float warning_threshold_high;
    float warning_threshold_low;
    float error_threshold_high;
    float error_threshold_low;

    can_interface_t *can_interface;

    // 스레드 관리
    thread_handle_t thread;
    volatile bool thread_running;

#ifdef _WIN32
    DWORD thread_id;
#endif
} virtual_sensor_t;

typedef struct
{
    virtual_sensor_t sensors[MAX_SENSOR_NODES];
    int sensor_cnt;
    bool is_running;
    mutex_t sim_mutex;
} can_simulator_t;

// function
can_error_t sim_init(can_simulator_t *simulator);
can_error_t sim_add_temperature_sensor(can_simulator_t *simulator, uint8_t sensor_id, float base_temp, float temp_range);
can_error_t sim_add_pressure_sensor(can_simulator_t *simulator, uint8_t sensor_id, float base_pressure, float pressure_range);
can_error_t sim_add_vibration_sensor(can_simulator_t *simulator, uint8_t sensor_id, float base_vibration, float vibration_range);
can_error_t sim_start(can_simulator_t *simulator);
can_error_t sim_stop(can_simulator_t *simulator);
void sim_cleanup(can_simulator_t *simulator);
void sim_set_alarm_thresholds(virtual_sensor_t *sensor, float warn_high, float warn_low, float err_high, float err_low);
#endif
