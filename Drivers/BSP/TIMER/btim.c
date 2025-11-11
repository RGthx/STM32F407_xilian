/**
 ****************************************************************************************************
 * @file        btim.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.1
 * @date        2021-11-29
 * @brief       基本定时器 驱动代码
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 探索者 F407开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 * 修改说明
 * V1.0 20211015
 * 第一次发布
 * V1.1 20211129
 * 添加定时器7初始化函数与中断函数
 ****************************************************************************************************
 */

#include "./BSP/TIMER/btim.h"
#include "./SYSTEM/usart/usart.h"
#include "./BSP/LED/led.h"


TIM_HandleTypeDef g_tim6_handler;         /* 定时器参数句柄 */
TIM_HandleTypeDef g_tim7_handler;         /* 定时器参数句柄 */

uint16_t frame;
__IO uint8_t frameup;

/**
 * @brief       基本定时器TIM6定时中断初始化函数
 * @note
 *              基本定时器的时钟来自APB1,当PPRE1 ≥ 2分频的时候
 *              基本定时器的时钟为APB1时钟的2倍, 而APB1为42M, 所以定时器时钟 = 84Mhz
 *              定时器溢出时间计算方法: Tout = ((arr + 1) * (psc + 1)) / Ft us.
 *              Ft=定时器工作频率,单位:Mhz
 *
 * @param       arr : 自动重装值。
 * @param       psc : 时钟预分频数
 * @retval      无
 */
void btim_tim6_int_init(uint16_t arr, uint16_t psc)
{
    g_tim6_handler.Instance = BTIM_TIM6_INT;                /* 通用定时器X */
    g_tim6_handler.Init.Prescaler = psc;                    /* 设置预分频器  */
    g_tim6_handler.Init.CounterMode = TIM_COUNTERMODE_UP;   /* 向上计数器 */
    g_tim6_handler.Init.Period = arr;                       /* 自动装载值 */
    HAL_TIM_Base_Init(&g_tim6_handler);
    
    HAL_TIM_Base_Start_IT(&g_tim6_handler);                 /* 使能通用定时器x和及其更新中断：TIM_IT_UPDATE */
}

/**
 * @brief       基本定时器TIM7定时中断初始化函数
 * @note
 *              基本定时器的时钟来自APB1,当PPRE1 ≥ 2分频的时候
 *              基本定时器的时钟为APB1时钟的2倍, 而APB1为42M, 所以定时器时钟 = 84Mhz
 *              定时器溢出时间计算方法: Tout = ((arr + 1) * (psc + 1)) / Ft us.
 *              Ft=定时器工作频率,单位:Mhz
 *
 * @param       arr : 自动重装值。
 * @param       psc : 时钟预分频数
 * @retval      无
 */
void btim_tim7_int_init(uint16_t arr, uint16_t psc)
{
    BTIM_TIM7_INT_CLK_ENABLE();                             /* 使能TIM时钟 */

    g_tim7_handler.Instance = BTIM_TIM7_INT;                /* 通用定时器X */
    g_tim7_handler.Init.Prescaler = psc;                    /* 设置预分频器  */
    g_tim7_handler.Init.CounterMode = TIM_COUNTERMODE_UP;   /* 向上计数器 */
    g_tim7_handler.Init.Period = arr;                       /* 自动装载值 */
    HAL_TIM_Base_Init(&g_tim7_handler);
    
    HAL_NVIC_SetPriority(BTIM_TIM7_INT_IRQn, 1, 3);         /* 抢占1，子优先级3，组2 */
    HAL_NVIC_EnableIRQ(BTIM_TIM7_INT_IRQn);                 /* 开启ITM3中断 */

    HAL_TIM_Base_Start_IT(&g_tim7_handler);                 /* 使能通用定时器x和及其更新中断：TIM_IT_UPDATE */
}

/**
 * @brief       定时器底层驱动，开启时钟，设置中断优先级
                此函数会被HAL_TIM_Base_Init()函数调用
 * @param       无
 * @retval      无
 */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == BTIM_TIM6_INT)
    {
        BTIM_TIM6_INT_CLK_ENABLE();                     /* 使能TIM时钟 */
        HAL_NVIC_SetPriority(BTIM_TIM6_INT_IRQn, 1, 3); /* 抢占1，子优先级3，组2 */
        HAL_NVIC_EnableIRQ(BTIM_TIM6_INT_IRQn);         /* 开启ITM6中断 */
    }
}


/**
 * @brief       基本定时器TIMX中断服务函数
 * @param       无
 * @retval      无
 */
void BTIM_TIM6_INT_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&g_tim6_handler);  /* 定时器回调函数 */
}

void BTIM_TIM7_INT_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&g_tim7_handler);  /* 定时器回调函数 */
}

/**
 * @brief       回调函数，定时器中断服务函数调用
 * @param       无
 * @retval      无
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == (&g_tim6_handler))              /* 定时器3 */
    {
        //printf("video frame:%d\r\n", frame);    /* 打印帧率 */
        frame = 0;
        LED0_TOGGLE();
    }
    if(htim==(&g_tim7_handler))                 /* 定时器6 */
    {
        frameup = 1;
        LED1_TOGGLE();
    }
}

