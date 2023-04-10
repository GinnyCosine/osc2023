#include "timer.h"
#include "malloc.h"
#include "list.h"
#include "uart.h"
#include "interrupt.h"

#define STR(x)        #x
#define XSTR(x)       STR(x)

extern list_head_t * timer_event_head;

void core_timer_interrupt_enable() {
    asm volatile(
        "mov x2, 2\n\t"
        "ldr x1, =" XSTR(CORE0_TIMER_IRQ_CTRL) "\n\t"
        "str w2, [x1]"
    );
}

void core_timer_interrupt_disable() {
    asm volatile(
        "mov x2, 0\n\t"
        "ldr x1, =" XSTR(CORE0_TIMER_IRQ_CTRL) "\n\t"
        "str w2, [x1]"
    );
}

unsigned long long get_clock_freq() {
    unsigned long long tick_freq;
    asm volatile(
        "mrs %0, cntfrq_el0\n\t":"=r"(tick_freq)
    );
    return tick_freq;
}

unsigned long long get_current_tick() {
    unsigned long long current_tick;
    asm volatile(
        "mrs %0, cntpct_el0\n\t":"=r"(current_tick)
    );
    return current_tick;
}

unsigned long long get_clock_time() {
    return get_current_tick() / get_clock_freq();
}

void set_core_timer_interrupt(unsigned long long time) {
    asm volatile(
        "msr cntp_tval_el0, %0\n\t"::"r"(time)
    );
}

void core_timer_interrupt_disable_alternative() {
    set_core_timer_interrupt(100000);
    // set_core_timer_interrupt(get_clock_freq() * 15);
}

void enable_core_timer() {
    asm volatile(
        "mov x1, 1\n\t"
        "msr cntp_ctl_el0, x1\n\t"
    );
}

void core_timer_handler() {
    if (list_empty(timer_event_head)) {
        // core_timer_interrupt_disable();
        core_timer_interrupt_disable_alternative();
    }
    else {
        pop_timer();
    }
}

void add_timer(timer_callback_t callback, char * msg, unsigned long long expire_time) {
    timer_event_t * new_event = (timer_event_t *)simple_malloc(sizeof(timer_event_t));
    new_event->expire_time = expire_time;
    new_event->callback = callback;
    new_event->args = msg;

    disable_interrupt();
    list_head_t * iter;
    int not_insert_flag = 1;
    list_for_each(iter, timer_event_head) {
        timer_event_t * cur_event = (timer_event_t *) list_entry(iter, timer_event_t, listhead);
        if (expire_time < cur_event->expire_time) {
            not_insert_flag = 0;
            list_add(&new_event->listhead, iter->prev);
            break;
        }
    }
    if (not_insert_flag) {
        list_add_tail(&new_event->listhead, timer_event_head);
    }
    
    set_core_timer_interrupt_to_first();
    enable_interrupt();

    core_timer_interrupt_enable();
}

void pop_timer() {
    timer_event_t * first = (timer_event_t *) list_entry(timer_event_head->next, timer_event_t, listhead);
    disable_interrupt();
    list_del_entry(timer_event_head->next);
    enable_interrupt();
    first->callback(first->args);
    if (list_empty(timer_event_head)) {
        core_timer_interrupt_disable();
    }
    else {
        disable_interrupt();
        set_core_timer_interrupt_to_first();
        enable_interrupt();
    }
}

void set_core_timer_interrupt_to_first() {
    unsigned long long min_expired_time = ((timer_event_t *) list_entry(timer_event_head->next, timer_event_t, listhead))->expire_time;
    set_core_timer_interrupt(min_expired_time);
} 