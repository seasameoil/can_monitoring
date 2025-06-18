#include "include/can_interface.h"
#include <time.h>

// 전역 CAN 관리자 인스턴스
can_manager_t g_can_manager = {0};

can_error_t can_init_manager(bool debug_mode)
{
    memset(&g_can_manager, 0, sizeof(can_manager_t));
    g_can_manager.debug_mode = debug_mode;
    g_can_manager.interface_cnt = 0;
    g_can_manager.global_queue.head = 0;
    g_can_manager.global_queue.tail = 0;
    g_can_manager.global_queue.cnt = 0;

    if (debug_mode)
    {
        printf("[CAN] Manager initialized in debug mode\n");
    }

    return CAN_SUCCESS;
}

// CAN 인터페이스 생성
can_error_t can_create_interface(can_interface_t *can_interface, const char *name, uint32_t node_id)
{
    if (!can_interface || !name)
    {
        return CAN_ERROR_INVALID_PARAM;
    }

    if (g_can_manager.interface_cnt >= CAN_MAX_INTERFACES)
    {
        printf("[CAN] MAximum interfaces reached\n");
        return CAN_ERROR_INIT_FAILED;
    }

    memset(can_interface, 0, sizeof(can_interface_t));
    strncpy(can_interface->interface_name, name, sizeof(can_interface->interface_name));
    can_interface->node_id = node_id;
    can_interface->is_connected = false;
    can_interface->filter_enabled = false;

    // 관리자에 등록
    g_can_manager.interfaces[g_can_manager.interface_cnt] = *can_interface;
    g_can_manager.interface_cnt++;

    if (g_can_manager.debug_mode)
    {
        printf("[CAN] Interface '%s' is created (Node ID: 0x%03X)\n", name, node_id);
    }

    return CAN_SUCCESS;
}

can_error_t can_connect(can_interface_t *can_interface)
{
    if (!can_interface)
    {
        return CAN_ERROR_INVALID_PARAM;
    }

    can_interface->is_connected = true;
    can_interface->tx_cnt = 0;
    can_interface->rx_cnt = 0;
    can_interface->err_cnt = 0;

    if (g_can_manager.debug_mode)
    {
        printf("[CAN] Interface '%s' is connected\n", can_interface->interface_name);
    }

    return CAN_SUCCESS;
}

can_error_t can_disconnect(can_interface_t *can_interface)
{
    if (!can_interface)
    {
        return CAN_ERROR_INVALID_PARAM;
    }

    can_interface->is_connected = false;

    if (g_can_manager.debug_mode)
    {
        printf("[CAN] Interface '%s' is disconnected\n", can_interface->interface_name);
    }

    return CAN_SUCCESS;
}

can_error_t can_send(can_interface_t *can_interface, const can_frame_t *frame)
{
    if (!can_interface || !frame)
    {
        return CAN_ERROR_INVALID_PARAM;
    }

    if (!can_interface->is_connected)
    {
        return CAN_ERROR_NOT_CONNECTED;
    }

    if (frame->dlc > CAN_MAX_DATA_LENGTH)
    {
        return CAN_ERROR_INVALID_PARAM;
    }

    // 전역 큐에 메시지 추가
    can_message_queue_t *queue = &g_can_manager.global_queue;

    if (queue->cnt >= CAN_MESSAGE_QUEUE_SIZE)
    {
        can_interface->err_cnt++;
        return CAN_ERROR_QUEUE_FULL;
    }

    // 메시지 복사 및 타임스탬프 추가
    can_frame_t *msg = &queue->messages[queue->head];
    *msg = *frame;
    // GetCurrentTime(&msg->timestamp);

    queue->head = (queue->head + 1) % CAN_MESSAGE_QUEUE_SIZE;
    queue->cnt++;

    can_interface->tx_cnt++;

    if (g_can_manager.debug_mode)
    {
        printf("[CAN TX] %s: ID=0x%03X, DLC=%d, DATA= ", can_interface->interface_name, frame->id, frame->dlc);
        for (int i = 0; i < frame->dlc; i++)
        {
            printf("%02X ", frame->data[i]);
        }
        printf("\n");
    }

    return CAN_SUCCESS;
}

can_error_t can_receive(can_interface_t *can_interface, can_frame_t *frame, int timeout_ms)
{
    if (!can_interface || !frame)
    {
        return CAN_ERROR_INVALID_PARAM;
    }

    if (!can_interface->is_connected)
    {
        return CAN_ERROR_NOT_CONNECTED;
    }

    can_message_queue_t *queue = &g_can_manager.global_queue;

    // 타임아웃 처리를 위한 시작 시간
    struct timespec start_time, current_time;
    // GetCurrentTime(&start_time);

    while (1)
    {
        // 큐에서 메시지 검색
        for (int i = 0; i < queue->cnt; i++)
        {
            int idx = (queue->tail + i) % CAN_MESSAGE_QUEUE_SIZE;
            can_frame_t *msg = &queue->messages[idx];

            // 필터 검사
            bool pass_filter = false;
            if (can_interface->filter_enabled)
            {
                pass_filter = ((msg->id & can_interface->filter_mask) == (can_interface->filter_id & can_interface->filter_mask));
            }

            // 자신이 보낸 메시지는 제외 (실제 CAN에서는 가능)
            bool is_own_message = false;
            // 간단한 방법: node id 기반으로 판단 (실제로는 더 복잡한 로직 필요)

            if (pass_filter && !is_own_message)
            {
                *frame = *msg;

                // 큐에서 메시지 제거 (해당 위치부터 앞으로 이동)
                for (int j = i; j < queue->cnt - 1; j++)
                {
                    int curr_idx = (queue->tail + j) % CAN_MESSAGE_QUEUE_SIZE;
                    int next_idx = (queue->tail + j + 1) % CAN_MESSAGE_QUEUE_SIZE;
                    queue->messages[curr_idx] = queue->messages[next_idx];
                }
                queue->cnt--;

                can_interface->rx_cnt++;

                if (g_can_manager.debug_mode)
                {
                    printf("[CAN RX] %s: ID=0x%03X, DLC=%d, DATA= ", can_interface->interface_name, frame->id, frame->dlc);
                    for (int k = 0; k < frame->dlc; k++)
                    {
                        printf("%02X ", frame->data[k]);
                    }
                    printf("\n");
                }
                return CAN_SUCCESS;
            }
        }

        if (timeout_ms > 0)
        {
            // GetCurrentTime(&current_time);

            long elasped_ms = (current_time.tv_sec - start_time.tv_sec) * 1000 + (current_time.tv_nsec - start_time.tv_nsec) / 1000000;

            if (elasped_ms >= timeout_ms)
            {
                return CAN_ERROR_RECV_FAILED;
            }
        }
        else if (timeout_ms == 0)
        {
            return CAN_ERROR_RECV_FAILED;
        }

        // _sleep(100);
    }
}

can_error_t can_set_filter(can_interface_t *can_interface, uint32_t id, uint32_t mask)
{
    if (!can_interface)
    {
        return CAN_ERROR_INVALID_PARAM;
    }

    can_interface->filter_id = id;
    can_interface->filter_mask = mask;
    can_interface->filter_enabled = true;

    if (g_can_manager.debug_mode)
    {
        printf("[CAN] Filter Set for '%s': ID=0x%03X, Mask=0x%03X\n", can_interface->interface_name, id, mask);
    }

    return CAN_SUCCESS;
}

void can_cleanup_manager(void)
{
    for (int i = 0; i < g_can_manager.interface_cnt; i++)
    {
        can_disconnect(&g_can_manager.interfaces[i]);
    }
    memset(&g_can_manager, 0, sizeof(g_can_manager));

    printf("[CAN] Manager cleanded up\n");
}