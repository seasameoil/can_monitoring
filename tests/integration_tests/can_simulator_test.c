#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>

// 프로젝트 헤더 포함
#include "../../src/common/can_interface.c"
#include "../../src/common/include/message_type.h"
#include "../../src/simulator/can_simulator.c"

// 전역 변수
static can_simulator_t g_simulator;
static can_interface_t g_controller_interface;
static bool g_running = true;

extern can_manager_t g_can_manager;

// 시그널 핸들러 (Ctrl+C 처리)
void signal_handler(int sig)
{
    printf("\n[TEST] Shutdown signal received...\n");
    g_running = false;
}

// 수신된 메시지 파싱 및 출력
void parse_and_display_message(const can_frame_t *frame)
{
    printf("\n=== CAN Message Received ===\n");
    printf("ID: 0x%03X, DLC: %d\n", frame->id, frame->dlc);
    printf("Raw Data: ");
    for (int i = 0; i < frame->dlc; i++)
    {
        printf("%02X ", frame->data[i]);
    }
    printf("\n");

    // 메시지 타입별 파싱
    if (frame->dlc >= 2)
    {
        uint8_t msg_type = frame->data[1];

        switch (msg_type)
        {
        case MSG_TYPE_SENSOR_DATA:
        {
            if (frame->dlc >= sizeof(sensor_data_msg_t))
            {
                sensor_data_msg_t *sensor_msg = (sensor_data_msg_t *)frame->data;
                float value = (float)sensor_msg->value / 100.0f;

                printf("📊 SENSOR DATA:\n");
                printf(" Sensor ID: %d\n", sensor_msg->sensor_id);
                printf(" Value: %.2f", value);

                switch (sensor_msg->unit)
                {
                case UNIT_CELSIUS:
                    printf(" °C");
                    break;
                case UNIT_BAR:
                    printf(" bar");
                    break;
                case UNIT_MM_S:
                    printf(" mm/s");
                    break;
                default:
                    printf(" (unknown unit)");
                    break;
                }
                printf("\n");

                printf(" Status: ");
                switch (sensor_msg->status)
                {
                case SENSOR_OK:
                    printf("OK ✅");
                    break;
                case SENSOR_WARNING:
                    printf("WARNING ⚠️");
                    break;
                case SENSOR_ERROR:
                    printf("ERROR ❌");
                    break;
                case SENSOR_OFFLINE:
                    printf("OFFLINE 📴");
                    break;
                default:
                    printf("UNKNOWN");
                    break;
                }
                printf("\n");
                printf(" Sequence: %d\n", sensor_msg->sequence);
            }
            break;
        }

        case MSG_TYPE_ALARM:
        {
            if (frame->dlc >= sizeof(alarm_msg_t))
            {
                alarm_msg_t *alarm_msg = (alarm_msg_t *)frame->data;
                float current_val = (float)alarm_msg->current_value / 100.0f;
                float threshold_val = (float)alarm_msg->threshold_value / 100.0f;

                printf("🚨 ALARM:\n");
                printf(" Sensor ID: %d\n", alarm_msg->sensor_id);
                printf(" Level: %d", alarm_msg->alarm_level);
                if (alarm_msg->alarm_level >= 3)
                    printf(" (ERROR)");
                else
                    printf(" (WARNING)");
                printf("\n");
                printf(" Code: 0x%02X\n", alarm_msg->alarm_code);
                printf(" Current: %.2f, Threshold: %.2f\n", current_val, threshold_val);
            }
            break;
        }

        case MSG_TYPE_HEARTBEAT:
        {
            if (frame->dlc >= sizeof(status_msg_t))
            {
                status_msg_t *status_msg = (status_msg_t *)frame->data;

                printf("💓 HEARTBEAT:\n");
                printf(" Node ID: %d\n", status_msg->node_id);
                printf(" System Status: %s\n", status_msg->system_status ? "ACTIVE" : "INACTIVE");
                printf(" Error Flags: 0x%02X\n", status_msg->error_flags);
                printf(" Uptime: %u ms\n", status_msg->uptime);
            }
            break;
        }

        default:
            printf("❓ UNKNOWN MESSAGE TYPE: 0x%02X\n", msg_type);
            break;
        }
    }
    printf("============================\n\n");
}

// 통계 정보 출력
void print_statistics()
{
    printf("\n📈 === STATISTICS ===\n");
    printf("Controller Interface:\n");
    printf(" RX Count: %u\n", g_controller_interface.rx_cnt);
    printf(" TX Count: %u\n", g_controller_interface.tx_cnt);
    printf(" Error Count: %u\n", g_controller_interface.err_cnt);

    printf("\nGlobal Message Queue:\n");
    printf(" Messages in Queue: %d\n", g_can_manager.global_queue.cnt);
    printf(" Queue Head: %d\n", g_can_manager.global_queue.head);
    printf(" Queue Tail: %d\n", g_can_manager.global_queue.tail);

    printf("\nSensor Status:\n");
    for (int i = 0; i < g_simulator.sensor_cnt; i++)
    {
        virtual_sensor_t *sensor = &g_simulator.sensors[i];
        printf(" Sensor %d: %.2f ", sensor->sensor_id, sensor->current_value);
        switch (sensor->type)
        {
        case SENSOR_TYPE_TEMPERATURE:
            printf("°C");
            break;
        case SENSOR_TYPE_PRESSURE:
            printf("bar");
            break;
        case SENSOR_TYPE_VIBRATION:
            printf("mm/s");
            break;
        }
        printf(" [%s]\n", sensor->status == SENSOR_OK ? "OK" : sensor->status == SENSOR_WARNING ? "WARN"
                                                                                                : "ERROR");
    }
    printf("====================\n\n");
}

int main()
{
    can_error_t result;

    printf("CAN Simulator Test\n");
    printf("=========================\n");

    // 시그널 핸들러 등록
    signal(SIGINT, signal_handler);

    // 1. CAN 관리자 초기화
    printf("1️⃣ Initializing CAN Manager...\n");
    result = can_init_manager(true); // 디버그 모드 활성화
    if (result != CAN_SUCCESS)
    {
        printf("❌ Failed to initialize CAN manager: %d\n", result);
        return -1;
    }
    printf("✅ CAN Manager initialized\n\n");

    // 2. 컨트롤러 인터페이스 생성
    printf("2️⃣ Creating Controller Interface...\n");
    result = can_create_interface(&g_controller_interface, "Controller", 0x001);
    if (result != CAN_SUCCESS)
    {
        printf("❌ Failed to create controller interface: %d\n", result);
        return -1;
    }

    result = can_connect(&g_controller_interface);
    if (result != CAN_SUCCESS)
    {
        printf("❌ Failed to connect controller interface: %d\n", result);
        return -1;
    }
    printf("✅ Controller interface created and connected\n\n");

    // 3. 시뮬레이터 초기화
    printf("3️⃣ Initializing Simulator...\n");
    result = sim_init(&g_simulator);
    if (result != CAN_SUCCESS)
    {
        printf("❌ Failed to initialize simulator: %d\n", result);
        return -1;
    }
    printf("✅ Simulator initialized\n\n");

    // 4. 센서들 추가
    printf("4️⃣ Adding Virtual Sensors...\n");

    // 온도 센서 (25°C 기준, ±10°C 변동)
    result = sim_add_temperature_sensor(&g_simulator, 1, 25.0f, 20.0f);
    if (result != CAN_SUCCESS)
    {
        printf("❌ Failed to add temperature sensor: %d\n", result);
        return -1;
    }

    // 압력 센서 (5 bar 기준, ±3 bar 변동)
    result = sim_add_pressure_sensor(&g_simulator, 2, 5.0f, 6.0f);
    if (result != CAN_SUCCESS)
    {
        printf("❌ Failed to add pressure sensor: %d\n", result);
        return -1;
    }

    // 진동 센서 (2 mm/s 기준, ±4 mm/s 변동)
    result = sim_add_vibration_sensor(&g_simulator, 3, 2.0f, 8.0f);
    if (result != CAN_SUCCESS)
    {
        printf("❌ Failed to add vibration sensor: %d\n", result);
        return -1;
    }

    printf("✅ Added %d virtual sensors\n\n", g_simulator.sensor_cnt);

    // 5. 시뮬레이터 시작
    printf("5️⃣ Starting Simulator...\n");
    result = sim_start(&g_simulator);
    if (result != CAN_SUCCESS)
    {
        printf("❌ Failed to start simulator: %d\n", result);
        return -1;
    }
    printf("✅ Simulator started with %d sensors\n\n", g_simulator.sensor_cnt);

    // 6. 메인 루프 - 메시지 수신 및 처리
    printf("6️⃣ Starting Message Reception Loop...\n");
    printf("Press Ctrl+C to stop the test\n\n");

    time_t last_stats_time = time(NULL);
    int message_count = 0;

    while (g_running)
    {
        can_frame_t frame;

        // 논블로킹 수신 시도
        result = can_receive(&g_controller_interface, &frame, 0);

        if (result == CAN_SUCCESS)
        {
            message_count++;
            parse_and_display_message(&frame);

            // 너무 많은 메시지가 출력되지 않도록 제한
            if (message_count >= 10)
            {
                printf("⏸️ Pausing output to prevent spam...\n");
                SLEEP_MS(20000);
                message_count = 0;
            }
        }

        // 10초마다 통계 출력
        time_t current_time = time(NULL);
        if (current_time - last_stats_time >= 5)
        {
            print_statistics();
            last_stats_time = current_time;

            break;
        }

        SLEEP_MS(10000); // 100ms 대기
        break;
    }

    // 7. 정리
    printf("\n7️⃣ Cleaning up...\n");

    sim_cleanup(&g_simulator);
    printf("✅ Simulator cleaned up\n");

    can_disconnect(&g_controller_interface);
    printf("✅ Controller interface disconnected\n");

    can_cleanup_manager();
    printf("✅ CAN Manager cleaned up\n");

    printf("\n🏁 Test completed successfully!\n");
    return 0;
}