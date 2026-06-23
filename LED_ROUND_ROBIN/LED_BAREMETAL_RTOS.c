#include <stdint.h>

#define NULL ((void *)0)

// --- 1. MEMORY-MAPPED PERIPHERAL ADDRESSES ---
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

// --- 2. CONFIGURATION ---
#define SYS_CLOCK_HZ     16000000U  // 16 MHz default HSI Clock
#define TICK_RATE_HZ     1000U      // 1 ms Tick
#define STACK_SIZE       256U       // 1024 bytes per task

// --- 3. RTOS STRUCTURES ---
typedef struct {
    uint32_t *stackPtr;
} TCB_t;

TCB_t tcb[2];
TCB_t *currentTask = NULL;
uint8_t currentTaskIdx = 0;

uint32_t task1_stack[STACK_SIZE] __attribute__((aligned(8)));
uint32_t task2_stack[STACK_SIZE] __attribute__((aligned(8)));

// --- 4. HARDWARE & TASKS ---
void Hardware_Init(void) {
    RCC_AHB1ENR |= (1U << 0) | (1U << 1); // Enable GPIOA & GPIOB Clocks

    GPIOA_MODER &= ~(3U << (5 * 2));
    GPIOA_MODER |=  (1U << (5 * 2));     // PA5 to Output (User LED)

    GPIOB_MODER &= ~(3U << (0 * 2));
    GPIOB_MODER |=  (1U << (0 * 2));     // PB0 to Output (External LED)
}

void Task1_BlinkPA5(void) {
    while(1) {
        GPIOA_ODR ^= (1U << 5);
        for(volatile int i = 0; i < 300000; i++); // Simple delay
    }
}

void Task2_BlinkPB0(void) {
    while(1) {
        GPIOB_ODR ^= (1U << 0);
        for(volatile int i = 0; i < 150000; i++); // Simple delay
    }
}

// --- 5. TASK INITIALIZER ---
void RTOS_CreateTask(TCB_t *pTcb, void (*taskFunc)(void), uint32_t *stack, uint32_t stackSize) {
    uint32_t *topOfStack = &stack[stackSize];

    // Artificial Hardware Stack frame
    *(--topOfStack) = 0x01000000U;         // xPSR (Thumb State)
    *(--topOfStack) = (uint32_t)taskFunc;  // PC (Program Counter)
    *(--topOfStack) = 0x00000000U;         // LR
    *(--topOfStack) = 0x00000012U;         // R12
    *(--topOfStack) = 0x00000003U;         // R3
    *(--topOfStack) = 0x00000002U;         // R2
    *(--topOfStack) = 0x00000001U;         // R1
    *(--topOfStack) = 0x00000000U;         // R0

    // Artificial Software Stack frame
    *(--topOfStack) = 0x00000011U;         // R11
    *(--topOfStack) = 0x00000010U;         // R10
    *(--topOfStack) = 0x00000009U;         // R9
    *(--topOfStack) = 0x00000008U;         // R8
    *(--topOfStack) = 0x00000007U;         // R7
    *(--topOfStack) = 0x00000006U;         // R6
    *(--topOfStack) = 0x00000005U;         // R5
    *(--topOfStack) = 0x00000004U;         // R4

    pTcb->stackPtr = topOfStack;
}

// --- 6. ROUND-ROBIN SCHEDULER TRIGGER ---
void SysTick_Handler(void) {
    currentTaskIdx = (currentTaskIdx + 1) % 2; // Swap index between 0 and 1
    SCB_ICSR |= (1U << 28);                    // Trigger PendSV context switch
}

// --- 7. CONTEXT SWITCHER ENGINE ---
__attribute__((naked)) void PendSV_Handler(void) {
    __asm volatile (
        "CPSID I \n"                    // Disable interrupts globally during context switch

        /* --- SAVE ACTIVE TASK CONTEXT --- */
        "LDR R0, =currentTask \n"
        "LDR R1, [R0] \n"
        "CBZ R1, skip_save \n"          // If currentTask == NULL (first run), skip save step

        "MRS R2, PSP \n"                // Read current Process Stack Pointer
        "STMDB R2!, {R4-R11} \n"        // Push manual registers R4 through R11 onto stack
        "STR R2, [R1] \n"               // Store current stack pointer value to TCB

        "skip_save: \n"

        /* --- LOAD NEXT TASK CONTEXT --- */
        "LDR R2, =currentTaskIdx \n"
        "LDRB R3, [R2] \n"
        "LDR R4, =tcb \n"
        "MOV R5, #4 \n"
        "MLA R1, R3, R5, R4 \n"         // Find address element: &tcb[currentTaskIdx]
        "STR R1, [R0] \n"               // currentTask = &tcb[currentTaskIdx]

        "LDR R2, [R1] \n"               // Get stack pointer for target task
        "LDMIA R2!, {R4-R11} \n"        // Pop manual registers R4 through R11
        "MSR PSP, R2 \n"                // Set CPU active stack to this task's PSP

        "MOV R0, #0xFFFFFFFD \n"        // Exception return value code: return to Thread mode using PSP
        "CPSIE I \n"                    // Re-enable interrupts
        "BX R0 \n"                      // Hardware automatically pops R0-R3, R12, LR, PC, xPSR
    );
}

// --- 8. MAIN METHOD ---
int main(void) {
    Hardware_Init();

    // Map tasks to individual context loops
    RTOS_CreateTask(&tcb[0], Task1_BlinkPA5, task1_stack, STACK_SIZE);
    RTOS_CreateTask(&tcb[1], Task2_BlinkPB0, task2_stack, STACK_SIZE);

    currentTask = NULL;
    currentTaskIdx = 0;

    // Set PendSV priority level to lowest (0xFF)
    SCB_SHPR3 |= (0xFFU << 16);

    // Setup SysTick Heartbeat (1ms tick interval)
    SYSTICK_LOAD = (SYS_CLOCK_HZ / TICK_RATE_HZ) - 1U;
    SYSTICK_VAL  = 0U;
    SYSTICK_CTRL = (1U << 2) | (1U << 1) | (1U << 0); // Core Clock, Exception Enable, Enable Counter

    // Prime the processor Process Stack Pointer (PSP) to 0 to kickoff execution structure
    __asm volatile("MOV R0, #0 \n MSR PSP, R0");

    while(1);
}
