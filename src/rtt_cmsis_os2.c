/***************************************************************************//**
 * @file    rtt_cmsis_os2.c
 * @brief   RT-Thread CMSIS RTOS2 library
 * @author  onelife <onelife.real[at]gmail.com>
 ******************************************************************************/
/* Includes ------------------------------------------------------------------*/
#include "os_tick.h"
#include "cmsis_os.h"

#include "include/rtthread.h"
#include "include/rthw.h"

/***************************************************************************//**
 * @addtogroup CMSIS_RTOS2
 * @{
 ******************************************************************************/

#ifndef RT_USING_MUTEX
# error "RT_USING_MUTEX is not enabled"
#endif
#ifndef RT_USING_SEMAPHORE
# error "RT_USING_SEMAPHORE is not enabled"
#endif
#ifndef RT_USING_EVENT
# error "RT_USING_EVENT is not enabled"
#endif

/* Private typedef -----------------------------------------------------------*/
struct thread_extra {
    struct rt_semaphore join;
    struct rt_event flags;
};
typedef struct thread_extra *thread_extra_t;

struct semaphore_extra {
    struct rt_semaphore sem;
    uint32_t max_count;
};
typedef struct semaphore_extra *semaphore_extra_t;

/* Private define ------------------------------------------------------------*/
#define THREAD_DEFAULT_STACK_SIZE   (512)
#define THREAD_DEFAULT_PRIORITY     (RT_THREAD_PRIORITY_MAX >> 1)
#define THREAD_DEFAULT_TICK         (100)

/* Private variables ---------------------------------------------------------*/
static osKernelState_t _state = osKernelInactive;

/* Private function prototypes -----------------------------------------------*/
extern void rt_thread_exit(void);

/* Private functions ---------------------------------------------------------*/

/* Public functions ----------------------------------------------------------*/
//  ==== Kernel Management Functions ====
 
/// Initialize the RTOS Kernel.
/// \return status code that indicates the execution status of the function.
osStatus_t osKernelInitialize(void) {
    _state = osKernelReady;
    return osOK;
}
 
///  Get RTOS Kernel Information.
/// \param[out]    version       pointer to buffer for retrieving version information.
/// \param[out]    id_buf        pointer to buffer for retrieving kernel identification string.
/// \param[in]     id_size       size of buffer for kernel identification string.
/// \return status code that indicates the execution status of the function.
osStatus_t osKernelGetInfo(osVersion_t *version, char *id_buf, uint32_t id_size) {
    if (!version || !version->api || !version->kernel || !id_buf)
        return osError;
    version->api = osCMSIS;
    version->kernel = osCMSIS_KERNEL;
    rt_strncpy(id_buf, osKernelSystemId, id_size);
    return osOK;
}
 
/// Get the current RTOS Kernel state.
/// \return current RTOS Kernel state.
osKernelState_t osKernelGetState(void) {
    return _state;
}
 
/// Start the RTOS Kernel scheduler.
/// \return status code that indicates the execution status of the function.
osStatus_t osKernelStart(void) {
    // RTT kernel is started by calling "RT_T.begin()"
    if (osKernelReady != _state)
        return osError;
    // Never return if no error
    _state = osKernelRunning;
    while (1);
}
 
/// Lock the RTOS Kernel scheduler.
/// \return previous lock state (1 - locked, 0 - not locked, error code if negative).
int32_t osKernelLock(void) {
    rt_enter_critical();
    if (rt_critical_level() > 0U) {
        _state = osKernelLocked;
        return 1;
    }
    return 0;
}
 
/// Unlock the RTOS Kernel scheduler.
/// \return previous lock state (1 - locked, 0 - not locked, error code if negative).
int32_t osKernelUnlock(void) {
    rt_exit_critical();
    if (rt_critical_level() > 0U)
        return 1;
    _state = osKernelRunning;
    return 0;
}
 
/// Restore the RTOS Kernel scheduler lock state.
/// \param[in]     lock          lock state obtained by \ref osKernelLock or \ref osKernelUnlock.
/// \return new lock state (1 - locked, 0 - not locked, error code if negative).
int32_t osKernelRestoreLock(int32_t lock) {
    if (0 == lock)
        return osKernelLock();
    else if (1 == lock)
        return osKernelUnlock();
    else
        return osError;
}
 
/// Suspend the RTOS Kernel scheduler.
/// \return time in ticks, for how long the system can sleep or power-down.
uint32_t osKernelSuspend(void) {
    // #error "Not implemented function."
    return 0;
}
 
/// Resume the RTOS Kernel scheduler.
/// \param[in]     sleep_ticks   time in ticks for how long the system was in sleep or power-down mode.
void osKernelResume(uint32_t sleep_ticks) {
    (void)sleep_ticks;
    // #error "Not implemented function."
}
 
/// Get the RTOS kernel tick count.
/// \return RTOS kernel current tick count.
uint32_t osKernelGetTickCount(void) {
    return rt_tick_get();
}
 
/// Get the RTOS kernel tick frequency.
/// \return frequency of the kernel tick in hertz, i.e. kernel ticks per second.
uint32_t osKernelGetTickFreq(void) {
    return RT_TICK_PER_SECOND;
}
 
/// Get the RTOS kernel system timer count.
/// \return RTOS kernel current system timer count as 32-bit value.
uint32_t osKernelGetSysTimerCount(void) {
    uint32_t tick;
    uint32_t count;

    tick  = rt_tick_get();
    count = OS_Tick_GetCount();
    if (OS_Tick_GetOverflow() != 0U) {
        count = OS_Tick_GetCount();
        tick++;
    }
    count += tick * OS_Tick_GetInterval();
    return count;
}
 
/// Get the RTOS kernel system timer frequency.
/// \return frequency of the system timer in hertz, i.e. timer ticks per second.
uint32_t osKernelGetSysTimerFreq (void) {
  return OS_Tick_GetClock();
}
 
 
//  ==== Thread Management Functions ====
 
/// Create a thread and add it to Active Threads.
/// \param[in]     func          thread function.
/// \param[in]     argument      pointer that is passed to the thread function as start argument.
/// \param[in]     attr          thread attributes; NULL: default values.
/// \return thread ID for reference by other functions or NULL in case of error.
osThreadId_t osThreadNew(osThreadFunc_t func, void *argument, const osThreadAttr_t *attr) {
    osThreadAttr_t dft_attr = {
        "thread", 0,
        NULL, 0,
        NULL, THREAD_DEFAULT_STACK_SIZE,
        THREAD_DEFAULT_PRIORITY, 0,
        0
    };
    thread_extra_t extra = NULL;
    rt_thread_t thread = NULL;

    if (!attr)
        attr = &dft_attr;

    do {
        if (NULL == (extra = (thread_extra_t)rt_malloc(sizeof(struct thread_extra))))
            break;
        // rt_memset(extra, 0x00, sizeof(struct thread_extra));
        if (RT_EOK != rt_sem_init(&extra->join, attr->name, 0, RT_IPC_FLAG_FIFO))
            break;
        if (RT_EOK != rt_event_init(&extra->flags, attr->name, RT_IPC_FLAG_FIFO))
            break;
        if (NULL == (thread = rt_thread_create(
            attr->name,
            func, argument,
            attr->stack_size,
            (uint8_t)(osPriorityISR - attr->priority),
            THREAD_DEFAULT_TICK)))
            break;
        thread->user_data = (uint32_t)extra;
        if (RT_EOK != rt_thread_startup(thread))
            break;
        return (osThreadId_t)thread;
    } while (0);

    if (extra)
        rt_free(extra);
    if (thread)
        (void)rt_thread_delete(thread);
    return NULL;
}
 
/// Get name of a thread.
/// \param[in]     thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
/// \return name as null-terminated string.
const char *osThreadGetName(osThreadId_t thread_id) {
    rt_thread_t thread = (rt_thread_t)thread_id;
    if (!thread)
        return NULL;
    return thread->name;
}
 
/// Return the thread ID of the current running thread.
/// \return thread ID for reference by other functions or NULL in case of error.
osThreadId_t osThreadGetId(void) {
    return (osThreadId_t)rt_thread_self();
}
 
/// Get current thread state of a thread.
/// \param[in]     thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
/// \return current thread state of the specified thread.
osThreadState_t osThreadGetState(osThreadId_t thread_id) {
    rt_thread_t thread = (rt_thread_t)thread_id;
    if (!thread)
        return osThreadError;
    if (RT_THREAD_INIT == (thread->stat & RT_THREAD_STAT_MASK))
        return osThreadInactive;
    else if (RT_THREAD_READY == (thread->stat & RT_THREAD_STAT_MASK))
        return osThreadReady;
    else if (RT_THREAD_SUSPEND == (thread->stat & RT_THREAD_STAT_MASK))
        return osThreadBlocked;
    else if (RT_THREAD_RUNNING == (thread->stat & RT_THREAD_STAT_MASK))
        return osThreadRunning;
    else if (RT_THREAD_CLOSE == (thread->stat & RT_THREAD_STAT_MASK))
        return osThreadTerminated;
    else
        return osThreadError;
}
 
/// Get stack size of a thread.
/// \param[in]     thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
/// \return stack size in bytes.
uint32_t osThreadGetStackSize(osThreadId_t thread_id) {
    rt_thread_t thread = (rt_thread_t)thread_id;
    if (!thread)
        return 0;
    return thread->stack_size;
}
 
/// Get available stack space of a thread based on stack watermark recording during execution.
/// \param[in]     thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
/// \return remaining stack space in bytes.
uint32_t osThreadGetStackSpace(osThreadId_t thread_id) {
    uint8_t *ptr;
    rt_thread_t thread = (rt_thread_t)thread_id;
    if (!thread)
        return 0;

#if defined(ARCH_CPU_STACK_GROWS_UPWARD)
    ptr = (uint8_t *)thread->stack_addr + thread->stack_size - 1;
    while (*ptr == '#')
        ptr--;
    return (thread->stack_size - ((uint32_t)ptr - (uint32_t)thread->stack_addr));
#else
    ptr = (uint8_t *)thread->stack_addr;
    while (*ptr == '#')
        ptr++;
    return ((uint32_t)ptr - (uint32_t)thread->stack_addr);
#endif
}
 
/// Change priority of a thread.
/// \param[in]     thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
/// \param[in]     priority      new priority value for the thread function.
/// \return status code that indicates the execution status of the function.
osStatus_t osThreadSetPriority(osThreadId_t thread_id, osPriority_t priority) {
    uint8_t prio;
    rt_thread_t thread = (rt_thread_t)thread_id;
    if (!thread)
        return osErrorParameter;
    prio = (uint8_t)(osPriorityISR - priority);
    if (prio >= RT_THREAD_PRIORITY_MAX)
        return osErrorParameter;
    if (RT_EOK != rt_thread_control(thread, RT_THREAD_CTRL_CHANGE_PRIORITY, &prio))
        return osErrorResource;
    return osOK;
}
 
/// Get current priority of a thread.
/// \param[in]     thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
/// \return current priority value of the specified thread.
osPriority_t osThreadGetPriority(osThreadId_t thread_id) {
    rt_thread_t thread = (rt_thread_t)thread_id;
    if (!thread)
        return osPriorityError;
   return (osPriorityISR - (osPriority_t)thread->current_priority);
}
 
/// Pass control to next thread that is in state \b READY.
/// \return status code that indicates the execution status of the function.
osStatus_t osThreadYield(void) {
    if (RT_EOK != rt_thread_yield())
        return osError;
    return osOK;
}
 
/// Suspend execution of a thread.
/// \param[in]     thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
/// \return status code that indicates the execution status of the function.
osStatus_t osThreadSuspend(osThreadId_t thread_id) {
    rt_thread_t thread = (rt_thread_t)thread_id;
    if (!thread)
        return osErrorParameter;
    if (RT_EOK != rt_thread_suspend(thread))
        return osErrorResource;
    return osOK;
}
 
/// Resume execution of a thread.
/// \param[in]     thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
/// \return status code that indicates the execution status of the function.
osStatus_t osThreadResume(osThreadId_t thread_id) {
    rt_thread_t thread = (rt_thread_t)thread_id;
    if (!thread)
        return osErrorParameter;
    if (RT_EOK != rt_thread_resume(thread))
        return osErrorResource;
    return osOK;
}
 
/// Detach a thread (thread storage can be reclaimed when thread terminates).
/// \param[in]     thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
/// \return status code that indicates the execution status of the function.
osStatus_t osThreadDetach(osThreadId_t thread_id) {
    thread_extra_t extra;
    rt_thread_t thread = (rt_thread_t)thread_id;
    if (!thread)
        return osErrorParameter;
    extra = (thread_extra_t)thread->user_data;
    (void)rt_sem_release(&extra->join);
    (void)rt_thread_sleep(RT_TICK_PER_SECOND / 10);
    rt_free(extra);
    if (RT_EOK != rt_thread_delete(thread))
        return osErrorResource;
    return osOK;
}
 
/// Wait for specified thread to terminate.
/// \param[in]     thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
/// \return status code that indicates the execution status of the function.
osStatus_t osThreadJoin(osThreadId_t thread_id) {
    thread_extra_t extra;
    rt_thread_t thread = (rt_thread_t)thread_id;
    if (!thread)
        return osErrorParameter;
    if (RT_THREAD_INIT == (thread->stat & RT_THREAD_STAT_MASK))
        return osErrorResource;
    else if (RT_THREAD_CLOSE == (thread->stat & RT_THREAD_STAT_MASK))
        return osOK;
    extra = (thread_extra_t)thread->user_data;
    (void)rt_sem_take(&extra->join, RT_WAITING_FOREVER);
    return osOK;
}
 
/// Terminate execution of current running thread.
__NO_RETURN void osThreadExit(void) {
    rt_thread_exit();
    while (1);
}
 
/// Terminate execution of a thread.
/// \param[in]     thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
/// \return status code that indicates the execution status of the function.
osStatus_t osThreadTerminate(osThreadId_t thread_id) {
    return osThreadDetach(thread_id);
}
 
/// Get number of active threads.
/// \return number of active threads.
uint32_t osThreadGetCount(void) {
    uint32_t cnt;
    struct rt_object_information *info;
    rt_list_t *node;
    rt_thread_t thread;

    if (rt_thread_self())
        rt_enter_critical();

    cnt = 0;
    info = rt_object_get_information(RT_Object_Class_Thread);
    for (node = info->object_list.next;
         node != &(info->object_list);
         node = node->next) {
        thread = rt_list_entry(node, struct rt_thread, list);
        if ((RT_THREAD_INIT != (thread->stat & RT_THREAD_STAT_MASK)) && \
            (RT_THREAD_CLOSE != (thread->stat & RT_THREAD_STAT_MASK)))
            cnt++;
    }

    if (rt_thread_self())
        rt_exit_critical();

    return cnt;
}
 
/// Enumerate active threads.
/// \param[out]    thread_array  pointer to array for retrieving thread IDs.
/// \param[in]     array_items   maximum number of items in array for retrieving thread IDs.
/// \return number of enumerated threads.
uint32_t osThreadEnumerate(osThreadId_t *thread_array, uint32_t array_items) {
    uint32_t cnt;
    struct rt_object_information *info;
    rt_list_t *node;
    rt_thread_t thread;

    if (rt_thread_self())
        rt_enter_critical();

    cnt = 0;
    info = rt_object_get_information(RT_Object_Class_Thread);
    for (node = info->object_list.next;
         (node != &(info->object_list)) && (cnt < array_items);
         node = node->next) {
        thread = rt_list_entry(node, struct rt_thread, list);
        if ((RT_THREAD_INIT != (thread->stat & RT_THREAD_STAT_MASK)) && \
            (RT_THREAD_CLOSE != (thread->stat & RT_THREAD_STAT_MASK)))
            thread_array[cnt++] = (osThreadId_t)thread;
    }

    if (rt_thread_self())
        rt_exit_critical();

    return cnt;
}
 
 
//  ==== Thread Flags Functions ====
 
/// Set the specified Thread Flags of a thread.
/// \param[in]     thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
/// \param[in]     flags         specifies the flags of the thread that shall be set.
/// \return thread flags after setting or error code if highest bit set.
uint32_t osThreadFlagsSet(osThreadId_t thread_id, uint32_t flags) {
    thread_extra_t extra;
    rt_thread_t thread = (rt_thread_t)thread_id;
    if (!thread)
        return osFlagsErrorParameter;
    extra = (thread_extra_t)thread->user_data;
    if (RT_EOK != rt_event_send(&extra->flags, flags))
        return osFlagsErrorResource;
    return extra->flags.set;
}
 
/// Clear the specified Thread Flags of current running thread.
/// \param[in]     flags         specifies the flags of the thread that shall be cleared.
/// \return thread flags before clearing or error code if highest bit set.
uint32_t osThreadFlagsClear(uint32_t flags) {
    uint32_t ret;
    register rt_ubase_t level;
    thread_extra_t extra;
    rt_thread_t thread = rt_thread_self();
    if (!thread)
        return osFlagsErrorUnknown;
    extra = (thread_extra_t)thread->user_data;
    ret = extra->flags.set;
    /* clear event */
    level = rt_hw_interrupt_disable();
    extra->flags.set &= ~flags;
    rt_hw_interrupt_enable(level);
    return ret;
}
 
/// Get the current Thread Flags of current running thread.
/// \return current thread flags.
uint32_t osThreadFlagsGet(void) {
    thread_extra_t extra;
    rt_thread_t thread = rt_thread_self();
    if (!thread)
        return 0;
    extra = (thread_extra_t)thread->user_data;
    return extra->flags.set;
}
 
/// Wait for one or more Thread Flags of the current running thread to become signaled.
/// \param[in]     flags         specifies the flags to wait for.
/// \param[in]     options       specifies flags options (osFlagsXxxx).
/// \param[in]     timeout       \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
/// \return thread flags before clearing or error code if highest bit set.
uint32_t osThreadFlagsWait(uint32_t flags, uint32_t options, uint32_t timeout) {
    rt_err_t ret;
    rt_uint32_t recv;
    rt_uint8_t opts = 0;
    thread_extra_t extra;
    rt_thread_t thread = rt_thread_self();
    if (!thread)
        return osFlagsErrorUnknown;
    extra = (thread_extra_t)thread->user_data;
    if (osFlagsWaitAll & options)
        opts |= RT_EVENT_FLAG_AND;
    else
        opts |= RT_EVENT_FLAG_OR;
    if (osFlagsNoClear & options)
        opts |= RT_EVENT_FLAG_CLEAR;
    ret = rt_event_recv(&extra->flags, flags, opts, timeout, &recv);
    if (-RT_ETIMEOUT == ret)
        return osFlagsErrorTimeout;
    else if (RT_EOK != ret)
        return osFlagsErrorResource;
    else
        return recv;
}

 
//  ==== Generic Wait Functions ====
 
/// Wait for Timeout (Time Delay).
/// \param[in]     ticks         \ref CMSIS_RTOS_TimeOutValue "time ticks" value
/// \return status code that indicates the execution status of the function.
osStatus_t osDelay(uint32_t ticks) {
    (void)rt_thread_delay(ticks);
    return osOK;
}
 
/// Wait until specified time.
/// \param[in]     ticks         absolute time in ticks
/// \return status code that indicates the execution status of the function.
osStatus_t osDelayUntil(uint32_t ticks) {
    uint32_t current = rt_tick_get();
    if (ticks > current)
        (void)rt_thread_delay(ticks - current);
    return osOK;
}
 
 
//  ==== Timer Management Functions ====
 
/// Create and Initialize a timer.
/// \param[in]     func          function pointer to callback function.
/// \param[in]     type          \ref osTimerOnce for one-shot or \ref osTimerPeriodic for periodic behavior.
/// \param[in]     argument      argument to the timer callback function.
/// \param[in]     attr          timer attributes; NULL: default values.
/// \return timer ID for reference by other functions or NULL in case of error.
osTimerId_t osTimerNew(osTimerFunc_t func, osTimerType_t type, void *argument, const osTimerAttr_t *attr) {
    osTimerAttr_t dft_attr = {
        "timer", 0,
        NULL, 0,
    };
    rt_uint8_t opts;

    if (osTimerOnce == type)
        opts = RT_TIMER_FLAG_ONE_SHOT;
    else
        opts = RT_TIMER_FLAG_PERIODIC;
    if (!attr)
        attr = &dft_attr;
    return (osTimerId_t)rt_timer_create(
        attr->name,
        func, argument,
        1, opts);
}
 
/// Get name of a timer.
/// \param[in]     timer_id      timer ID obtained by \ref osTimerNew.
/// \return name as null-terminated string.
const char *osTimerGetName(osTimerId_t timer_id) {
    rt_timer_t timer = (rt_timer_t)timer_id;
    if (!timer)
        return NULL;
    return timer->parent.name;
}
 
/// Start or restart a timer.
/// \param[in]     timer_id      timer ID obtained by \ref osTimerNew.
/// \param[in]     ticks         \ref CMSIS_RTOS_TimeOutValue "time ticks" value of the timer.
/// \return status code that indicates the execution status of the function.
osStatus_t osTimerStart(osTimerId_t timer_id, uint32_t ticks) {
    rt_timer_t timer = (rt_timer_t)timer_id;
    if (!timer)
        return osErrorParameter;
    timer->init_tick = ticks;
    if (RT_EOK != rt_timer_start(timer))
        return osErrorResource;
    return osOK;
}
 
/// Stop a timer.
/// \param[in]     timer_id      timer ID obtained by \ref osTimerNew.
/// \return status code that indicates the execution status of the function.
osStatus_t osTimerStop(osTimerId_t timer_id) {
    rt_timer_t timer = (rt_timer_t)timer_id;
    if (!timer)
        return osErrorParameter;
    if (RT_EOK != rt_timer_stop(timer))
        return osErrorResource;
    return osOK;
}
 
/// Check if a timer is running.
/// \param[in]     timer_id      timer ID obtained by \ref osTimerNew.
/// \return 0 not running, 1 running.
uint32_t osTimerIsRunning (osTimerId_t timer_id) {
    rt_timer_t timer = (rt_timer_t)timer_id;
    if (!timer)
        return 0;
    if (timer->parent.flag & RT_TIMER_FLAG_ACTIVATED)
        return 1;
    return 0;
}
 
/// Delete a timer.
/// \param[in]     timer_id      timer ID obtained by \ref osTimerNew.
/// \return status code that indicates the execution status of the function.
osStatus_t osTimerDelete(osTimerId_t timer_id) {
    rt_timer_t timer = (rt_timer_t)timer_id;
    if (!timer)
        return osErrorParameter;
    if (RT_EOK != rt_timer_delete(timer))
        return osErrorResource;
    return osOK;
}
 
 
//  ==== Event Flags Management Functions ====
 
/// Create and Initialize an Event Flags object.
/// \param[in]     attr          event flags attributes; NULL: default values.
/// \return event flags ID for reference by other functions or NULL in case of error.
osEventFlagsId_t osEventFlagsNew(const osEventFlagsAttr_t *attr) {
    osEventFlagsAttr_t dft_attr = {
        "event", 0,
        NULL, 0,
    };

    if (!attr)
        attr = &dft_attr;
    return (osEventFlagsId_t)rt_event_create(attr->name, RT_IPC_FLAG_FIFO);
}
 
/// Get name of an Event Flags object.
/// \param[in]     ef_id         event flags ID obtained by \ref osEventFlagsNew.
/// \return name as null-terminated string.
const char *osEventFlagsGetName(osEventFlagsId_t ef_id) {
    rt_event_t event = (rt_event_t)ef_id;
    if (!event)
        return NULL;
    return event->parent.parent.name;
}
 
/// Set the specified Event Flags.
/// \param[in]     ef_id         event flags ID obtained by \ref osEventFlagsNew.
/// \param[in]     flags         specifies the flags that shall be set.
/// \return event flags after setting or error code if highest bit set.
uint32_t osEventFlagsSet(osEventFlagsId_t ef_id, uint32_t flags) {
    rt_event_t event = (rt_event_t)ef_id;
    if (!event)
        return osFlagsErrorParameter;
    if (RT_EOK != rt_event_send(event, flags))
        return osFlagsErrorResource;
    return event->set;
}
 
/// Clear the specified Event Flags.
/// \param[in]     ef_id         event flags ID obtained by \ref osEventFlagsNew.
/// \param[in]     flags         specifies the flags that shall be cleared.
/// \return event flags before clearing or error code if highest bit set.
uint32_t osEventFlagsClear(osEventFlagsId_t ef_id, uint32_t flags) {
    uint32_t ret;
    register rt_ubase_t level;
    rt_event_t event = (rt_event_t)ef_id;
    if (!event)
        return osFlagsErrorParameter;
    ret = event->set;
    /* clear event */
    level = rt_hw_interrupt_disable();
    event->set &= ~flags;
    rt_hw_interrupt_enable(level);
    return ret;
}
 
/// Get the current Event Flags.
/// \param[in]     ef_id         event flags ID obtained by \ref osEventFlagsNew.
/// \return current event flags.
uint32_t osEventFlagsGet(osEventFlagsId_t ef_id) {
    rt_event_t event = (rt_event_t)ef_id;
    if (!event)
        return 0;
    return event->set;
}
 
/// Wait for one or more Event Flags to become signaled.
/// \param[in]     ef_id         event flags ID obtained by \ref osEventFlagsNew.
/// \param[in]     flags         specifies the flags to wait for.
/// \param[in]     options       specifies flags options (osFlagsXxxx).
/// \param[in]     timeout       \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
/// \return event flags before clearing or error code if highest bit set.
uint32_t osEventFlagsWait(osEventFlagsId_t ef_id, uint32_t flags, uint32_t options, uint32_t timeout) {
    rt_err_t ret;
    rt_uint32_t recv;
    rt_uint8_t opts = 0;
    rt_event_t event = (rt_event_t)ef_id;
    if (!event)
        return osFlagsErrorParameter;
    if (osFlagsWaitAll & options)
        opts |= RT_EVENT_FLAG_AND;
    else
        opts |= RT_EVENT_FLAG_OR;
    if (osFlagsNoClear & options)
        opts |= RT_EVENT_FLAG_CLEAR;
    ret = rt_event_recv(event, flags, opts, timeout, &recv);
    if (-RT_ETIMEOUT == ret)
        return osFlagsErrorTimeout;
    else if (RT_EOK != ret)
        return osFlagsErrorResource;
    else
        return recv;
}
 
/// Delete an Event Flags object.
/// \param[in]     ef_id         event flags ID obtained by \ref osEventFlagsNew.
/// \return status code that indicates the execution status of the function.
osStatus_t osEventFlagsDelete(osEventFlagsId_t ef_id) {
    rt_event_t event = (rt_event_t)ef_id;
    if (!event)
        return osFlagsErrorParameter;
    if (RT_EOK != rt_event_delete(event))
        return osFlagsErrorResource;
    return osOK;
}


//  ==== Mutex Management Functions ====
 
/// Create and Initialize a Mutex object.
/// \param[in]     attr          mutex attributes; NULL: default values.
/// \return mutex ID for reference by other functions or NULL in case of error.
osMutexId_t osMutexNew(const osMutexAttr_t *attr) {
    osMutexAttr_t dft_attr = {
        "mutex", 0,
        NULL, 0,
    };

    if (!attr)
        attr = &dft_attr;
    return (osMutexId_t)rt_mutex_create(attr->name, RT_IPC_FLAG_FIFO);
}
 
/// Get name of a Mutex object.
/// \param[in]     mutex_id      mutex ID obtained by \ref osMutexNew.
/// \return name as null-terminated string.
const char *osMutexGetName(osMutexId_t mutex_id) {
    rt_mutex_t mutex = (rt_mutex_t)mutex_id;
    if (!mutex)
        return NULL;
    return mutex->parent.parent.name;
}
 
/// Acquire a Mutex or timeout if it is locked.
/// \param[in]     mutex_id      mutex ID obtained by \ref osMutexNew.
/// \param[in]     timeout       \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
/// \return status code that indicates the execution status of the function.
osStatus_t osMutexAcquire(osMutexId_t mutex_id, uint32_t timeout) {
    rt_err_t ret;
    rt_mutex_t mutex = (rt_mutex_t)mutex_id;
    if (!mutex)
        return osErrorParameter;
    ret = rt_mutex_take(mutex, timeout);
    if (-RT_ETIMEOUT == ret)
        return osErrorTimeout;
    else if (RT_EOK != ret)
        return osErrorResource;
    else
        return osOK;
}
 
/// Release a Mutex that was acquired by \ref osMutexAcquire.
/// \param[in]     mutex_id      mutex ID obtained by \ref osMutexNew.
/// \return status code that indicates the execution status of the function.
osStatus_t osMutexRelease(osMutexId_t mutex_id) {
    rt_mutex_t mutex = (rt_mutex_t)mutex_id;
    if (!mutex)
        return osErrorParameter;
    if (RT_EOK != rt_mutex_release(mutex))
        return osErrorResource;
    return osOK;
}
 
/// Get Thread which owns a Mutex object.
/// \param[in]     mutex_id      mutex ID obtained by \ref osMutexNew.
/// \return thread ID of owner thread or NULL when mutex was not acquired.
osThreadId_t osMutexGetOwner(osMutexId_t mutex_id) {
    rt_mutex_t mutex = (rt_mutex_t)mutex_id;
    if (!mutex)
        return NULL;
    return (osThreadId_t)mutex->owner;
}
 
/// Delete a Mutex object.
/// \param[in]     mutex_id      mutex ID obtained by \ref osMutexNew.
/// \return status code that indicates the execution status of the function.
osStatus_t osMutexDelete(osMutexId_t mutex_id) {
    rt_mutex_t mutex = (rt_mutex_t)mutex_id;
    if (!mutex)
        return osErrorParameter;
    if (RT_EOK != rt_mutex_delete(mutex))
        return osErrorResource;
    return osOK;
}


//  ==== Semaphore Management Functions ====
 
/// Create and Initialize a Semaphore object.
/// \param[in]     max_count     maximum number of available tokens.
/// \param[in]     initial_count initial number of available tokens.
/// \param[in]     attr          semaphore attributes; NULL: default values.
/// \return semaphore ID for reference by other functions or NULL in case of error.
osSemaphoreId_t osSemaphoreNew(uint32_t max_count, uint32_t initial_count, const osSemaphoreAttr_t *attr) {
    osSemaphoreAttr_t dft_attr = {
        "semaphore", 0,
        NULL, 0,
    };
    semaphore_extra_t semaphore;

    if (!attr)
        attr = &dft_attr;

    do {
        if (NULL == (semaphore = (semaphore_extra_t)rt_malloc(sizeof(struct semaphore_extra))))
            break;
        if (RT_EOK != rt_sem_init(&semaphore->sem, attr->name, initial_count, RT_IPC_FLAG_FIFO))
            break;
        semaphore->max_count = max_count;
        return (osSemaphoreId_t)semaphore;
    } while (0);

    if (semaphore)
        rt_free(semaphore);
    return NULL;
}
 
/// Get name of a Semaphore object.
/// \param[in]     semaphore_id  semaphore ID obtained by \ref osSemaphoreNew.
/// \return name as null-terminated string.
const char *osSemaphoreGetName(osSemaphoreId_t semaphore_id) {
    semaphore_extra_t semaphore = (semaphore_extra_t)semaphore_id;
    if (!semaphore)
        return NULL;
    return semaphore->sem.parent.parent.name;
}
 
/// Acquire a Semaphore token or timeout if no tokens are available.
/// \param[in]     semaphore_id  semaphore ID obtained by \ref osSemaphoreNew.
/// \param[in]     timeout       \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
/// \return status code that indicates the execution status of the function.
osStatus_t osSemaphoreAcquire(osSemaphoreId_t semaphore_id, uint32_t timeout) {
    rt_err_t ret;
    semaphore_extra_t semaphore = (semaphore_extra_t)semaphore_id;
    if (!semaphore)
        return osErrorParameter;
    ret = rt_sem_take(&semaphore->sem, timeout);
    if (-RT_ETIMEOUT == ret)
        return osErrorTimeout;
    else if (RT_EOK != ret)
        return osErrorResource;
    else
        return osOK;
}
 
/// Release a Semaphore token up to the initial maximum count.
/// \param[in]     semaphore_id  semaphore ID obtained by \ref osSemaphoreNew.
/// \return status code that indicates the execution status of the function.
osStatus_t osSemaphoreRelease(osSemaphoreId_t semaphore_id) {
    semaphore_extra_t semaphore = (semaphore_extra_t)semaphore_id;
    if (!semaphore)
        return osErrorParameter;
    if (semaphore->sem.value >= semaphore->max_count)
        return osErrorResource;
    if (RT_EOK != rt_sem_release(&semaphore->sem))
        return osErrorResource;
    return osOK;
}
 
/// Get current Semaphore token count.
/// \param[in]     semaphore_id  semaphore ID obtained by \ref osSemaphoreNew.
/// \return number of tokens available.
uint32_t osSemaphoreGetCount(osSemaphoreId_t semaphore_id) {
    semaphore_extra_t semaphore = (semaphore_extra_t)semaphore_id;
    if (!semaphore)
        return 0;
    return (uint32_t)semaphore->sem.value;
}
 
/// Delete a Semaphore object.
/// \param[in]     semaphore_id  semaphore ID obtained by \ref osSemaphoreNew.
/// \return status code that indicates the execution status of the function.
osStatus_t osSemaphoreDelete(osSemaphoreId_t semaphore_id) {
    semaphore_extra_t semaphore = (semaphore_extra_t)semaphore_id;
    if (!semaphore)
        return osErrorParameter;
    if (RT_EOK != rt_sem_detach(&semaphore->sem))
        return osErrorResource;
    rt_free(semaphore);
    return osOK;
}


#ifdef RT_USING_MEMPOOL
//  ==== Memory Pool Management Functions ====
 
/// Create and Initialize a Memory Pool object.
/// \param[in]     block_count   maximum number of memory blocks in memory pool.
/// \param[in]     block_size    memory block size in bytes.
/// \param[in]     attr          memory pool attributes; NULL: default values.
/// \return memory pool ID for reference by other functions or NULL in case of error.
osMemoryPoolId_t osMemoryPoolNew(uint32_t block_count, uint32_t block_size, const osMemoryPoolAttr_t *attr) {
    osMemoryPoolAttr_t dft_attr = {
        "mempool", 0,
        NULL, 0,
        NULL, 0,
    };

    if (!attr)
        attr = &dft_attr;
    return (osMemoryPoolId_t)rt_mp_create(attr->name, block_count, block_size);
}
 
/// Get name of a Memory Pool object.
/// \param[in]     mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
/// \return name as null-terminated string.
const char *osMemoryPoolGetName(osMemoryPoolId_t mp_id) {
    rt_mp_t mempool = (rt_mp_t)mp_id;
    if (!mempool)
        return NULL;
    return mempool->parent.name;
}
 
/// Allocate a memory block from a Memory Pool.
/// \param[in]     mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
/// \param[in]     timeout       \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
/// \return address of the allocated memory block or NULL in case of no memory is available.
void *osMemoryPoolAlloc(osMemoryPoolId_t mp_id, uint32_t timeout) {
    rt_mp_t mempool = (rt_mp_t)mp_id;
    if (!mempool)
        return NULL;
    return rt_mp_alloc(mempool, timeout);
}
 
/// Return an allocated memory block back to a Memory Pool.
/// \param[in]     mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
/// \param[in]     block         address of the allocated memory block to be returned to the memory pool.
/// \return status code that indicates the execution status of the function.
osStatus_t osMemoryPoolFree(osMemoryPoolId_t mp_id, void *block) {
    rt_mp_t mempool = (rt_mp_t)mp_id;
    if (!mempool)
        return osErrorParameter;
    rt_mp_free(block);
    return osOK;
}
 
/// Get maximum number of memory blocks in a Memory Pool.
/// \param[in]     mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
/// \return maximum number of memory blocks.
uint32_t osMemoryPoolGetCapacity(osMemoryPoolId_t mp_id) {
    rt_mp_t mempool = (rt_mp_t)mp_id;
    if (!mempool)
        return 0;
    return mempool->block_total_count;
}
 
/// Get memory block size in a Memory Pool.
/// \param[in]     mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
/// \return memory block size in bytes.
uint32_t osMemoryPoolGetBlockSize(osMemoryPoolId_t mp_id) {
    rt_mp_t mempool = (rt_mp_t)mp_id;
    if (!mempool)
        return 0;
    return mempool->block_size;
}
 
/// Get number of memory blocks used in a Memory Pool.
/// \param[in]     mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
/// \return number of memory blocks used.
uint32_t osMemoryPoolGetCount(osMemoryPoolId_t mp_id) {
    rt_mp_t mempool = (rt_mp_t)mp_id;
    if (!mempool)
        return 0;
    return (mempool->block_total_count - mempool->block_free_count);
}
 
/// Get number of memory blocks available in a Memory Pool.
/// \param[in]     mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
/// \return number of memory blocks available.
uint32_t osMemoryPoolGetSpace(osMemoryPoolId_t mp_id) {
    rt_mp_t mempool = (rt_mp_t)mp_id;
    if (!mempool)
        return 0;
    return mempool->block_free_count;
}
 
/// Delete a Memory Pool object.
/// \param[in]     mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
/// \return status code that indicates the execution status of the function.
osStatus_t osMemoryPoolDelete(osMemoryPoolId_t mp_id) {
    rt_mp_t mempool = (rt_mp_t)mp_id;
    if (!mempool)
        return osErrorParameter;
    if (RT_EOK != rt_mp_delete(mempool))
        return osErrorResource;
    return osOK;
}
#endif /* RT_USING_MEMPOOL */


#ifdef RT_USING_MESSAGEQUEUE
//  ==== Message Queue Management Functions ====
 
/// Create and Initialize a Message Queue object.
/// \param[in]     msg_count     maximum number of messages in queue.
/// \param[in]     msg_size      maximum message size in bytes.
/// \param[in]     attr          message queue attributes; NULL: default values.
/// \return message queue ID for reference by other functions or NULL in case of error.
osMessageQueueId_t osMessageQueueNew(uint32_t msg_count, uint32_t msg_size, const osMessageQueueAttr_t *attr) {
    osMessageQueueAttr_t dft_attr = {
        "msgqueue", 0,
        NULL, 0,
        NULL, 0,
    };

    if (!attr)
        attr = &dft_attr;

    return (osMessageQueueId_t)rt_mq_create(attr->name, msg_size, msg_count, RT_IPC_FLAG_FIFO);
}
 
/// Get name of a Message Queue object.
/// \param[in]     mq_id         message queue ID obtained by \ref osMessageQueueNew.
/// \return name as null-terminated string.
const char *osMessageQueueGetName(osMessageQueueId_t mq_id) {
    rt_mq_t msgqueue = (rt_mq_t)mq_id;
    if (!msgqueue)
        return NULL;
    return msgqueue->parent.parent.name;
}
 
/// Put a Message into a Queue or timeout if Queue is full.
/// \param[in]     mq_id         message queue ID obtained by \ref osMessageQueueNew.
/// \param[in]     msg_ptr       pointer to buffer with message to put into a queue.
/// \param[in]     msg_prio      message priority.
/// \param[in]     timeout       \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
/// \return status code that indicates the execution status of the function.
osStatus_t osMessageQueuePut(osMessageQueueId_t mq_id, const void *msg_ptr, uint8_t msg_prio, uint32_t timeout) {
    rt_err_t ret;
    rt_mq_t msgqueue = (rt_mq_t)mq_id;
    if (!msgqueue)
        return osErrorParameter;
    (void)msg_prio;
    (void)timeout;

    ret = rt_mq_send(msgqueue, (void *)msg_ptr, msgqueue->msg_size);
    if (-RT_EFULL == ret)
        return osErrorTimeout;
    else if (RT_EOK != ret)
        return osErrorResource;
    return osOK;
}
 
/// Get a Message from a Queue or timeout if Queue is empty.
/// \param[in]     mq_id         message queue ID obtained by \ref osMessageQueueNew.
/// \param[out]    msg_ptr       pointer to buffer for message to get from a queue.
/// \param[out]    msg_prio      pointer to buffer for message priority or NULL.
/// \param[in]     timeout       \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
/// \return status code that indicates the execution status of the function.
osStatus_t osMessageQueueGet(osMessageQueueId_t mq_id, void *msg_ptr, uint8_t *msg_prio, uint32_t timeout) {
    rt_err_t ret;
    rt_mq_t msgqueue = (rt_mq_t)mq_id;
    if (!msgqueue)
        return osErrorParameter;
    (void)msg_prio;

    ret = rt_mq_recv(msgqueue, msg_ptr, msgqueue->msg_size, timeout);
    if (-RT_ETIMEOUT == ret)
        return osErrorTimeout;
    else if (RT_EOK != ret)
        return osErrorResource;
    return osOK;
}
 
/// Get maximum number of messages in a Message Queue.
/// \param[in]     mq_id         message queue ID obtained by \ref osMessageQueueNew.
/// \return maximum number of messages.
uint32_t osMessageQueueGetCapacity(osMessageQueueId_t mq_id) {
    rt_mq_t msgqueue = (rt_mq_t)mq_id;
    if (!msgqueue)
        return 0;
    return (uint32_t)msgqueue->max_msgs;
}
 
/// Get maximum message size in a Message Queue.
/// \param[in]     mq_id         message queue ID obtained by \ref osMessageQueueNew.
/// \return maximum message size in bytes.
uint32_t osMessageQueueGetMsgSize(osMessageQueueId_t mq_id) {
    rt_mq_t msgqueue = (rt_mq_t)mq_id;
    if (!msgqueue)
        return 0;
    return (uint32_t)msgqueue->msg_size;
}
 
/// Get number of queued messages in a Message Queue.
/// \param[in]     mq_id         message queue ID obtained by \ref osMessageQueueNew.
/// \return number of queued messages.
uint32_t osMessageQueueGetCount(osMessageQueueId_t mq_id) {
    rt_mq_t msgqueue = (rt_mq_t)mq_id;
    if (!msgqueue)
        return 0;
    return (uint32_t)msgqueue->entry;
}
 
/// Get number of available slots for messages in a Message Queue.
/// \param[in]     mq_id         message queue ID obtained by \ref osMessageQueueNew.
/// \return number of available slots for messages.
uint32_t osMessageQueueGetSpace(osMessageQueueId_t mq_id) {
    rt_mq_t msgqueue = (rt_mq_t)mq_id;
    if (!msgqueue)
        return 0;
    return (uint32_t)(msgqueue->max_msgs - msgqueue->entry);
}
 
/// Reset a Message Queue to initial empty state.
/// \param[in]     mq_id         message queue ID obtained by \ref osMessageQueueNew.
/// \return status code that indicates the execution status of the function.
osStatus_t osMessageQueueReset(osMessageQueueId_t mq_id) {
    rt_mq_t msgqueue = (rt_mq_t)mq_id;
    if (!msgqueue)
        return osErrorParameter;
    return osOK;
}
 
/// Delete a Message Queue object.
/// \param[in]     mq_id         message queue ID obtained by \ref osMessageQueueNew.
/// \return status code that indicates the execution status of the function.
osStatus_t osMessageQueueDelete(osMessageQueueId_t mq_id) {
    rt_mq_t msgqueue = (rt_mq_t)mq_id;
    if (!msgqueue)
        return osErrorParameter;
    if (RT_EOK != rt_mq_delete(msgqueue))
        return osErrorResource;
    return osOK;
}
#endif /* RT_USING_MESSAGEQUEUE */
