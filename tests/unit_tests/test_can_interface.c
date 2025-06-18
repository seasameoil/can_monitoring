#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <winsock2.h>
#include <Windows.h>
#include "../../src/common/can_interface.c"

// test result counter
static int test_passed = 0;
static int test_failed = 0;
static int test_total = 0;

// test macro
#define TEST_START(name)                        \
    do                                          \
    {                                           \
        printf("\n=== Testing %s ===\n", name); \
        test_total++;                           \
    } while (0)

#define TEST_ASSERT(condition, message)    \
    do                                     \
    {                                      \
        if (condition)                     \
        {                                  \
            printf("PASS: %s\n", message); \
        }                                  \
        else                               \
        {                                  \
            printf("FAIL: %s\n", message); \
            test_failed++;                 \
            return;                        \
        }                                  \
    } while (0)

#define TEST_END()                                 \
    do                                             \
    {                                              \
        test_passed++;                             \
        printf("\nTest completed successfully\n"); \
    } while (0)

// 테스트용 헬퍼 함수
static void create_test_frame(can_frame_t *frame, uint32_t id, uint8_t dlc, const uint8_t *data)
{
    memset(frame, 0, sizeof(can_frame_t));
    frame->id = id;
    frame->dlc = dlc;
    frame->is_extended = false;
    frame->is_rtr = false;

    if (data && dlc > 0)
    {
        memcpy(frame->data, data, dlc);
    }
}

// 1. CAN 인터페이스 초기화 테스트
void test_can_init()
{
    TEST_START("CAN Interface Initialization");

    can_interface_t can_interface;
    can_error_t result;

    // 정상 초기화 테스트
    result = can_init(&can_interface, "test_interface");
    TEST_ASSERT(result == CAN_SUCCESS, "Normal Initialization");
    TEST_ASSERT(can_interface.tx_cnt == 0, "TX counter is initialized to 0");

    TEST_END();
}

// 2. CAN 연결 테스트
void test_can_conncet()
{
    TEST_START("CAN Connection");

    can_interface_t can_interface;
    can_error_t result;

    // init
    result = can_init(&can_interface, "test_connect");
    TEST_ASSERT(result == CAN_SUCCESS, "Interface Initialization for connection test");

    // 정상 연결 테스트
    result = can_connect(&can_interface);
    TEST_ASSERT(result == CAN_SUCCESS, "Normal Connection");
    TEST_ASSERT(can_interface.is_connected == true, "Connection Status updated");
    TEST_ASSERT(can_interface.socket_fd != INVALID_SOCKET, "Valid Socket created");

    // 이미 연결된 상태에서 재연결 테스트
    result = can_connect(&can_interface);
    TEST_ASSERT(result == CAN_SUCCESS, "Reconnection when already connected");

    // 연결 해제
    can_disconnect(&can_interface);

    // NULL 포인터 테스트
    result = can_connect(NULL);
    TEST_ASSERT(result == CAN_ERROR_INVALID_PARAM, "Null interface param");

    TEST_END();
}

// 3. CAN 연결 해제 테스트
void test_can_disconnect()
{
}

// 4. CAN 메시지 생성 및 검증 테스트
void test_can_frame_creation()
{
    TEST_START("CAN Frame Creation and Validation");

    can_frame_t frame;
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x067, 0x07, 0x08};

    // 정상 프레임 생성
    create_test_frame(&frame, 0x123, 8, test_data);
    TEST_ASSERT(frame.id == 0x123, "Frame ID set correctly");
    TEST_ASSERT(memcmp(frame.data, test_data, 8) == 0, "Frame data set correctly");
    TEST_ASSERT(frame.is_extended == false, "Standard frame type");

    TEST_END();
}

// main test function
int main()
{
    printf("==============================================\n");
    printf("         CAN Interface Unit Test\n");
    printf("==============================================\n");

    test_can_init();
    test_can_conncet();
    test_can_frame_creation();
}