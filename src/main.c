#include "main.h"

#include <stdint.h>

#include "stm32f411xe.h"

#include "systick.h"
#include "led.h"

volatile uint8_t bit;
volatile uint8_t row;
volatile uint32_t frame_count;

volatile uint8_t busyFlag;

uint8_t *nextBuffer;

static void init(void);
static void initClock(void);
static void initGPIO(void);
static void initTimers(void);
static void initDMA(void);

int main(void) {
    init();
    uint32_t current_buffer, start_time;
    while(1) {
        start_time = millis();
        while(busyFlag);
        busyFlag = 1;
        current_buffer = DMA2_Stream3->CR | DMA_SxCR_CT;
        nextBuffer = current_buffer ? buffer2 : buffer1;
        while(busyFlag);
        busyFlag = 1;
        LED_fillBuffer(frame, nextBuffer);
        LED_waveEffect(frame);
        while(millis() - start_time < 30);

    }
    return 0;
}

static void init(void) {
    // enable the data and instruction cache and prefetch
    FLASH->ACR |= FLASH_ACR_DCEN | FLASH_ACR_ICEN | FLASH_ACR_PRFTEN;
    
    // This will set the clock to 180MHz
    initClock();
    SysTick_Init();

    // Set up any input/output pins
    initGPIO();
    initDMA();
    DBGMCU->APB2FZ |= DBGMCU_APB2_FZ_DBG_TIM1_STOP;
    DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM3_STOP;
    initTimers();
    busyFlag = 1;
}

static void initGPIO(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    
    GPIOA->MODER |= (0x1U << GPIO_MODER_MODE5_Pos);
    GPIOA->OTYPER |= (0x0U << GPIO_OTYPER_OT5_Pos);
    GPIOA->OSPEEDR |= (0x0U << GPIO_OSPEEDR_OSPEED5_Pos);
    GPIOA->PUPDR |= (0x0U << GPIO_PUPDR_PUPD5_Pos);
    
    // PB0-4 are A,B,C,D,E pins
    GPIOB->MODER &= ~(GPIO_MODER_MODE0 | GPIO_MODER_MODE1 | GPIO_MODER_MODE2 | GPIO_MODER_MODE3 | GPIO_MODER_MODE4); 
    GPIOB->MODER |= GPIO_MODER_MODE0_0 |
                    GPIO_MODER_MODE1_0 |
                    GPIO_MODER_MODE2_0 |
                    GPIO_MODER_MODE3_0 |
                    GPIO_MODER_MODE4_0;

    GPIOB->OSPEEDR |= GPIO_OSPEEDR_OSPEED0_0 | GPIO_OSPEEDR_OSPEED0_1 |
                      GPIO_OSPEEDR_OSPEED1_0 | GPIO_OSPEEDR_OSPEED1_1 |
                      GPIO_OSPEEDR_OSPEED2_0 | GPIO_OSPEEDR_OSPEED2_1 |
                      GPIO_OSPEEDR_OSPEED3_0 | GPIO_OSPEEDR_OSPEED3_1 |
                      GPIO_OSPEEDR_OSPEED4_0 | GPIO_OSPEEDR_OSPEED4_1; 
 
    // PC0-PC5 are RGB pins
    GPIOC->MODER |= GPIO_MODER_MODE0_0 |
                    GPIO_MODER_MODE1_0 |
                    GPIO_MODER_MODE2_0 |
                    GPIO_MODER_MODE3_0 |
                    GPIO_MODER_MODE4_0 |
                    GPIO_MODER_MODE5_0;

    GPIOC->OSPEEDR |= GPIO_OSPEEDR_OSPEED0_0 | GPIO_OSPEEDR_OSPEED0_1 |
                      GPIO_OSPEEDR_OSPEED1_0 | GPIO_OSPEEDR_OSPEED1_1 |
                      GPIO_OSPEEDR_OSPEED2_0 | GPIO_OSPEEDR_OSPEED2_1 |
                      GPIO_OSPEEDR_OSPEED3_0 | GPIO_OSPEEDR_OSPEED3_1 |
                      GPIO_OSPEEDR_OSPEED4_0 | GPIO_OSPEEDR_OSPEED4_1 |
                      GPIO_OSPEEDR_OSPEED5_0 | GPIO_OSPEEDR_OSPEED5_1; 
}

static void initDMA(void) {
    /*
     * Channel 6
     * Double Buffer
     * Very high priority
     * Memory Inc
     * 1byte-1byte transfers
     * Memory to Peripheral
     */

    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;

    DMA2_Stream3->CR |= (6<<DMA_SxCR_CHSEL_Pos) | DMA_SxCR_DBM | DMA_SxCR_PL_0 | DMA_SxCR_PL_1 | DMA_SxCR_MINC | DMA_SxCR_DIR_0;
    DMA2_Stream3->NDTR = 0x4000; // 64 col, 8 Bits per colour channel, 32 rows
    //DMA1_Stream4->PAR = 0;
    DMA2_Stream3->PAR = (uint32_t)&(GPIOC->ODR);
    DMA2_Stream3->M0AR = (uint32_t)buffer1;
    DMA2_Stream3->M1AR = (uint32_t)buffer2;
    DMA2_Stream3->FCR |= DMA_SxFCR_DMDIS; // turn on the FIFO
    //DMA2->LIFCR |= DMA_LIFCR_CTEIF3; // make sure the interrupt flag is clear
    DMA2->LIFCR |= DMA_LIFCR_CTCIF3; // make sure the interrupt flag is clear
    DMA2_Stream3->CR |= DMA_SxCR_TCIE; // enable the transfer complete interrupt
    //DMA2_Stream3->CR |= DMA_SxCR_TEIE; // enable the transfer complete interrupt

    NVIC_EnableIRQ(DMA2_Stream3_IRQn);

    DMA2_Stream3->CR |= DMA_SxCR_EN; // enable the stream
}

static void initClock(void) {
    // 1. Enable clock to PWR in APB1
    // 2. Set up the HSE and PLL
    // 3. Enable overdrive and wait for it to be enabled
    // 4. Adjust the flash latency
    // 5. Set AHB/APB1/APB2 prescalers
    // 6. Wait for PLL lock
    // 7. Select PLL as sysclock
    // 8. Call SystemCoreClockUpdate();

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;

    // Enable the HSE in bypass mode (there is a 8MHz signal coming from the ST-LINK)
    RCC->CR |= RCC_CR_HSEON;
    while(!(RCC->CR & RCC_CR_HSERDY));

    // Set the main PLL M, N, P, Q, R, and HSE as the input
    // M = 4, N = 96, P = 2 = 96MHz SYSCLK
    RCC->PLLCFGR = RCC_PLLCFGR_PLLM_2 | (96 << RCC_PLLCFGR_PLLN_Pos) | (0 << RCC_PLLCFGR_PLLP_Pos) |  RCC_PLLCFGR_PLLSRC_HSE;
    RCC->CR |= RCC_CR_PLLON;

    // Set the flash wait time
    FLASH->ACR |= FLASH_ACR_LATENCY_5WS;
    if((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_5WS) {
        _error_handler();
    }

    // set APB prescalers
    // APB1 = 2, APB2 = 1
    // 48MHz and 96Mhz
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV1;
    
    // wait for PLL lock
    while(!(RCC->CR & RCC_CR_PLLRDY));

    // Switch SYSCLK to PLL
    RCC->CFGR |= RCC_CFGR_SW_PLL;
}

static void initTimers(void) {
    // Timer 1, running 9Mhz 50% duty cycle
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    
    GPIOA->MODER |= GPIO_MODER_MODE8_1; // PA8 in AF mode
    GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEED8_0 | GPIO_OSPEEDR_OSPEED8_1; // high speed
    GPIOA->AFR[1] |= (0x1U << GPIO_AFRH_AFSEL8_Pos); // AF1 for PA8 is TIM1_CH1

    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    
    TIM1->ARR = 9;
    TIM1->CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2; //PWM mode 1
    TIM1->DIER |= TIM_DIER_CC1DE; // DMA request on CH1 (falling edge of clock)
    TIM1->CCR1 = 5;
    TIM1->CCER |= TIM_CCER_CC1E; // OC1 enabled

    TIM1->BDTR |= TIM_BDTR_MOE; // Main output enabled.. if this isn't set, there is no output on the pins!

    TIM1->PSC = PRESCALE; // scale down for testing
    TIM1->EGR |= TIM_EGR_UG; // trigger a UEV to update the preload, auto-reload, and capture-compare shadow registers
    TIM1->SR = 0;
    TIM1->CR2 |= TIM_CR2_MMS_1; // Master mode - update

    // Timer 3
    // LAT on CH1, output to PC6
    // OE on CH2, output to PC7
    GPIOC->MODER |= GPIO_MODER_MODE6_1; // PC6 in AF mode
    GPIOC->OSPEEDR |= GPIO_OSPEEDR_OSPEED6_0 | GPIO_OSPEEDR_OSPEED6_1; // high speed
    GPIOC->AFR[0] |= (0x2U << GPIO_AFRL_AFSEL6_Pos); // AF2 for PC6 is TIM3_CH1

    GPIOC->MODER |= GPIO_MODER_MODE7_1; // PC7 in AF mode
    GPIOC->OSPEEDR |= GPIO_OSPEEDR_OSPEED7_0 | GPIO_OSPEEDR_OSPEED1_1; // high speed
    GPIOC->AFR[0] |= (0x2U << GPIO_AFRL_AFSEL7_Pos); // AF2 for PC7 is TIM3_CH2

    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    
    //TIM5->SMCR |= TIM_SMCR_TS_0; // select ITR0 as the trigger input (for TIM5, this is TIM3)
    TIM3->SMCR |= TIM_SMCR_SMS_2 | TIM_SMCR_SMS_1; // slave mode set to trigger mode
    TIM3->ARR = 639;
    TIM3->CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2; //PWM mode 1
    TIM3->CCR1 = 12;
    TIM3->CCER |= TIM_CCER_CC1E; // OC1 enabled

    TIM3->CCMR1 |= TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2; //PWM mode 1
    TIM3->CCMR1 |= TIM_CCMR1_OC2PE; // preload enabled for CCR2
    TIM3->CCR2 = 640; // start with OC2 (OE) held at 1 (off)
    TIM3->CCER |= TIM_CCER_CC2E; // OC2 enabled

    TIM3->PSC = PRESCALE; // scale down for testing
    TIM3->EGR |= TIM_EGR_UG; // trigger a UEV to update the preload, auto-reload, and capture-compare shadow registers
    TIM3->SR = 0; // clear the status flags
    TIM3->DIER |= TIM_DIER_UIE; // enable TIM5 interrupt on UEV
    TIM3->CNT = 15;

    NVIC_EnableIRQ(TIM3_IRQn);

    // Start the timer
    TIM1->CR1 |= TIM_CR1_CEN;
    TIM3->CCR2 = 640 - BRIGHTNESS; // put the first value in CCR2 preload it will be loaded at the first UEV
}


void _error_handler(void) {
    while(1);
}

