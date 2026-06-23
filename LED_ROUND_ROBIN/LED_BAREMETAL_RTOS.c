#include <stdint.h>

#define NULL ((void *)0)

#define SCB_ICSR         (*(volatile uint32_t *)0xE000ED04U)
#define SCB_SHPR3        (*(volatile uint32_t *)0xE000ED20U)
#define SYSTICK_CTRL     (*(volatile uint32_t *)0xE000E010U)
#define SYSTICK_LOAD     (*(volatile uint32_t *)0xE000E014U)
#define SYSTICK_VAL      (*(volatile uint32_t *)0xE000E018U)

#define RCC_AHB1ENR      (*(volatile uint32_t *)0x40023830U)
#define GPIOA_MODER      (*(volatile uint32_t *)0x40020000U)
#define GPIOA_ODR        (*(volatile uint32_t *)0x40020014U)
#define GPIOB_MODER      (*(volatile uint32_t *)0x40020400U)
#define GPIOB_ODR        (*(volatile uint32_t *)0x40020414U)

#define SYS_CLOCK_HZ     16000000U
#define TICK_RATE_HZ     1000U
#define STACK_SIZE       256U

typedef struct
{
    uint32_t *stackPtr;
} TCB_t;

TCB_t tcb[2];
TCB_t *currentTask = NULL;
uint8_t currentTaskIdx = 0;

uint32_t task1_stack[STACK_SIZE] __attribute__((aligned(8)));
uint32_t task2_stack[STACK_SIZE] __attribute__((aligned(8)));

void Hardware_Init(void)
{
    RCC_AHB1ENR |= (1U << 0) | (1U << 1);

    GPIOA_MODER &= ~(3U << (5 * 2));
    GPIOA_MODER |=  (1U << (5 * 2));

    GPIOB_MODER &= ~(3U << (0 * 2));
    GPIOB_MODER |=  (1U << (0 * 2));
}

void Task1_BlinkPA5(void)
{
    while(1)
    {
        GPIOA_ODR ^= (1U << 5);
        for(volatile int i = 0; i < 300000; i++);
    }
}

void Task2_BlinkPB0(void)
{
    while(1)
    {
        GPIOB_ODR ^= (1U << 0);
        for(volatile int i = 0; i < 150000; i++);
    }
}

void RTOS_CreateTask(TCB_t *pTcb, void (*taskFunc)(void), uint32_t *stack, uint32_t stackSize)
{
    uint32_t *topOfStack = &stack[stackSize];

    *(--topOfStack) = 0x01000000U;
    *(--topOfStack) = (uint32_t)taskFunc;
    *(--topOfStack) = 0x00000000U;
    *(--topOfStack) = 0x00000012U;
    *(--topOfStack) = 0x00000003U;
    *(--topOfStack) = 0x00000002U;
    *(--topOfStack) = 0x00000001U;
    *(--topOfStack) = 0x00000000U;

    *(--topOfStack) = 0x00000011U;
    *(--topOfStack) = 0x00000010U;
    *(--topOfStack) = 0x00000009U;
    *(--topOfStack) = 0x00000008U;
    *(--topOfStack) = 0x00000007U;
    *(--topOfStack) = 0x00000006U;
    *(--topOfStack) = 0x00000005U;
    *(--topOfStack) = 0x00000004U;

    pTcb->stackPtr = topOfStack;
}

void SysTick_Handler(void)
{
    currentTaskIdx = (currentTaskIdx + 1) % 2;
    SCB_ICSR |= (1U << 28);
}

__attribute__((naked)) void PendSV_Handler(void)
{
    __asm volatile (
        "CPSID I                    \n"

        "LDR R0, =currentTask       \n"
        "LDR R1, [R0]              \n"
        "CBZ R1, skip_save         \n"

        "MRS R2, PSP               \n"
        "STMDB R2!, {R4-R11}       \n"
        "STR R2, [R1]              \n"

        "skip_save:                \n"

        "LDR R2, =currentTaskIdx   \n"
        "LDRB R3, [R2]            \n"
        "LDR R4, =tcb             \n"
        "MOV R5, #4               \n"
        "MLA R1, R3, R5, R4       \n"
        "STR R1, [R0]             \n"

        "LDR R2, [R1]             \n"
        "LDMIA R2!, {R4-R11}      \n"
        "MSR PSP, R2              \n"

        "MOV R0, #0xFFFFFFFD      \n"
        "CPSIE I                  \n"
        "BX R0                    \n"
    );
}

int main(void)
{
    Hardware_Init();

    RTOS_CreateTask(&tcb[0], Task1_BlinkPA5, task1_stack, STACK_SIZE);
    RTOS_CreateTask(&tcb[1], Task2_BlinkPB0, task2_stack, STACK_SIZE);

    currentTask = NULL;
    currentTaskIdx = 0;

    SCB_SHPR3 |= (0xFFU << 16);

    SYSTICK_LOAD = (SYS_CLOCK_HZ / TICK_RATE_HZ) - 1U;
    SYSTICK_VAL  = 0U;
    SYSTICK_CTRL = (1U << 2) | (1U << 1) | (1U << 0);

    __asm volatile(
        "MOV R0, #0      \n"
        "MSR PSP, R0     \n"
    );

    while(1);
}
