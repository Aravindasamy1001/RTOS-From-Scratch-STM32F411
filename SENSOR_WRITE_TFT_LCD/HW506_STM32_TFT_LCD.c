#include <stdint.h>

#define NULL ((void *)0)

#define SCB_ICSR         (*(volatile uint32_t *)0xE000ED04U)
#define SCB_SHPR3        (*(volatile uint32_t *)0xE000ED20U)
#define SYSTICK_CTRL     (*(volatile uint32_t *)0xE000E010U)
#define SYSTICK_LOAD     (*(volatile uint32_t *)0xE000E014U)
#define SYSTICK_VAL      (*(volatile uint32_t *)0xE000E018U)

#define RCC_AHB1ENR      (*(volatile uint32_t *)0x40023830U)
#define RCC_APB2ENR      (*(volatile uint32_t *)0x40023844U)

#define GPIOA_MODER      (*(volatile uint32_t *)0x40020000U)
#define GPIOA_OSPEEDR    (*(volatile uint32_t *)0x40020008U)
#define GPIOA_ODR        (*(volatile uint32_t *)0x40020014U)
#define GPIOA_BSRR       (*(volatile uint32_t *)0x40020018U)

#define GPIOB_MODER      (*(volatile uint32_t *)0x40020400U)
#define GPIOB_OSPEEDR    (*(volatile uint32_t *)0x40020408U)
#define GPIOB_AFRL       (*(volatile uint32_t *)0x40020420U)

#define ADC1_SR          (*(volatile uint32_t *)0x40012000U)
#define ADC1_CR2         (*(volatile uint32_t *)0x40012008U)
#define ADC1_SQR3        (*(volatile uint32_t *)0x40012034U)
#define ADC1_DR          (*(volatile uint32_t *)0x4001204CU)

#define SPI1_CR1         (*(volatile uint32_t *)0x40013000U)
#define SPI1_SR          (*(volatile uint32_t *)0x4001300CU)
#define SPI1_DR          (*(volatile uint32_t *)0x4001300CU)

#define SYS_CLOCK_HZ     16000000U
#define TICK_RATE_HZ     1000U
#define STACK_SIZE       256U

typedef struct {
    uint32_t *stackPtr;
} TCB_t;

TCB_t tcb[2];
TCB_t *currentTask = NULL;
uint8_t currentTaskIdx = 0;

uint32_t task1_stack[STACK_SIZE] __attribute__((aligned(8)));
uint32_t task2_stack[STACK_SIZE] __attribute__((aligned(8)));

volatile uint32_t g_sensor_value = 0;

void Hardware_Init(void)
{
    RCC_AHB1ENR |= (1U << 0) | (1U << 1);
    RCC_APB2ENR |= (1U << 8) | (1U << 12);

    GPIOA_MODER |= (3U << (4 * 2));
    ADC1_SQR3 = 4U;
    ADC1_CR2 |= (1U << 0);

    GPIOA_MODER &= ~((3U << 0) | (3U << 2) | (3U << 4));
    GPIOA_MODER |= ((1U << 0) | (1U << 2) | (1U << 4));
    GPIOA_OSPEEDR |= ((3U << 0) | (3U << 2) | (3U << 4));

    GPIOB_MODER &= ~((3U << (3 * 2)) |
                     (3U << (4 * 2)) |
                     (3U << (5 * 2)));

    GPIOB_MODER |= ((2U << (3 * 2)) |
                    (2U << (4 * 2)) |
                    (2U << (5 * 2)));

    GPIOB_OSPEEDR |= ((3U << (3 * 2)) |
                      (3U << (4 * 2)) |
                      (3U << (5 * 2)));

    GPIOB_AFRL &= ~((0xFU << (3 * 4)) |
                    (0xFU << (4 * 4)) |
                    (0xFU << (5 * 4)));

    GPIOB_AFRL |= ((5U << (3 * 4)) |
                   (5U << (4 * 4)) |
                   (5U << (5 * 4)));

    SPI1_CR1 = 0;
    SPI1_CR1 |= (1U << 2);
    SPI1_CR1 |= (4U << 3);
    SPI1_CR1 |= (1U << 9);
    SPI1_CR1 |= (1U << 8);
    SPI1_CR1 |= (1U << 6);
}

uint32_t ADC_Read(void)
{
    ADC1_CR2 |= (1U << 30);

    while (!(ADC1_SR & (1U << 1)));

    return ADC1_DR;
}

uint8_t SPI1_Transfer(uint8_t data)
{
    while (!(SPI1_SR & (1U << 1)));

    *(volatile uint8_t *)&SPI1_DR = data;

    while (!(SPI1_SR & (1U << 0)));

    return *(volatile uint8_t *)&SPI1_DR;
}

void TFT_SendCommand(uint8_t cmd)
{
    GPIOA_BSRR = (1U << 18);
    GPIOA_BSRR = (1U << 16);

    SPI1_Transfer(cmd);

    GPIOA_BSRR = (1U << 0);
}

void TFT_SendData(uint8_t data)
{
    GPIOA_BSRR = (1U << 2);
    GPIOA_BSRR = (1U << 16);

    SPI1_Transfer(data);

    GPIOA_BSRR = (1U << 0);
}

void TFT_SetWindow(uint16_t x0,
                   uint16_t y0,
                   uint16_t x1,
                   uint16_t y1)
{
    TFT_SendCommand(0x2A);

    TFT_SendData((x0 >> 8) & 0xFF);
    TFT_SendData(x0 & 0xFF);
    TFT_SendData((x1 >> 8) & 0xFF);
    TFT_SendData(x1 & 0xFF);

    TFT_SendCommand(0x2B);

    TFT_SendData((y0 >> 8) & 0xFF);
    TFT_SendData(y0 & 0xFF);
    TFT_SendData((y1 >> 8) & 0xFF);
    TFT_SendData(y1 & 0xFF);
}

void Task1_ReadSensor(void)
{
    while (1)
    {
        g_sensor_value = ADC_Read();

        for (volatile int i = 0; i < 40000; i++);
    }
}

void Task2_UpdateDisplay(void)
{
    GPIOA_BSRR = (1U << 17);

    for (volatile int i = 0; i < 40000; i++);

    GPIOA_BSRR = (1U << 1);

    for (volatile int i = 0; i < 40000; i++);

    TFT_SendCommand(0x11);

    for (volatile int i = 0; i < 60000; i++);

    TFT_SendCommand(0x3A);
    TFT_SendData(0x55);

    TFT_SendCommand(0x20);

    TFT_SendCommand(0x29);

    for (volatile int i = 0; i < 60000; i++);

    while (1)
    {
        uint16_t local_adc = g_sensor_value;
        uint16_t dynamic_color = (local_adc << 4);

        TFT_SetWindow(0, 0, 49, 49);

        TFT_SendCommand(0x2C);

        for (int i = 0; i < 2500; i++)
        {
            TFT_SendData((dynamic_color >> 8) & 0xFF);
            TFT_SendData(dynamic_color & 0xFF);
        }

        for (volatile int i = 0; i < 100000; i++);
    }
}

void RTOS_CreateTask(TCB_t *pTcb,
                     void (*taskFunc)(void),
                     uint32_t *stack,
                     uint32_t stackSize)
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
        "CPSID I \n"
        "LDR R0, =currentTask \n"
        "LDR R1, [R0] \n"
        "CBZ R1, skip_save \n"
        "MRS R2, PSP \n"
        "STMDB R2!, {R4-R11} \n"
        "STR R2, [R1] \n"

        "skip_save: \n"
        "LDR R2, =currentTaskIdx \n"
        "LDRB R3, [R2] \n"
        "LDR R4, =tcb \n"
        "MOV R5, #4 \n"
        "MLA R1, R3, R5, R4 \n"
        "STR R1, [R0] \n"
        "LDR R2, [R1] \n"
        "LDMIA R2!, {R4-R11} \n"
        "MSR PSP, R2 \n"
        "MOV R0, #0xFFFFFFFD \n"
        "CPSIE I \n"
        "BX R0 \n"
    );
}

int main(void)
{
    Hardware_Init();

    RTOS_CreateTask(&tcb[0],
                    Task1_ReadSensor,
                    task1_stack,
                    STACK_SIZE);

    RTOS_CreateTask(&tcb[1],
                    Task2_UpdateDisplay,
                    task2_stack,
                    STACK_SIZE);

    currentTask = NULL;
    currentTaskIdx = 0;

    SCB_SHPR3 |= (0xFFU << 16);

    SYSTICK_LOAD = (SYS_CLOCK_HZ / TICK_RATE_HZ) - 1U;
    SYSTICK_VAL  = 0U;
    SYSTICK_CTRL = (1U << 2) |
                   (1U << 1) |
                   (1U << 0);

    __asm volatile("MOV R0, #0 \n MSR PSP, R0");

    while (1);
}
