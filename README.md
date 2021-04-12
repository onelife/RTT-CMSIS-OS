# RTT-CMSIS-OS

RT-Thread implementation of ARM CMSIS-RTOS C API v2 


## Dependence

* [RT-Thread Library](https://github.com/onelife/Arduino_RT-Thread)
  - Mandatory flags (in "rtconfig.h")
    - `RT_USING_MUTEX`
    - `RT_USING_SEMAPHORE`
    - `RT_USING_EVENT`
  - Optional flags (in "rtconfig.h")
    - `RT_USING_MEMPOOL`: To enable `osMemoryPoolxxx` APIs
    - `RT_USING_MESSAGEQUEUE`: To use `osMessageQueuexxx` APIs


## Not Implemented APIs ##

The following functions do nothing.

* `osKernelSuspend()`
* `osKernelResume()`


## Thread Priority ##

* In CMSIS OS APIs, bigger priority number means higher priority
* In RT-Thread, bigger priority number means lower priority

In `osThreadSetPriority()` and `osThreadGetPriority()` functions, the following formulas are used for priority conversion.

* `rtt_priority = osPriorityISR - cmsis_priority`
* `cmsis_priority = osPriorityISR - rtt_priority`

The default values of `osPriorityISR` and `RT_THREAD_PRIORITY_MAX` are 56 and 32. So the available priority range for `osThreadSetPriority()` function is from `osPriorityNormal1` (25) to `osPriorityISR` (56).


## Message Queue ##

* The `msg_prio` (message priority) and `timeout` parameters of `osMessageQueuePut()` function are ignored. So there is no waiting in the function.
* The `*msg_prio` (message priority pointer) parameter of `osMessageQueueGet()` function is ignored.
