#include <stdio.h>

#include "board_init.h"
#include "stm32l475e_iot01.h"
#include "cmsis_utils.h"
#include "stm_networking.h"
#include "secrets.h"

#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/int32.h>
#include <rmw_microros/rmw_microros.h>

#include <microros_transports.h>

#define AZURE_THREAD_STACK_SIZE 4096*2
#define AZURE_THREAD_PRIORITY   4

TX_THREAD azure_thread;
ULONG azure_thread_stack[AZURE_THREAD_STACK_SIZE / sizeof(ULONG)];

rcl_publisher_t publisher;

void subscription_callback(const void * msgin)
{
  const std_msgs__msg__Int32 * msg = (const std_msgs__msg__Int32 *)msgin;

  if (msg->data == 0)
  {
      BSP_LED_Off(LED2);
  } else {
      BSP_LED_On(LED2);
  }
}

void timer_callback(rcl_timer_t * timer, int64_t last_call_time)
{
    (void) last_call_time;
    (void) timer;

    static std_msgs__msg__Int32 msg = {0};

    if(RMW_RET_OK == rcl_publish(&publisher, &msg, NULL)){
        printf("Sent: %ld\n", msg.data);
        msg.data++;
    } else {
        printf("Failed to send\n");
    }
}

void microros_thread(ULONG parameter)
{
    // Initialize the network
    if (stm32_network_init(WIFI_SSID, WIFI_PSK, WPA2_PSK_AES) != NX_SUCCESS)
    {
        printf("Failed to initialize the network\r\n");
        return;
    }

    rcl_allocator_t allocator = rcl_get_default_allocator();

    //create init_options
    rclc_support_t support;
    rclc_support_init(&support, 0, NULL, &allocator);

    // create nodes
    rcl_node_t node;
    rclc_node_init_default(&node, "threadx_node", "", &support);

    // create publisher
    rclc_publisher_init_default(
        &publisher,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
        "threadx_publisher");

    // create subscriber
    rcl_subscription_t subscriber;
    rclc_subscription_init_default(
        &subscriber,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
        "threadx_subscriber");

    // create timer,
    rcl_timer_t timer;
    rclc_timer_init_default(
        &timer,
        &support,
        RCL_MS_TO_NS(1000),
        timer_callback);

    // create executor
    rclc_executor_t executor = rclc_executor_get_zero_initialized_executor();
    rclc_executor_init(&executor, &support.context, 2, &allocator);

    std_msgs__msg__Int32 msg = {0};
    rclc_executor_add_subscription(&executor, &subscriber, &msg, subscription_callback, ON_NEW_DATA);
    rclc_executor_add_timer(&executor, &timer);

    while(1){
        rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
        tx_thread_sleep((ULONG) 0.1*TX_TIMER_TICKS_PER_SECOND);
    }

    (void)! rcl_publisher_fini(&publisher, &node);
    (void)! rcl_node_fini(&node);
}

void tx_application_define(void* first_unused_memory)
{
    systick_interval_set(TX_TIMER_TICKS_PER_SECOND);

    // Configure micro-ROS Agent IP and port
    static custom_transport_args args = {
      .agent_ip_address = IP_ADDRESS(192,168,1,57),
      .agent_port = 8888
    };

    printf("\r\nStarting micro-ROS thread\r\n\r\n");

    rmw_uros_set_custom_transport(
      false,
      (void *) &args,
      azure_transport_open,
      azure_transport_close,
      azure_transport_write,
      azure_transport_read);

    // Create Azure thread
    UINT status = tx_thread_create(&azure_thread,
        "micro-ROS thread",
        microros_thread,
        0,
        azure_thread_stack,
        AZURE_THREAD_STACK_SIZE,
        AZURE_THREAD_PRIORITY,
        AZURE_THREAD_PRIORITY,
        TX_NO_TIME_SLICE,
        TX_AUTO_START);

    if (status != TX_SUCCESS)
    {
        printf("Thread creation failed\r\n");
        tx_thread_sleep(5);
    }
}

int main(void)
{
    // Initialize the board
    board_init();

    // Enter the ThreadX kernel
    tx_kernel_enter();
    return 0;
}
