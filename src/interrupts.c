#include "stm32f446xx.h"
#include "main.h"

void TIM3_IRQHandler(void) {
    /* Clear the interrupt flag right away.
     * Due to pipelining, the register itself might not get updated for several
     * cycles. If we wait until the end of the ISR to clear the flag,
     * it can trigger again immediately */
    TIM3->SR &= ~TIM_SR_UIF_Msk;

    if(bit == 0) { // the first 64 bits of a new row have been latched. Set the row select to match.
        GPIOB->ODR = (GPIOB->ODR & ~(row_mask)) | row;
        row++;
        row &= row_mask;
    }
    bit++;
    bit &=  0x7;
    TIM3->CCR2 = 640 - (BRIGHTNESS * (1 << bit)); // set the duty cycle of the NEXT ~OE pulse
}

void DMA2_Stream3_IRQHandler(void) {
    DMA2->LIFCR |= DMA_LIFCR_CTCIF3; // make sure the interrupt flag is clear
    frame_count++;
    busyFlag = 0; // main loop watches this flag to know when to fill up the next buffer
}

