#include "include/can_simulator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define M_PI 3.14159265358979323846

// 외부 CAN 관리자 참조
extern can_manager_t g_can_manager;
// 전역 센서 인터페이스들 (각 센서마다 하나씩)
static can_interface_t sensor_interfaces[MAX_SENSOR_NODES];
static int interface_cnt = 0;

#ifdef _WIN32
static void mutex_init(mutex_t *mutex)
{
    InitializeCriticalSection(mutex);
}

static void mutex_destroy(mutex_t *mutex)
{
    DeleteCriticalSection(mutex);
}

static void mutex_lock(mutex_t *mutex)
{
    EnterCriticalSection(mutex);
}

static void mutex_unlock(mutex_t *mutex)
{
    LeaveCriticalSection(mutex);
}
#else
static void mutex_init(mutex_t *mutex)
{
    pthread_mutex_init(mutex, NULL);
}

static void mutex_destroy(mutex_t *mutex)
{
    pthread_mutex_destroy(mutex);
}

static void mutex_lock(mutex_t *mutex)
{
    pthread_mutex_lock(mutex);
}

static void mutex_unlock(mutex_t *mutex)
{
    pthread_mutex_unlock(mutex);
}
#endif

// 시간 유틸리티
static uint32_t get_uptime_seconds()
{
    static time_t start_time = 0;

    if (start_time == 0)
    {
        start_time = time(NULL);
    }
    return (uint32_t)(time(NULL) - start_time);
}

// 밀리초 단위 시간 측정 (Windows 고해상도)
#ifdef _WIN32
static DWORD get_tick_count_ms()
{
    return GetTickCount();
}
#else
static uint32_t get_tick_count_ms()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
#endif

// 센서 인터페이스 초기화 함수
static can_error_t init_sensor_interface(virtual_sensor_t *sensor)
{
    if (interface_cnt >= MAX_SENSOR_NODES)
    {
        return CAN_ERROR_INIT_FAILED;
    }

    char interface_name[32];
    snprintf(interface_name, sizeof(interface_name), "Sensor %d", sensor->sensor_id);

    can_interface_t *can_interface = &sensor_interfaces[interface_cnt];
    can_error_t result = can_create_interface(can_interface, interface_name, sensor->sensor_id);

    result = can_connect(can_interface);
    if (result != CAN_SUCCESS)
    {
        return result;
    }

    // 센서에 인터페이스 포인터 저장
    sensor->can_interface = can_interface;
    interface_cnt++;

    return CAN_SUCCESS;
}

// 센서 데이터 생성 함수
static float generate_sensor_value(virtual_sensor_t *sensor, float time_sec)
{
    float base = sensor->params.base_value;
    float amplitude = sensor->params.amplitude;
    float frequency = sensor->params.frequency;
    float noise = sensor->params.noise_level;

    // 사인파 기반 주기적 변화
    float periodic = amplitude * sin(2.0 * M_PI * frequency * time_sec);
    float random_noise = noise * ((float)rand() / RAND_MAX - 0.5);

    // 드리프트 (장기적 변화)
    sensor->accumulated_drift += sensor->params.drift_rate * ((float)rand() / RAND_MAX - 0.5);

    return base + periodic + random_noise + sensor->accumulated_drift;
}

// 센서 상태 판단
static sensor_status_t evaluate_sensor_status(virtual_sensor_t *sensor, float value)
{
    if (value > sensor->error_threshold_high || value < sensor->error_threshold_low)
    {
        return SENSOR_ERROR;
    }
    else if (value > sensor->warning_threshold_high || value < sensor->warning_threshold_low)
    {
        return SENSOR_WARNING;
    }

    return SENSOR_OK;
}

static void send_sensor_data(virtual_sensor_t *sensor)
{
    can_frame_t frame;
    sensor_data_msg_t sensor_msg;

    sensor_msg.sensor_id = sensor->sensor_id;
    sensor_msg.msg_type = MSG_TYPE_SENSOR_DATA;
    sensor_msg.value = (uint16_t)(sensor->current_value * 100); // resolution (0.01)
    sensor_msg.status = sensor->status;
    sensor_msg.sequence = sensor->sequence++;

    // 단위 설정
    switch (sensor->type)
    {
    case SENSOR_TYPE_TEMPERATURE:
        sensor_msg.unit = UNIT_CELSIUS;
        break;
    case SENSOR_TYPE_PRESSURE:
        sensor_msg.unit = UNIT_BAR;
        break;
    case SENSOR_TYPE_VIBRATION:
        sensor_msg.unit = UNIT_MM_S;
        break;
    }

    // can frame 구성
    frame.id = sensor->can_id_base + sensor->sensor_id;
    frame.dlc = sizeof(sensor_data_msg_t);
    frame.is_extended = false;
    frame.is_remote = false;
    memcpy(frame.data, &sensor_msg, sizeof(sensor_data_msg_t));

    // can_send() 함수를 통해 전송
    can_error_t result = can_send(sensor->can_interface, &frame);
    if (result != CAN_SUCCESS && g_can_manager.debug_mode)
    {
        printf("[SIM ERROR] Failed to send sensor data for sensor %d: %d\n", sensor->sensor_id, result);
    }
}

static void send_alaram_data(virtual_sensor_t *sensor, uint8_t alarm_level, uint8_t alarm_code)
{
    can_frame_t frame;
    alarm_msg_t alarm_msg;

    alarm_msg.sensor_id = sensor->sensor_id;
    alarm_msg.msg_type = MSG_TYPE_ALARM;
    alarm_msg.alarm_level = alarm_level;
    alarm_msg.alarm_code = alarm_code;
    alarm_msg.current_value = (int16_t)(sensor->current_value * 100);

    // 임계값 설정
    if (alarm_level >= 3)
    { // error level
        alarm_msg.threshold_value = (sensor->current_value > 0) ? (int16_t)(sensor->error_threshold_high * 100) : (int16_t)(sensor->error_threshold_low * 100);
    }
    else
    { // warning level
        alarm_msg.threshold_value = (sensor->current_value > 0) ? (int16_t)(sensor->warning_threshold_high * 100) : (int16_t)(sensor->warning_threshold_low * 100);
    }

    frame.id = CAN_ID_SYSTEM_BASE + sensor->sensor_id;
    frame.dlc = sizeof(alarm_msg_t);
    frame.is_extended = false;
    frame.is_remote = false;

    // can_send() 함수를 통해 전송
    can_error_t result = can_send(sensor->can_interface, &frame);
    if (result != CAN_SUCCESS && g_can_manager.debug_mode)
    {
        printf("[SIM ERROR] Failed to send sensor data for sensor %d: %d\n", sensor->sensor_id, result);
    }
}

static void send_status_message(virtual_sensor_t *sensor)
{
    can_frame_t frame;
    status_msg_t status_msg;

    status_msg.node_id = sensor->sensor_id;
    status_msg.msg_type = MSG_TYPE_HEARTBEAT;
    status_msg.system_status = sensor->is_active;
    status_msg.error_flags = (sensor->status == SENSOR_ERROR) ? 0x01 : 0x00;
    status_msg.uptime = get_tick_count_ms();

    frame.id = CAN_ID_SYSTEM_BASE + 0x10 + sensor->sensor_id;
    frame.dlc = sizeof(status_msg_t);
    frame.is_extended = false;
    frame.is_remote = false;
    memcpy(frame.data, &status_msg, sizeof(status_msg_t));

    // can_send() 함수를 통해 전송
    can_error_t result = can_send(sensor->can_interface, &frame);
    if (result != CAN_SUCCESS && g_can_manager.debug_mode)
    {
        printf("[SIM ERROR] Failed to send sensor data for sensor %d: %d\n", sensor->sensor_id, result);
    }
}

// 센서 스레드 함수
#ifdef _WIN32
static DWORD WINAPI sensor_thread_func(LPVOID arg)
#else
static void *sensor_thread_func(void *arg)
#endif
{
    virtual_sensor_t *sensor = (virtual_sensor_t *)arg;
    DWORD start_time = time(NULL);
    DWORD last_heartbeat = start_time;
    sensor_status_t prev_status = SENSOR_OK;

    printf("[SIM] Sensor %d thread started\n", sensor->sensor_id);

    while (sensor->thread_running)
    {
        DWORD current_time = time(NULL);
        float elapsed_time = (float)(current_time - start_time);

        // 센서값 설정
        sensor->current_value = generate_sensor_value(sensor, elapsed_time);

        // 상태 평가
        sensor->status = evaluate_sensor_status(sensor, sensor->current_value);

        // 상태 변화 시 알람 전송
        if (sensor->status != prev_status)
        {
            if (sensor->status == SENSOR_WARNING)
            {
                send_alaram_data(sensor, 2, 0x01);
            }
            else if (sensor->status == SENSOR_ERROR)
            {
                send_alaram_data(sensor, 4, 0x02);
            }
            prev_status = sensor->status;
        }

        // 센서 데이터 전송
        send_sensor_data(sensor);

        // 하트비트 전송 (5초마다)
        if (current_time - last_heartbeat >= 5)
        {
            send_status_message(sensor);
            last_heartbeat = current_time;
        }

        // 1초 대기
        SLEEP_MS(1);
    }

    printf("[SIM] Sensor %d thread stopped\n", sensor->sensor_id);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

// 시뮬레이터 초기화
can_error_t sim_init(can_simulator_t *simulator)
{
    if (!simulator)
        return CAN_ERROR_INVALID_PARAM;

    memset(simulator, 0, sizeof(can_simulator_t));
    simulator->sensor_cnt = 0;
    simulator->is_running = false;

    mutex_init(&simulator->sim_mutex);

    srand((unsigned int)time(NULL));

    printf("[SIM] Simulator initialized\n");
    return CAN_SUCCESS;
}

can_error_t sim_add_temperature_sensor(can_simulator_t *simulator, uint8_t sensor_id, float base_temp, float temp_range)
{
    if (!simulator || simulator->sensor_cnt >= MAX_SENSOR_NODES)
    {
        return CAN_ERROR_INVALID_PARAM;
    }

    virtual_sensor_t *sensor = &simulator->sensors[simulator->sensor_cnt];
    memset(sensor, 0, sizeof(virtual_sensor_t));

    sensor->sensor_id = sensor_id;
    sensor->type = SENSOR_TYPE_TEMPERATURE;
    sensor->can_id_base = CAN_ID_TEMPERATURE_BASE;
    sensor->is_active = true;
    sensor->sequence = 0;

    // 시뮬레이터 파라미터 설정
    sensor->params.base_value = base_temp;
    sensor->params.amplitude = temp_range / 2.0f;
    sensor->params.frequency = 0.01f;
    sensor->params.noise_level = temp_range * 0.1f;
    sensor->params.drift_rate = 0.001f;

    // 알림 임계값 설정
    sim_set_alarm_thresholds(sensor,
                             base_temp + temp_range * 0.7f,
                             base_temp - temp_range * 0.7f,
                             base_temp + temp_range * 0.9f,
                             base_temp - temp_range * 0.9f);

    // 센서 인터페이스 초기화
    can_error_t result = init_sensor_interface(sensor);
    if (result != CAN_SUCCESS && g_can_manager.debug_mode)
    {
        printf("[SIM ERROR] Failed to send sensor data for sensor %d: %d\n", sensor_id, result);
    }

    simulator->sensor_cnt++;

    printf("[SIM] Temperature sensor %d added (Base: %.1fC)\n", sensor->sensor_id, base_temp);
    return CAN_SUCCESS;
}

// 압력 센서 추가
can_error_t sim_add_pressure_sensor(can_simulator_t *simulator, uint8_t sensor_id, float base_pressure, float pressure_range)
{
    if (!simulator || simulator->sensor_cnt >= MAX_SENSOR_NODES)
    {
        return CAN_ERROR_INVALID_PARAM;
    }

    virtual_sensor_t *sensor = &simulator->sensors[simulator->sensor_cnt];
    memset(sensor, 0, sizeof(virtual_sensor_t));

    sensor->sensor_id = sensor_id;
    sensor->type = SENSOR_TYPE_PRESSURE;
    sensor->can_id_base = CAN_ID_PRESSURE_BASE;
    sensor->is_active = true;
    sensor->sequence = 0;

    sensor->params.base_value = base_pressure;
    sensor->params.amplitude = pressure_range / 3.0f;
    sensor->params.frequency = 0.05f;
    sensor->params.noise_level = pressure_range * 0.05f;
    sensor->params.drift_rate = 0.002f;

    sim_set_alarm_thresholds(sensor,
                             base_pressure + pressure_range * 0.8f,
                             base_pressure - pressure_range * 0.3f,
                             base_pressure + pressure_range * 0.95f,
                             base_pressure - pressure_range * 0.1f);

    // 센서 인터페이스 초기화
    can_error_t result = init_sensor_interface(sensor);
    if (result != CAN_SUCCESS && g_can_manager.debug_mode)
    {
        printf("[SIM ERROR] Failed to send sensor data for sensor %d: %d\n", sensor_id, result);
    }

    simulator->sensor_cnt++;

    printf("[SIM] Pressure sensor %d added (Base: %.1f bar)\n", sensor->sensor_id, base_pressure);
    return CAN_SUCCESS;
}

can_error_t sim_add_vibration_sensor(can_simulator_t *simulator, uint8_t sensor_id, float base_vibration, float vibration_range)
{
    if (!simulator || simulator->sensor_cnt >= MAX_SENSOR_NODES)
    {
        return CAN_ERROR_INVALID_PARAM;
    }

    virtual_sensor_t *sensor = &simulator->sensors[simulator->sensor_cnt];
    memset(sensor, 0, sizeof(virtual_sensor_t));

    sensor->sensor_id = sensor_id;
    sensor->type = SENSOR_TYPE_VIBRATION;
    sensor->can_id_base = CAN_ID_VIBRATION_BASE;
    sensor->is_active = true;
    sensor->sequence = 0;

    sensor->params.base_value = base_vibration;
    sensor->params.amplitude = vibration_range / 2.0f;
    sensor->params.frequency = 0.1f;
    sensor->params.noise_level = vibration_range * 0.2f;
    sensor->params.drift_rate = 0.0005f;

    sim_set_alarm_thresholds(sensor,
                             base_vibration + vibration_range * 0.6f, 0,
                             base_vibration + vibration_range * 0.8f, 0);

    // 센서 인터페이스 초기화
    can_error_t result = init_sensor_interface(sensor);
    if (result != CAN_SUCCESS && g_can_manager.debug_mode)
    {
        printf("[SIM ERROR] Failed to send sensor data for sensor %d: %d\n", sensor_id, result);
    }

    simulator->sensor_cnt++;

    printf("[SIM] Vibration sensor %d added (Base: %.1f mm/s)\n", sensor->sensor_id, base_vibration);
    return CAN_SUCCESS;
}

can_error_t sim_start(can_simulator_t *simulator)
{
    if (!simulator || simulator->is_running)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    simulator->is_running = true;

    // 각 센서 스레드 시작
    for (int i = 0; i < simulator->sensor_cnt; i++)
    {
        virtual_sensor_t *sensor = &simulator->sensors[i];
        sensor->thread_running = true;

#ifdef _WIN32
        sensor->thread = CreateThread(
            NULL,
            0,
            sensor_thread_func,
            sensor,
            0,
            &sensor->thread_id);

        if (sensor->thread == NULL)
        {
            printf("[SIM ERROR] Failed to create thread for sensor %d (Error: %lu)\n",
                   sensor->sensor_id, GetLastError());
            sensor->thread_running = false;
            return CAN_ERROR_INIT_FAILED;
        }
#else
        if (pthread_create(&sensor->thread, NULL, sensor_thread_func, sensor) != 0)
        {
            printf("[SIM ERROR] Failed to create thread for sensor %d \n", sensor->sensor_id);
            sensor->thread_running = false;
            return CAN_ERROR_INIT_FAILED;
        }
#endif
    }

    printf("[SIM] Simulator started with %d sensors\n", simulator->sensor_cnt);
    return CAN_SUCCESS;
}

can_error_t sim_stop(can_simulator_t *simulator)
{
    if (!simulator || simulator->is_running)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    simulator->is_running = false;

    // 각 센서 스레드 종료
    for (int i = 0; i < simulator->sensor_cnt; i++)
    {
        virtual_sensor_t *sensor = &simulator->sensors[i];
        sensor->thread_running = false;

#ifdef _WIN32
        if (sensor->thread != NULL)
        {
            WaitForSingleObject(sensor->thread, INFINITE);
            CloseHandle(sensor->thread);
            sensor->thread = NULL;
        }
#else
        if (sensor->thread)
        {
            pthread_join(sensor->thread, NULL);
        }
#endif
    }

    printf("[SIM] Simulator stopped\n");
    return CAN_SUCCESS;
}

void sim_cleanup(can_simulator_t *simulator)
{
    if (!simulator)
        return;

    sim_stop(simulator);

    // 센서 인터페이스를 정의
    for (int i = 0; i < interface_cnt; i++)
    {
        can_disconnect(&sensor_interfaces[i]);
    }
    interface_cnt = 0;

    mutex_destroy(&simulator->sim_mutex);
    memset(simulator, 0, sizeof(can_simulator_t));

    printf("[SIM] Simulator cleaned up\n");
}

// 알람 임계값 설정
void sim_set_alarm_thresholds(virtual_sensor_t *sensor, float warn_high, float warn_low, float err_high, float err_low)
{
    sensor->warning_threshold_high = warn_high;
    sensor->warning_threshold_low = warn_low;
    sensor->error_threshold_high = err_high;
    sensor->error_threshold_low = err_low;
}