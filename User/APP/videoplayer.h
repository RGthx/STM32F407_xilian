/**
 ****************************************************************************************************
 * @file        videoplay.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2021-11-16
 * @brief       视频播放器 应用代码
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 STM32开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 * 修改说明
 * V1.0 20211116
 * 第一次发布
 *
 ****************************************************************************************************
 */
#ifndef __VIDEOPLAYER_H
#define __VIDEOPLAYER_H 
#include <stm32f4xx.h> 
#include "./FATFS/source/ff.h"

// 在文件顶部添加必要的头文件包含
#include "./MALLOC/malloc.h"
#include "./SYSTEM/usart/usart.h"
#include "./BSP/ES8388/es8388.h"
#include "./BSP/I2S/i2s.h"
#include "./BSP/LED/led.h"
#include "./BSP/LCD/lcd.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/KEY/key.h"
#include "./FATFS/exfuns/exfuns.h"
#include "./TEXT/text.h"
#include "./FATFS/source/ffconf.h"
#include "./FATFS/exfuns/fattester.h"
#include "./MJPEG/mjpeg.h"
#include "./MJPEG/avi.h"
#include "./BSP/TIMER/btim.h"
#include "string.h"
#include "stdlib.h"


#define AVI_AUDIO_BUF_SIZE    1024*5        /* 定义avi解码时,音频buf大小. */
#define AVI_VIDEO_BUF_SIZE    1024*60       /* 定义avi解码时,视频buf大小. */
// 添加文本覆盖显示相关变量和函数声明
#define MAX_TEXT_LINES 200
#define DISPLAY_LINES 20

extern char* text_lines[MAX_TEXT_LINES];
extern uint16_t total_lines;
extern uint16_t display_start_line;

void load_text_file(void);
void display_text_overlay(void);
void init_text_display(void);


uint16_t video_get_tnum(char *path);                                /* 得到path路径下，目标文件的总数 */
void video_time_show(FIL *favi,AVI_INFO *aviinfo);                  /* 显示当前播放时间 */
void video_info_show(AVI_INFO *aviinfo);                            /* 显示当前视频文件的相关信息 */
void video_bmsg_show(uint8_t* name, uint16_t index, uint16_t total);/* 视频基本信息显示 */
void video_play(void);                                              /* 播放视频 */
uint8_t video_play_mjpeg(uint8_t *pname);                           /* 播放一个mjpeg文件 */
uint8_t video_seek(FIL *favi,AVI_INFO *aviinfo,uint8_t *mbuf);      /* AVI文件查找 */




// 在包含头文件后添加宏定义
#define TEXT_BUFFER_SIZE 512
#define MAX_DISPLAY_LINES 20
#define MAX_TEXT_LINES 200

// 添加函数声明
void init_text_stream(void);
void load_text_file_stream(void);
void display_text_overlay_stream(void);
FRESULT read_next_char(char* ch);
typedef struct {
    FIL file;
    char buffer[TEXT_BUFFER_SIZE];
    UINT buffer_pos;
    UINT buffer_len;
    UINT file_pos;
    UINT file_size;
    char line_buffer[200];
    uint16_t line_pos;
    uint16_t current_line;
    uint16_t total_lines;
    char* display_lines[MAX_DISPLAY_LINES];
    uint16_t display_start;
} text_stream_t;

static text_stream_t text_stream;

#endif



