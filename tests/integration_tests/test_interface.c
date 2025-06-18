#include "../../src/common/can_interface.c"
#include "../../src/common/include/message_type.h"

// test setting
#define TEST_TIMEOUT_MS 5000
#define TEST_MESAGE_CNT 10
#define SENDER_THREAD_DELAY 100 // ms

// test result
typedef struct
{
    int total_tests;
    int passed_tests;
    int failed_tests;
} test_result_t;

static test_result_t test_result = {0, 0, 0};

#define INTEGRATION_TEST_START(name)                      \
    do                                                    \
    {                                                     \
        printf("\n=== Integration Test: %s ===\n", name); \
        test_result.total_tests++;                        \
    } while (0)

#define INTEGRATION_TEST_ASSERT(condition, message) \
    do                                              \
    {                                               \
        if (condition)                              \
        {                                           \
            printf("PASS: %s\n", message);          \
        }                                           \
        else                                        \
        {                                           \
            printf("FAIL: %s\n", message);          \
            test_result.failed_tests++;             \
            return;                                 \
        }                                           \
    } while (0)

#define INTEGRATION_TEST_END()                     \
    do                                             \
    {                                              \
        test_result.passed_tests++;                \
        printf("\nTest completed successfully\n"); \
    } while (0)

// 1. 기본 송수신 테스트
void test_basic_send_receive()
{
    INTEGRATION_TEST_START("Basic Send/Recive");

    can_init_manager(true);

    // 인터페이스 생성
    can_interface_t temp_sensor, controller;
    can_create_interface(&temp_sensor, "temp_sensor", 0x101);
    can_create_interface(&controller, "controller", 0x001);

    // 연결
    can_connect(&temp_sensor);
    can_connect(&controller);

    // 컨트롤러에 필터 설정 (온도 센서 메시지만 수신)
    can_set_filter(&controller, CAN_ID_TEMPERATURE_BASE, 0xFF0);

    // 온도 센서 데이터 전송
    can_frame_t tx_frame = {
        .id = 0x101,
        .dlc = 8,
        .data = {0x01, MSG_TYPE_SENSOR_DATA, 0x4C, 0x09, UNIT_CELSIUS, SENSOR_OK, 0x01, 0x00}};

    printf("Sending temperature data...\n");
    can_send(&temp_sensor, &tx_frame);

    // 컨트롤러에서 수신
    can_frame_t rx_frame;
    can_error_t result = can_receive(&controller, &rx_frame, 1000);

    if (result == CAN_SUCCESS)
    {
        printf("Received messsage successfully!\n");
        printf("Temperature: %.2f C\n", ((rx_frame.data[3] << 8) | rx_frame.data[2]) / 100.0);
    }
    else
    {
        printf("Failed to recieve message: %d", result);
    }

    can_disconnect(&temp_sensor);
    can_disconnect(&controller);
    can_cleanup_manager();

    printf("Test completed\n");
}

int main()
{
    test_basic_send_receive();
    return 0;
}