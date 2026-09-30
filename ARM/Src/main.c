#include "stm32f411_min.h"
#include <stdint.h>
#include <stdio.h>

#define SENSOR_SELECT 1
#if SENSOR_SELECT == 1
#define SENSOR_ADDR 0x48U
#define SCL_PIN 6U
#define SDA_PIN 7U
#elif SENSOR_SELECT == 2
#define SENSOR_ADDR 0x48U
#define SCL_PIN 10U
#define SDA_PIN 3U
#elif SENSOR_SELECT == 3
#define SENSOR_ADDR 0x49U
#define SCL_PIN 10U
#define SDA_PIN 3U
#else
#error SENSOR_SELECT_must_be_1_2_or_3
#endif

static volatile uint8_t sample_due;
void TIM2_IRQHandler(void)
{
    if(TIM_SR&1U){TIM_SR&=~1U;sample_due=1U;}
}
static void clock_init(void)
{
    RCC_CR|=1U<<16; while(!(RCC_CR&(1U<<17))){}
    RCC_PLLCFGR=8U|(336U<<6)|(1U<<16)|(7U<<24)|(1U<<22);
    RCC_CR|=1U<<24; while(!(RCC_CR&(1U<<25))){}
    FLASH_ACR=0x702U;
    RCC_CFGR=(RCC_CFGR&~((3U)|(7U<<10)|(7U<<13)))|(2U)|(4U<<10);
    while(((RCC_CFGR>>2)&3U)!=2U){}
}
static void gpio_init(void)
{
    uint32_t mask=(3U<<(SCL_PIN*2U))|(3U<<(SDA_PIN*2U)),shift;
    RCC_AHB1ENR|=3U;
    GPIO_MODER(GPIOA_BASE)=(GPIO_MODER(GPIOA_BASE)&~((3U<<18)|(3U<<20)))|(2U<<18)|(2U<<20);
    GPIO_OSPEEDR(GPIOA_BASE)|=(3U<<18)|(3U<<20);
    GPIO_AFRH(GPIOA_BASE)=(GPIO_AFRH(GPIOA_BASE)&~((15U<<4)|(15U<<8)))|(7U<<4)|(7U<<8);
    GPIO_MODER(GPIOB_BASE)=(GPIO_MODER(GPIOB_BASE)&~mask)|(2U<<(SCL_PIN*2U))|(2U<<(SDA_PIN*2U));
    GPIO_OTYPER(GPIOB_BASE)|=(1U<<SCL_PIN)|(1U<<SDA_PIN);
    GPIO_OSPEEDR(GPIOB_BASE)|=mask;
    GPIO_PUPDR(GPIOB_BASE)=(GPIO_PUPDR(GPIOB_BASE)&~mask)|(1U<<(SCL_PIN*2U))|(1U<<(SDA_PIN*2U));
    shift=(SCL_PIN<8U)?SCL_PIN*4U:(SCL_PIN-8U)*4U;
    if(SCL_PIN<8U) GPIO_AFRL(GPIOB_BASE)=(GPIO_AFRL(GPIOB_BASE)&~(15U<<shift))|(4U<<shift);
    else GPIO_AFRH(GPIOB_BASE)=(GPIO_AFRH(GPIOB_BASE)&~(15U<<shift))|(4U<<shift);
    shift=(SDA_PIN<8U)?SDA_PIN*4U:(SDA_PIN-8U)*4U;
    if(SDA_PIN<8U) GPIO_AFRL(GPIOB_BASE)=(GPIO_AFRL(GPIOB_BASE)&~(15U<<shift))|(4U<<shift);
    else GPIO_AFRH(GPIOB_BASE)=(GPIO_AFRH(GPIOB_BASE)&~(15U<<shift))|(4U<<shift);
}
static void uart_init(void)
{
    RCC_APB2ENR|=1U<<4; USART_CR1=0U; USART_BRR=42000000U/115200U;
    USART_CR2=0U; USART_CR3=0U; USART_CR1=(1U<<13)|(1U<<3)|(1U<<2);
}
static void uart_write(const char *s)
{
    while(*s){while(!(USART_SR&(1U<<7))){} USART_DR=(uint8_t)*s++;}
    while(!(USART_SR&(1U<<6))){}
}
static void i2c_init(void)
{
    RCC_APB1ENR|=1U<<21; I2C_CR1=0U; I2C_CR2=42U; I2C_CCR=210U; I2C_TRISE=43U; I2C_CR1=1U;
}
static int wait_sr1(uint32_t mask)
{
    uint32_t n=1000000U;
    while(n--){if((I2C_SR1&mask)==mask)return 1;if(I2C_SR1&((1U<<8)|(1U<<9)|(1U<<10)))return 0;}
    return 0;
}
static int sensor_read(int16_t *q4)
{
    uint8_t b0,b1; uint16_t v;
    I2C_CR1|=1U<<8;
    if(!wait_sr1(1U))goto fail;
    I2C_DR=(uint8_t)(SENSOR_ADDR<<1);
    if(!wait_sr1(1U<<1))goto fail;
    (void)I2C_SR1;(void)I2C_SR2;
    I2C_CR1|=1U<<8;
    if(!wait_sr1(1U))goto fail;
    I2C_DR=(uint8_t)((SENSOR_ADDR<<1)|1U);
    I2C_CR1|=1U<<10;
    if(!wait_sr1(1U<<1))goto fail;
    (void)I2C_SR1;(void)I2C_SR2;
    if(!wait_sr1(1U<<6))goto fail;
    I2C_CR1&=~(1U<<10); I2C_CR1|=1U<<9;
    b0=(uint8_t)I2C_DR;
    while(!(I2C_SR1&(1U<<6))){}
    b1=(uint8_t)I2C_DR; I2C_CR1|=1U<<10;
    v=(uint16_t)(((uint16_t)b0<<8)|b1); v>>=4;
    if(v&0x0800U)v|=0xF000U;
    *q4=(int16_t)v; return 1;
fail:
    I2C_CR1|=1U<<9; I2C_CR1|=1U<<10; return 0;
}
static void timer_init(void)
{
    RCC_APB1ENR|=1U; TIM_CR1=0U; TIM_PSC=8399U; TIM_ARR=9999U;
    TIM_EGR=1U; TIM_SR=0U; TIM_DIER=1U; NVIC_ISER0=1U<<28; TIM_CR1=1U;
}
int main(void)
{
    int16_t q4; int32_t scaled,whole,frac; char line[40];
    clock_init();gpio_init();uart_init();i2c_init();timer_init();
    __enable_irq();
    for(;;){
        if(sample_due){
            sample_due=0U;
            if(sensor_read(&q4)){
                scaled=(int32_t)q4*625; whole=scaled/10000; frac=scaled%10000;
                if(frac<0)frac=-frac;
                (void)snprintf(line,sizeof(line),"T=%+ld.%04ld C\r\n",(long)whole,(long)frac);
                uart_write(line);
            }else uart_write("ERR=I2C\r\n");
        }
        //cpu_wfi();
        __wfi();
    }
}
