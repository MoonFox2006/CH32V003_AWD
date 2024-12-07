#include "debug.h"

//#define MIN_POWER

#define AWD_CHANNEL 7
#define AWD_MINIMUM 100
#define AWD_MAXIMUM 200

#if AWD_CHANNEL <= 1
#define AWD_RCC     RCC_APB2Periph_GPIOA
#define AWD_GPIO    GPIOA
#if AWD_CHANNEL == 0
#define AWD_PIN     GPIO_Pin_2
#else // AWD_CHANNEL == 1
#define AWD_PIN     GPIO_Pin_1
#endif
#elif AWD_CHANNEL == 2
#define AWD_RCC     RCC_APB2Periph_GPIOC
#define AWD_GPIO    GPIOC
#define AWD_PIN     GPIO_Pin_4
#else // AWD_CHANNEL >= 3
#define AWD_RCC     RCC_APB2Periph_GPIOD
#define AWD_GPIO    GPIOD
#if AWD_CHANNEL == 3
#define AWD_PIN     GPIO_Pin_2
#elif AWD_CHANNEL == 4
#define AWD_PIN     GPIO_Pin_3
#elif AWD_CHANNEL == 5
#define AWD_PIN     GPIO_Pin_5
#elif AWD_CHANNEL == 6
#define AWD_PIN     GPIO_Pin_6
#else // AWD_CHANNEL == 7
#define AWD_PIN     GPIO_Pin_4
#endif
#endif

#define LED_RCC     RCC_APB2Periph_GPIOC
#define LED_GPIO    GPIOC
#define LED_PIN     GPIO_Pin_0
#define LED_LEVEL   0

static void AWD_Init(void) {
    const GPIO_InitTypeDef gpio_cfg = {
        .GPIO_Pin = AWD_PIN,
        .GPIO_Mode = GPIO_Mode_AIN
    };
    const ADC_InitTypeDef adc_cfg = {
        .ADC_Mode = ADC_Mode_Independent,
        .ADC_ScanConvMode = DISABLE,
        .ADC_ContinuousConvMode = ENABLE,
        .ADC_ExternalTrigConv = ADC_ExternalTrigConv_None,
        .ADC_DataAlign = ADC_DataAlign_Right,
        .ADC_NbrOfChannel = 1
    };
    const NVIC_InitTypeDef nvic_cfg = {
        .NVIC_IRQChannel = ADC_IRQn,
        .NVIC_IRQChannelPreemptionPriority = 0,
        .NVIC_IRQChannelSubPriority = 1,
        .NVIC_IRQChannelCmd = ENABLE
    };

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | AWD_RCC, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div8);

    GPIO_Init(AWD_GPIO, (GPIO_InitTypeDef*)&gpio_cfg);

    ADC_DeInit(ADC1);
    ADC_Init(ADC1, (ADC_InitTypeDef*)&adc_cfg);
    
    ADC_RegularChannelConfig(ADC1, AWD_CHANNEL, 1, ADC_SampleTime_241Cycles);

    ADC_Calibration_Vol(ADC1, ADC_CALVOL_50PERCENT);
    ADC_Cmd(ADC1, ENABLE);

    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1)) {}
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1)) {}

    ADC_AnalogWatchdogThresholdsConfig(ADC1, 1023, AWD_MINIMUM);
    ADC_AnalogWatchdogSingleChannelConfig(ADC1, AWD_CHANNEL);
    ADC_AnalogWatchdogCmd(ADC1, ADC_AnalogWatchdog_SingleRegEnable);

    NVIC_Init((NVIC_InitTypeDef*)&nvic_cfg);

    ADC_ClearITPendingBit(ADC1, ADC_IT_AWD);
    ADC_ITConfig(ADC1, ADC_IT_AWD, ENABLE);

    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

static void LED_Init(void) {
    const GPIO_InitTypeDef gpio_cfg = {
        .GPIO_Pin = LED_PIN,
        .GPIO_Speed = GPIO_Speed_2MHz,
        .GPIO_Mode = GPIO_Mode_Out_PP
    };

    RCC_APB2PeriphClockCmd(LED_RCC, ENABLE);

    GPIO_Init(LED_GPIO, (GPIO_InitTypeDef*)&gpio_cfg);
    GPIO_WriteBit(LED_GPIO, LED_PIN, ! LED_LEVEL);
}

int main(void) {
    SystemCoreClockUpdate();

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);

#ifndef MIN_POWER
    Delay_Init();
    USART_Printf_Init(115200);
#endif

    LED_Init();
    AWD_Init();

    while (1) {
#ifdef MIN_POWER
        __WFI();
#else
        Delay_Ms(50);
        printf("\r%4u", ADC_GetConversionValue(ADC1));
#endif
    }
}

void __attribute__((interrupt("WCH-Interrupt-fast"))) ADC1_IRQHandler(void) {
    if (ADC_GetITStatus(ADC1, ADC_IT_AWD)) {
        if (GPIO_ReadOutputDataBit(LED_GPIO, LED_PIN) == LED_LEVEL) { // Must be turns off
            GPIO_WriteBit(LED_GPIO, LED_PIN, ! LED_LEVEL);
            ADC_AnalogWatchdogThresholdsConfig(ADC1, 1023, AWD_MINIMUM);
        } else {
            GPIO_WriteBit(LED_GPIO, LED_PIN, LED_LEVEL);
            ADC_AnalogWatchdogThresholdsConfig(ADC1, AWD_MAXIMUM, 0);
        }
        ADC_ClearITPendingBit(ADC1, ADC_IT_AWD);
    }
}
