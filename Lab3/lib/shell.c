#include "uart.h"
#include "mbox.h"
#include "string.h"
#include "malloc.h"
#include "cpio.h"
#include "exec.h"
#include "timer.h"
#include "interrupt.h"

extern char *cpio_start;

/* mbox */
void get_board_revision(){
    mbox[0] = 7 * 4; // buffer size in bytes
    mbox[1] = MBOX_REQUEST;
    // tags begin
    mbox[2] = 0x00010002; // tag identifier : GET_BOARD_REVISION
    mbox[3] = 4; // maximum of request and response value buffer's length.
    mbox[4] = 4;
    mbox[5] = 0; // value buffer
    // tags end
    mbox[6] = MBOX_TAG_LAST;

    mbox_call(MBOX_CH_PROP); // message passing procedure call, you should implement it following the 6 steps provided above.

    uart_puts("board version:    0x");
    uart_hex(mbox[5]);
    uart_puts("\n");
}

void get_arm_memory(){
    mbox[0] = 8 * 4; // buffer size in bytes
    mbox[1] = MBOX_REQUEST;
    // tags begin
    mbox[2] = 0x00010005; // tag identifier: GET_ARM_MEMORY
    mbox[3] = 8; // maximum of request and response value buffer's length.
    mbox[4] = 8;
    mbox[5] = 0; // value buffer
    mbox[6] = 0; // value buffer
    // tags end
    mbox[7] = MBOX_TAG_LAST;

    mbox_call(MBOX_CH_PROP); // message passing procedure call, you should implement it following the 6 steps provided above.

    uart_puts("Arm base address: 0x");
    uart_hex(mbox[5]);
    uart_puts("\n");
    uart_puts("Arm memory size:  0x");
    uart_hex(mbox[6]);
    uart_puts("\n");
}

/* reboot */
#define PM_PASSWORD 0x5a000000
#define PM_RSTC 0x3F10001c
#define PM_WDOG 0x3F100024

void set(long addr, unsigned int value) {
    volatile unsigned int* point = (unsigned int*)addr;
    *point = value;
}

void reset(int tick) {                 // reboot after watchdog timer expire
    set(PM_RSTC, PM_PASSWORD | 0x20);  // full reset
    set(PM_WDOG, PM_PASSWORD | tick);  // number of watchdog tick
}

void cancel_reset() {
    set(PM_RSTC, PM_PASSWORD | 0);  // full reset
    set(PM_WDOG, PM_PASSWORD | 0);  // number of watchdog tick
}

/* cpio */
int cat(char *thefilepath)
{
    char *filepath;
    char *filedata;
    unsigned int filesize;
    struct cpio_newc_header *header_pointer = (struct cpio_newc_header *)cpio_start;

    while (header_pointer)
    {
        int error = cpio_newc_parse_header(header_pointer, &filepath, &filesize, &filedata, &header_pointer);
        // if parse header error
        if (error)
        {
            uart_printf("error\n");
            break;
        }

        if (!strcmp(thefilepath, filepath))
        {
            for (unsigned int i = 0; i < filesize; i++)
                uart_printf("%c", filedata[i]);
            uart_printf("\n");
            break;
        }

        if (header_pointer == 0)
            uart_printf("cat: %s: No such file or directory\r\n", thefilepath);
    }
    return 0;
}

int ls(char *working_dir)
{
    char *filepath;
    char *filedata;
    unsigned int filesize;
    struct cpio_newc_header *header_pointer = (struct cpio_newc_header *)cpio_start;

    while (header_pointer)
    {
        int error = cpio_newc_parse_header(header_pointer, &filepath, &filesize, &filedata, &header_pointer);
        // if parse header error
        if (error)
        {
            uart_printf("error\n");
            break;
        }

        // if this is not TRAILER!!! (last of file)
        if (header_pointer != 0)
            uart_printf("%s\n", filepath);
    }
    return 0;
}

/* timer */
unsigned long long get_clock_time() {
    unsigned long long current_tick;
    asm volatile(
        "mrs x1, cntpct_el0\n\t":"=r"(current_tick):
    );

    unsigned long long tick_freq;
    asm volatile(
        "mrs x1, cntfrq_el0\n\t":"=r"(current_tick):
    );

    return current_tick / tick_freq;
}

void print_timeout(unsigned long long sec) {
    uart_printf("Set Timeout %d sec.\n", sec);
}

void twoSec(unsigned long long sec) {
    uart_printf("Current second: %d\n", get_clock_time());
    add_timer(twoSec, 2); // continuously set timeout 2 sec
}

/* shell */
void shell(void) {
    uart_puts("! Welcome Lab3 !\n");

    char command[32];
    int idx = 0;

    while (1) {
        idx = 0;
        uart_puts("$ ");
        while (1) {
            command[idx] = uart_getc();
            if (command[idx] == '\n') {
                command[idx] = '\0';
                break;
            }
            idx++;
        }

        // Lab 1
        if (strcmp("hello", command) == 0) {
            uart_puts("Hello Kernel!\n");
        }
        else if (strcmp("help", command) == 0) {
            uart_puts("help\t: print this help menu\n");
            uart_puts("hello\t: print Hello World!\n");
            uart_puts("reboot\t: reboot this device\n");
            uart_puts("lshw\t: print hardware info from mailbox\n");
            uart_puts("malloc\t: allocate memory to specific string\n");
            uart_puts("ls\t: print files and directories\n");
            uart_puts("cat [FILE]\t: print specific file\n");
            uart_puts("exec [FILE]\t: execute specific file\n");
        }
        else if (strcmp("reboot", command) == 0) {
            uart_puts("rebooting...\n");
            reset(1000);
        }
        else if (strcmp("lshw", command) == 0) {
            get_board_revision();
            get_arm_memory();
        }
        // Lab 2
        else if (strcmp("malloc", command) == 0) {
            uart_puts("allocating...\n");
            char* str = (char*) simple_malloc(8);
            *str = 'a';
            *(str + 1) = 'b';
            *(str + 2) = 'c';
            *(str + 3) = '\0';
            uart_printf("%s\n", str);
        }
        else if (strcmp("ls", command) == 0) {
            ls(".");
        }
        else if (strncmp("cat", command, 3) == 0) {
            cat(command + 4);
        }
        // Lab 3
        else if (strncmp("exec", command, 4) == 0) {
            exec_file(command + 5);
        }
        else if (strncmp("asyncPut", command, 8) == 0) {
            uart_async_putc(command + 9);
        }
        else if (strncmp("setTimeout", command, 10) == 0) {
            int sec = atoi(command + 11);
            add_timer(print_timeout, sec);
        }
        else if (strcmp("twoSec", command) == 0) {
            add_timer(twoSec, 2);
        }
        else {
            uart_puts("unknown\n");
        }
        
    }
        
}

