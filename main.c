// #include <stdio.h>
// #include <stdlib.h>
// #include <stdbool.h>
// #include <signal.h>
// #include <time.h>

// // 프로젝트 헤더들
// #include "src/common/include/can_interface.h"
// #include "src/common/include/message_type.h"
// #include "src/simulator/include/can_simulator.h"
// #include "src/central_controller/include/data_processor.h"

// // 전역 변수들
// static volatile bool g_running = true;
// static can_simulator_t g_simulator;
// static data_processor_t g_data_processor;
// static can_interface_t g_monitoring_interface;

// // 신호 핸들러 (Ctrl+C 처리)
// void signal_handler(int sig)
// {
//     printf("\n[MAIN] Shutting down...\n");
//     g_running = false;
// }

// // 콜백 함수들 정의
// void on_sensor_data_received(uint8_t sensor_id, float value, sensor_status_t status)
// {
//     printf("[CALLBACK] Sensor %d: %.2f (Status: %s)\n",
//            sensor_id, value, dp_get_sensor_status_string(status));

//     // 여기서 웹 대시보드로 데이터 전송하거나
//     // 데이터베이스에 저장하는 로직 추가 가능
// }

// void on_alarm_raised(alarm_record_t *alarm)
// {
//     printf("[ALARM] Sensor %d - Level %d: %.2f (Threshold: %.2f)\n",
//            alarm->sensor_id, alarm->alarm_level,
//            alarm->current_value, alarm->threshold_value);
// }

// void on_sensor_offline(uint8_t sensor_id)
// {
//     printf("[WARNING] Sensor %d is OFFLINE!\n", sensor_id);
// }

// // 시스템 초기화
// int initialize_system()
// {
//     printf("[MAIN] Initializing CAN monitoring system...\n");

//     // 1. CAN 관리자 초기화
//     if (can_init_manager(true) != CAN_SUCCESS)
//     {
//         printf("[ERROR] Failed to initialize CAN manager\n");
//         return -1;
//     }

//     // 2. 모니터링용 CAN 인터페이스 생성
//     if (can_create_interface(&g_monitoring_interface, "monitor", 0xFF) != CAN_SUCCESS)
//     {
//         printf("[ERROR] Failed to create monitoring interface\n");
//         return -1;
//     }

//     // 3. CAN 인터페이스 연결
//     if (can_connect(&g_monitoring_interface) != CAN_SUCCESS)
//     {
//         printf("[ERROR] Failed to connect monitoring interface\n");
//         return -1;
//     }

//     // 4. 데이터 프로세서 초기화
//     if (dp_init(&g_data_processor, true) != PROCESS_SUCCESS)
//     {
//         printf("[ERROR] Failed to initialize data processor\n");
//         return -1;
//     }

//     // 5. 콜백 함수 등록
//     dp_set_sensor_data_callback(&g_data_processor, on_sensor_data_received);
//     dp_set_alarm_callback(&g_data_processor, on_alarm_raised);
//     dp_set_offline_callback(&g_data_processor, on_sensor_offline);

//     // 6. 센서들 등록 (실제 환경에서는 설정 파일에서 읽어올 수 있음)
//     dp_register_sensor(&g_data_processor, 1, SENSOR_TYPE_TEMPERATURE, "Engine_Temp_1");
//     dp_register_sensor(&g_data_processor, 2, SENSOR_TYPE_TEMPERATURE, "Engine_Temp_2");
//     dp_register_sensor(&g_data_processor, 3, SENSOR_TYPE_PRESSURE, "Hydraulic_Press_1");
//     dp_register_sensor(&g_data_processor, 4, SENSOR_TYPE_PRESSURE, "Air_Press_1");
//     dp_register_sensor(&g_data_processor, 5, SENSOR_TYPE_VIBRATION, "Motor_Vib_1");
//     dp_register_sensor(&g_data_processor, 6, SENSOR_TYPE_VIBRATION, "Pump_Vib_1");

//     // 7. 시뮬레이터 초기화 (테스트용)
//     if (sim_init(&g_simulator) != CAN_SUCCESS)
//     {
//         printf("[ERROR] Failed to initialize simulator\n");
//         return -1;
//     }

//     // 8. 가상 센서들 추가
//     sim_add_temperature_sensor(&g_simulator, 1, 85.0f, 20.0f); // 엔진 온도 (65-105°C)
//     sim_add_temperature_sensor(&g_simulator, 2, 80.0f, 15.0f); // 엔진 온도 2
//     sim_add_pressure_sensor(&g_simulator, 3, 150.0f, 50.0f);   // 유압 (100-200 bar)
//     sim_add_pressure_sensor(&g_simulator, 4, 7.0f, 3.0f);      // 공기압 (4-10 bar)
//     sim_add_vibration_sensor(&g_simulator, 5, 2.0f, 4.0f);     // 모터 진동 (0-6 mm/s)
//     sim_add_vibration_sensor(&g_simulator, 6, 1.5f, 3.0f);     // 펌프 진동

//     printf("[MAIN] System initialization completed\n");
//     return 0;
// }

// // 메인 모니터링 루프
// void monitoring_loop()
// {
//     printf("[MAIN] Starting monitoring loop...\n");

//     // 시뮬레이터 시작
//     if (sim_start(&g_simulator) != CAN_SUCCESS)
//     {
//         printf("[ERROR] Failed to start simulator\n");
//         return;
//     }

//     time_t last_status_print = time(NULL);
//     time_t last_timeout_check = time(NULL);

//     while (g_running)
//     {
//         can_frame_t frame;

//         // CAN 메시지 수신 (논블로킹)
//         can_error_t result = can_receive(&g_monitoring_interface, &frame, 0);

//         if (result == CAN_SUCCESS)
//         {
//             // 수신된 메시지를 데이터 프로세서로 전달
//             process_result_t process_result = dp_process_can_message(&g_data_processor, &frame);

//             if (process_result != PROCESS_SUCCESS && process_result != PROCESS_ERROR_UNKNOWN_SENSOR)
//             {
//                 printf("[MAIN] Message processing failed: %d\n", process_result);
//             }
//         }
//         else if (result != CAN_ERROR_RECV_FAILED)
//         {
//             // 타임아웃이 아닌 실제 에러
//             printf("[MAIN] CAN receive error: %d\n", result);
//         }

//         // 주기적 작업들
//         time_t current_time = time(NULL);

//         // 5초마다 센서 타임아웃 체크
//         if (current_time - last_timeout_check >= 5)
//         {
//             dp_check_sensor_timeouts(&g_data_processor);
//             last_timeout_check = current_time;
//         }

//         // 30초마다 시스템 상태 출력
//         if (current_time - last_status_print >= 30)
//         {
//             dp_print_system_status(&g_data_processor);
//             last_status_print = current_time;
//         }

//         // CPU 부하 줄이기 위한 짧은 대기
//         SLEEP_MS(10000); // 10ms 대기
//     }
// }

// // 시스템 정리
// void cleanup_system()
// {
//     printf("[MAIN] Cleaning up system...\n");

//     // 1. 시뮬레이터 정지
//     sim_stop(&g_simulator);
//     sim_cleanup(&g_simulator);

//     // 2. CAN 인터페이스 연결 해제
//     can_disconnect(&g_monitoring_interface);

//     // 3. 데이터 프로세서 정리
//     dp_cleanup(&g_data_processor);

//     // 4. CAN 관리자 정리
//     can_cleanup_manager();

//     printf("[MAIN] System cleanup completed\n");
// }

// // 사용자 명령 처리 (별도 스레드나 이벤트 방식으로 구현 가능)
// void handle_user_commands()
// {
//     // 간단한 예제: 'q' 입력시 종료
//     // 실제로는 더 복잡한 명령 시스템 구현 가능

//     printf("\n=== Available Commands ===\n");
//     printf("s - Show system status\n");
//     printf("a - Show active alarms\n");
//     printf("h - Show sensor history for sensor ID\n");
//     printf("q - Quit\n");
//     printf("========================\n\n");

//     char cmd;
//     while (g_running && scanf(" %c", &cmd) == 1)
//     {
//         switch (cmd)
//         {
//         case 's':
//             dp_print_system_status(&g_data_processor);
//             break;

//         case 'a':
//         {
//             int alarm_count;
//             alarm_record_t *alarms = dp_get_active_alarms(&g_data_processor, &alarm_count);

//             printf("\n=== Active Alarms (%d) ===\n", alarm_count);
//             for (int i = 0; i < alarm_count && i < 10; i++)
//             {
//                 if (!alarms[i].is_acknowledged)
//                 {
//                     printf("[%d] Sensor %d: %s\n", i, alarms[i].sensor_id, alarms[i].description);
//                 }
//             }
//             printf("=======================\n\n");
//             break;
//         }

//         case 'h':
//         {
//             printf("Enter sensor ID (1-6): ");
//             int sensor_id;
//             if (scanf("%d", &sensor_id) == 1)
//             {
//                 dp_print_sensor_stats(&g_data_processor, sensor_id);
//             }
//             break;
//         }

//         case 'q':
//             g_running = false;
//             break;

//         default:
//             printf("Unknown command: %c\n", cmd);
//             break;
//         }
//     }
// }

// // 메인 함수
// int main(int argc, char *argv[])
// {
//     printf("=== Industrial CAN Monitoring System ===\n");
//     printf("Version 1.0\n\n");

//     // 신호 핸들러 등록
//     signal(SIGINT, signal_handler);
//     signal(SIGTERM, signal_handler);

//     // 시스템 초기화
//     if (initialize_system() != 0)
//     {
//         printf("[ERROR] System initialization failed\n");
//         return EXIT_FAILURE;
//     }

//     // 실제 시스템에서는 백그라운드 스레드로 실행
//     // 여기서는 간단히 포그라운드에서 실행

//     printf("[MAIN] System ready. Press 's' for status, 'q' to quit.\n");

//     // 메인 모니터링 루프를 별도 스레드에서 실행하고
//     // 메인 스레드에서는 사용자 입력 처리
//     // (실제 구현에서는 pthread 사용)

//     monitoring_loop();

//     // 정리 작업
//     cleanup_system();

//     printf("[MAIN] System shutdown complete\n");
//     return EXIT_SUCCESS;
// }

// /*
//  * 컴파일 방법:
//  * gcc -o can_monitor main.c \
//  * src/common/can_interface.c \
//  * src/central_controller/data_processor.c \
//  * src/simulator/can_simulator.c \
//  * -lpthread -lm
//  *
//  * 실행 방법:
//  * ./can_monitor
//  */