
#include "gpio.h"
#include "timer.h"
#include "uart.h"



ButtonState GetGpioState(uint16_t gpio_state)
{
    static ButtonState buttonState = BUTTON_IDLE;
    static uint32_t lastPressTime = 0;
    switch (buttonState)
    {
        case BUTTON_IDLE:
            if(gpio_state == GPIO_BUTTON_PRESSED) 
            {
                buttonState = BUTTON_DEBOUNCED; 
            }else if(gpio_state == GPIO_BUTTON_RELEASED)
            {
                buttonState = BUTTON_IDLE; 
            }
            break;
        case BUTTON_DEBOUNCED:
            if(gpio_state == GPIO_BUTTON_PRESSED) 
            {
                lastPressTime = GetSysTick(); 
                buttonState = BUTTON_PRESSED; 
            }else if(gpio_state == GPIO_BUTTON_RELEASED)
            {
                buttonState = BUTTON_IDLE; 
            }
            break;
        case BUTTON_PRESSED:
            if(gpio_state == GPIO_BUTTON_RELEASED) 
            {
                buttonState = BUTTON_DEBOUNCED; 
            }else if(gpio_state == GPIO_BUTTON_PRESSED)
            {
                if(GetSysTick() < lastPressTime)  /*溢出*/
                {
                    lastPressTime = (UINT32_PORT_MAX - lastPressTime) ;
                    if(GetSysTick() + lastPressTime > GPIO_LONG_PRESS_TIME) 
                    {
                        buttonState = BUTTON_LONG_PRESSED; 
                    }
                }else
                {
                    if (GetSysTick() - lastPressTime > GPIO_LONG_PRESS_TIME) 
                    {
                        buttonState = BUTTON_LONG_PRESSED; 
                    }
                }
            }
            break;
        case BUTTON_LONG_PRESSED:
            if(gpio_state == GPIO_BUTTON_RELEASED) 
            {
                buttonState = BUTTON_IDLE; 
            }
            break;
        default:
            break;
    }
    return buttonState;
}


void GpioTask(void)
{
    #define TEST_GPIO     P15
    ButtonState buttonState = GetGpioState(GPIO_STATE(TEST_GPIO));
    
    if (buttonState == BUTTON_PRESSED) 
    {
        UartSendData(&Uart2, (uint8_t *)"Button Pressed\r\n", sizeof("Button Pressed\r\n") - 1);
    }else if(buttonState == BUTTON_LONG_PRESSED) 
    {
        UartSendData(&Uart2, (uint8_t *)"Button Long Pressed\r\n", sizeof("Button Long Pressed\r\n") - 1);  
    }else if(buttonState == BUTTON_IDLE) 
    {
        UartSendData(&Uart2, (uint8_t *)"Button Idle\r\n", sizeof("Button Idle\r\n") - 1);
    }
}