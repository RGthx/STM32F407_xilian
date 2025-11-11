/**
 ****************************************************************************************************
 * @file        videoplay.c
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
#include "./APP/videoplayer.h"
#include "./FATFS/source/ff.h"
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

#define TEXT_BUFFER_SIZE 512
#define MAX_DISPLAY_LINES 20

// 在文件顶部修改滚动控制变量
static uint32_t last_scroll_time = 0;
#define SCROLL_INTERVAL 1000 // 每1秒滚动一次（单位：毫秒）

void init_text_stream(void)
{
    // 初始化结构体
    memset(&text_stream, 0, sizeof(text_stream_t));
    
    // 释放之前的显示行内存
    for(int i = 0; i < MAX_DISPLAY_LINES; i++)
    {
        if(text_stream.display_lines[i])
        {
            myfree(SRAMIN, text_stream.display_lines[i]);
            text_stream.display_lines[i] = NULL;
        }
    }
}


// 文本显示相关变量
char* text_lines[MAX_TEXT_LINES];
uint16_t total_lines = 0;
uint16_t display_start_line = 0;


extern uint16_t frame;          /* 播放帧率 */
extern __IO uint8_t frameup;    /* 视频播放时隙控制变量,当等于1的时候,可以更新下一帧视频 */

volatile uint8_t i2splaybuf;    /* 即将播放的音频帧缓冲编号 */
uint8_t *i2sbuf[4];             /* 音频缓冲帧,共4帧,4*5K=20K */

/**
 * @brief       音频数据I2S DMA传输回调函数
 * @param       无
 * @retval      无
 */
void audio_i2s_dma_callback(void)
{
    i2splaybuf++;

    if (i2splaybuf > 3)
    {
        i2splaybuf = 0;
    }

    if (DMA1_Stream4->CR & (1 << 19))
    {
        DMA1_Stream4->M0AR = (uint32_t)i2sbuf[i2splaybuf];  /* 指向下一个buf */
    }
    else
    {
        DMA1_Stream4->M1AR = (uint32_t)i2sbuf[i2splaybuf];  /* 指向下一个buf */
    }
}

/**
 * @brief       得到path路径下,目标文件的总个数
 * @param       path:路径
 * @retval      返回值:总有效文件数
 */
uint16_t video_get_tnum(char *path)
{
    uint8_t res;
    uint16_t rval = 0;
    
    DIR tdir;           /* 临时目录 */
    FILINFO *tfileinfo; /* 临时文件信息 */
    
    tfileinfo = (FILINFO *)mymalloc(SRAMIN, sizeof(FILINFO));   /* 申请内存 */
    res = f_opendir(&tdir, (const TCHAR *)path);    /* 打开目录 */
    
    if (res == FR_OK && tfileinfo)
    {
        while (1)       /* 查询总的有效文件数 */
        {
            res = f_readdir(&tdir, tfileinfo);                  /* 读取目录下的一个文件 */
            if (res != FR_OK || tfileinfo->fname[0] == 0)
            {
                break; /* 错误了/到末尾了,退出 */
            }

            res = exfuns_file_type(tfileinfo->fname);
            if ((res & 0xF0) == 0x60)   /* 取高四位,看看是不是视频文件 */
            {
                rval++;                 /* 有效文件数增加1 */
            }
        }
    }

    myfree(SRAMIN, tfileinfo);          /* 释放内存 */
    
    return rval;
}

/**
 * @brief       显示当前播放时间
 * @param       favi    : 当前播放的视频文件
 * @param       aviinfo : avi控制结构体
 * @retval      无
 */
void video_time_show(FIL *favi, AVI_INFO *aviinfo)
{
    /*
    static uint32_t oldsec; /* 上一次的播放时间 
    
    uint8_t *buf;
    
    uint32_t totsec = 0;    /* video文件总时间 
    uint32_t cursec;        /* 当前播放时间 
    
    totsec = (aviinfo->SecPerFrame / 1000) * aviinfo->TotalFrame;   /* 歌曲总长度(单位:ms) 
    totsec /= 1000;         /* 秒钟数. 
    cursec = ((double)favi->fptr / favi->obj.objsize) * totsec;     /* 获取当前播放到多少秒 
    
    if (oldsec != cursec)   /* 需要更新显示时间 
    {
        buf = mymalloc(SRAMIN, 100); /* 申请100字节内存 
        oldsec = cursec;
        
        sprintf((char *)buf, "播放时间:%02d:%02d:%02d/%02d:%02d:%02d", cursec / 3600, (cursec % 3600) / 60, cursec % 60, totsec / 3600, (totsec % 3600) / 60, totsec % 60);
        text_show_string(10, 90, lcddev.width - 10, 16, (char *)buf, 16, 0, RED);   /* 显示歌曲名字 
        
        myfree(SRAMIN, buf);
        
    }
        */
}

/**
 * @brief       显示当前视频文件的相关信息
 * @param       aviinfo : avi控制结构体
 * @retval      无
 */
void video_info_show(AVI_INFO *aviinfo)
{
    /*
    uint8_t *buf;
    
    buf = mymalloc(SRAMIN, 100);    //申请100字节内存
    
    sprintf((char *)buf, "声道数:%d,采样率:%d", aviinfo->Channels, aviinfo->SampleRate * 10);
    text_show_string(10, 50, lcddev.width - 10, 16, (char *)buf, 16, 0, RED);   //显示歌曲名字
    
    sprintf((char *)buf, "帧率:%d帧", 1000 / (aviinfo->SecPerFrame / 1000));
    text_show_string(10, 70, lcddev.width - 10, 16, (char *)buf, 16, 0, RED);   //显示歌曲名字
    
    myfree(SRAMIN, buf);            // 释放内存 

    */
}

/**
 * @brief       视频基本信息显示
 * @param       name    : 视频名字
 * @param       index   : 当前索引
 * @param       total   : 总文件数
 * @retval      无
 */
void video_bmsg_show(uint8_t *name, uint16_t index, uint16_t total)
{
    /*
    uint8_t *buf;
    
    buf = mymalloc(SRAMIN, 100);    /* 申请100字节内存 
    
    sprintf((char *)buf, "文件名:%s", name);
    text_show_string(10, 10, lcddev.width - 10, 16, (char *)buf, 16, 0, RED);   /* 显示文件名 
    
    sprintf((char *)buf, "索引:%d/%d", index, total);
    text_show_string(10, 30, lcddev.width - 10, 16, (char *)buf, 16, 0, RED);   /* 显示索引 
    
    myfree(SRAMIN, buf);            释放内存 */
}

/**
 * @brief       播放视频
 * @param       无
 * @retval      无
 */
void video_play(void)
{
    uint8_t res;
    
    DIR vdir;               /* 目录 */
    FILINFO *vfileinfo;     /* 文件信息 */
    uint8_t *pname;         /* 带路径的文件名 */
    
    uint16_t totavinum;     /* 视频文件总数 */
    uint16_t curindex;      /* 视频文件当前索引 */
    uint32_t *voffsettbl;   /* 视频文件off block偏移表 */

    uint32_t temp;
    
    while (f_opendir(&vdir, "0:/VIDEO"))    /* 打开视频文件夹 */
    {
        text_show_string(60, 190, 240, 16, "VIDEO文件夹不存在!", 16, 0, RED);
        delay_ms(200);
        lcd_fill(60, 190, 240, 206, WHITE); /* 清除显示 */
        delay_ms(200);
    }

    totavinum = video_get_tnum("0:/VIDEO"); /* 得到所有有效文件数 */
    while (totavinum == NULL)               /* 视频文件个数为0 */
    {
        text_show_string(60, 190, 240, 16, "没有视频文件!", 16, 0, RED);
        delay_ms(200);
        lcd_fill(60, 190, 240, 146, WHITE); /* 清除显示 */
        delay_ms(200);
    }

    vfileinfo = (FILINFO *)mymalloc(SRAMIN, sizeof(FILINFO));   /* 为文件信息结构体分配内存 */
    pname = mymalloc(SRAMIN, 2 * FF_MAX_LFN + 1);               /* 为带路径文件名分配内存 */
    
    voffsettbl = mymalloc(SRAMIN, 4 * totavinum);               /* 分配4*totavinum个字节的内存,用于存放视频文件偏移表 */
    
    while (vfileinfo == NULL || pname == NULL || voffsettbl == NULL)    /* 内存分配出错 */
    {
        text_show_string(60, 190, 240, 16, "内存分配失败!", 16, 0, RED);
        delay_ms(200);
        lcd_fill(60, 190, 240, 146, WHITE);     /* 清除显示 */
        delay_ms(200);
    }

    /* 记录偏移 */
    res = f_opendir(&vdir, "0:/VIDEO");         /* 打开目录 */
    if (res == FR_OK)
    {
        curindex = 0;   /* 当前索引为0 */
        while (1)       /* 全部查询一遍 */
        {
            temp = vdir.dptr;                   /* 记录当前offset */
            res = f_readdir(&vdir, vfileinfo);  /* 读取目录下的一个文件 */
            if (res != FR_OK || vfileinfo->fname[0] == 0)
            {
                break;                          /* 错误了/到末尾了,退出 */
            }

            res = exfuns_file_type(vfileinfo->fname);
            if ((res & 0xF0) == 0x60)           /* 取高四位,判断是不是视频文件 */
            {
                voffsettbl[curindex] = temp;    /* 记录偏移 */
                curindex++;
            }
        }
    }

    // 修改为无限循环播放列表，去除所有按键控制
    while (1)  // 添加外层循环以实现循环播放
    {
        curindex = 0;           /* 从0开始显示 */
        res = f_opendir(&vdir, (const TCHAR *)"0:/VIDEO");  /* 打开目录 */
        while (res == FR_OK)    /* 打开成功 */
        {
            dir_sdi(&vdir, voffsettbl[curindex]);   /* 改变当前目录项的值 */
            res = f_readdir(&vdir, vfileinfo);      /* 读取目录下的一个文件 */

            if (res != FR_OK || vfileinfo->fname[0] == 0)
            {
                break;          /* 错误了/到末尾了,退出 */
            }

            strcpy((char *)pname, "0:/VIDEO/");                     /* 复制路径(目录) */
            strcat((char *)pname, (const char *)vfileinfo->fname);  /* 把文件名添加到后面 */
            
            lcd_clear(WHITE);   /* 清屏 */
            // video_bmsg_show((uint8_t *)vfileinfo->fname, curindex + 1, totavinum);  /* 显示播放,文件信息 */
            
            video_play_mjpeg(pname);  /* 播放指定视频文件，不再接收返回值 */
            
            // 去掉所有按键控制逻辑，直接播放下一个视频
            curindex++;
            if (curindex >= totavinum)
            {
                curindex = 0;     /* 到末尾时,自动从头开始 */
            }
        }
    }

    myfree(SRAMIN, vfileinfo);      /* 释放内存 */
    myfree(SRAMIN, pname);          /* 释放内存 */
    myfree(SRAMIN, voffsettbl);     /* 释放内存 */
}
/**
 * @brief       播放一个mjpeg文件
 * @param       pname : 文件名
 * @retval      res
 *   @arg       KEY0_PRES , 下一曲.
 *   @arg       KEY1_PRES , 上一曲.
 *   @arg       其他 , 错误
 */
void video_play_mjpeg(uint8_t *pname)  // 修改返回类型为 void
{
    uint8_t *framebuf;  /* 视频数据buf */
    uint8_t *pbuf;      /* buf指针 */
    
    uint8_t  res = 0;
    uint16_t offset = 0;
    uint32_t nr;
    uint8_t i2ssavebuf;
    
    FIL *favi;

    i2sbuf[0] = mymalloc(SRAMIN, AVI_AUDIO_BUF_SIZE);   /* 申请音频内存 */
    i2sbuf[1] = mymalloc(SRAMIN, AVI_AUDIO_BUF_SIZE);   /* 申请音频内存 */
    i2sbuf[2] = mymalloc(SRAMIN, AVI_AUDIO_BUF_SIZE);   /* 申请音频内存 */
    i2sbuf[3] = mymalloc(SRAMIN, AVI_AUDIO_BUF_SIZE);   /* 申请音频内存 */
    
    framebuf = mymalloc(SRAMIN, AVI_VIDEO_BUF_SIZE) ;   /* 申请视频buf */
    
    favi = (FIL *)mymalloc(SRAMIN, sizeof(FIL));        /* 申请favi内存 */
    
    memset(i2sbuf[0], 0, AVI_AUDIO_BUF_SIZE);
    memset(i2sbuf[1], 0, AVI_AUDIO_BUF_SIZE); 
    memset(i2sbuf[2], 0, AVI_AUDIO_BUF_SIZE);
    memset(i2sbuf[3], 0, AVI_AUDIO_BUF_SIZE);

    if (!i2sbuf[3] || !framebuf || !favi)
    {
        printf("memory error!\r\n");
        res = 0xFF;
    }

    while (res == 0)
    {
        res = f_open(favi, (char *)pname, FA_READ);
        if (res == 0)
        {
            pbuf = framebuf;
            res = f_read(favi, pbuf, AVI_VIDEO_BUF_SIZE, &nr);  /* 初始化读取 */
            if (res)
            {
                printf("fread error:%d\r\n", res);
                break;
            }

            /* 初始化avi解析 */
            res = avi_init(pbuf, AVI_VIDEO_BUF_SIZE);           /* avi解析 */
            if (res)
            {
                printf("avi err:%d\r\n", res);
                break;
            }

             // video_info_show(&g_avix);
            btim_tim7_int_init(g_avix.SecPerFrame / 100 - 1, 8400 - 1);   /* 10Khz计数频率,每1个100us */
            offset = avi_srarch_id(pbuf, AVI_VIDEO_BUF_SIZE, "movi");     /* 寻找movi ID */
            avi_get_streaminfo(pbuf + offset + 4);  /* 获取流信息 */
            f_lseek(favi, offset + 12);             /* 跳过标记ID,地址偏移到真正的数据开始处 */
            
            res = mjpegdec_init((lcddev.width - g_avix.Width) / 2, (lcddev.height - g_avix.Height) / 2);
            init_text_stream();
            load_text_file_stream();
            if (g_avix.SampleRate)    /* 有音频信息,初始化 */
            {
                i2s_init(I2S_STANDARD_PHILIPS, I2S_MODE_MASTER_TX, I2S_CPOL_LOW, I2S_DATAFORMAT_16B_EXTENDED);          /* 使用飞利浦标准,主机发送,时钟低电平有效,16位帧结构 */
                i2s_samplerate_set(g_avix.SampleRate);      /* 设置采样率 */
                i2s_tx_dma_init(i2sbuf[1], i2sbuf[2], g_avix.AudioBufSize / 2);     /* 配置DMA */
                i2s_tx_callback = audio_i2s_dma_callback;   /* 回调函数指向I2S_DMA_Callback */
                i2splaybuf = 0;
                i2ssavebuf = 0;
                i2s_play_start();   /* 开启I2S播放 */
            }

            while (1)   /* 播放循环 */
            {
                // 在 video_play_mjpeg 函数中找到视频解码部分，修改为：
                if (g_avix.StreamID == AVI_VIDS_FLAG)       /* 视频帧 */
                {
                    pbuf = framebuf;
                    f_read(favi, pbuf, g_avix.StreamSize + 8, &nr);   /* 读取一帧+下一个ID信息 */
                    res = mjpegdec_decode(pbuf, g_avix.StreamSize);
                    display_text_overlay_stream();
                    if (res)
                    {
                        // printf("decode error!\r\n");
                    }

                    // 强制刷新视频区域以外的屏幕区域
                    // 计算视频显示区域的位置和尺寸
                    uint16_t video_x = (lcddev.width - g_avix.Width) / 2;
                    uint16_t video_y = (lcddev.height - g_avix.Height) / 2;
                    uint16_t video_width = g_avix.Width;
                    uint16_t video_height = g_avix.Height;

                    // 刷新视频区域上方背景
                    if (video_y > 0) {
                        lcd_fill(0, 0, lcddev.width - 1, video_y - 1, WHITE);
                    }

                    // 刷新视频区域下方背景
                    uint16_t bottom_y = video_y + video_height;
                    if (bottom_y < lcddev.height) {
                        lcd_fill(0, bottom_y, lcddev.width - 1, lcddev.height - 1, WHITE);
                    }

                    // 刷新视频区域左侧背景(与视频相同高度)
                    if (video_x > 0) {
                        lcd_fill(0, video_y, video_x - 1, video_y + video_height - 1, WHITE);
                    }

                    // 刷新视频区域右侧背景(与视频相同高度)
                    uint16_t right_x = video_x + video_width;
                    if (right_x < lcddev.width) {
                        lcd_fill(right_x, video_y, lcddev.width - 1, video_y + video_height - 1, WHITE);
                    }

                    while (frameup == 0);   /* 等待时间到(由TIM7中断将此值设置为1) */

                    frameup = 0;            /* 标志清零 */
                    frame++;
                }
                else    /* 音频帧 */
                {
                    // video_time_show(favi, &g_avix);   /* 显示当前播放时间 */
                    i2ssavebuf++;
                    
                    if (i2ssavebuf > 3)
                    {
                        i2ssavebuf = 0;
                    }

                    do
                    {
                        nr = i2splaybuf;

                        if (nr)
                        {
                            nr--;
                        }
                        else
                        {
                            nr = 3;
                        }
                    } while (i2ssavebuf == nr); /* 碰撞等待. */

                    f_read(favi, i2sbuf[i2ssavebuf], g_avix.StreamSize + 8, &nr); /* 填充i2sbuf */
                    pbuf = i2sbuf[i2ssavebuf];
                } 
 
                // 移除所有按键扫描和控制逻辑
                /*
                key = key_scan(0);
                if (key == KEY0_PRES || key == KEY2_PRES) // KEY0/KEY2按下,播放上一个/下一个视频
                {
                    res = key;
                    break;
                }
                else if (key == KEY1_PRES || key == WKUP_PRES)
                {
                    i2s_play_stop();    // 关闭音频
                    video_seek(favi, &g_avix, framebuf);
                    pbuf = framebuf;
                    i2s_play_start();   // 启动DMA播放
                }
                */

                // 检查是否到达文件末尾
                if (favi->fptr >= favi->obj.objsize) {
                    // 文件播放完成，退出循环
                    break;
                }

                if (avi_get_streaminfo(pbuf + g_avix.StreamSize))     /* 获取下一帧 标记 */
                {
                        // printf("g_frame error \r\n");
                        // 不再因为错误而退出，而是正常播放完成
                        break;
                }
            }

            i2s_play_stop();                                    /* 关闭音频 */
            TIM7->CR1 &= ~(1 << 0);                             /* 关闭定时器6 */
            lcd_set_window(0, 0, lcddev.width, lcddev.height);  /* 恢复全屏 */
            mjpegdec_free();                                    /* 释放内存 */
            f_close(favi);                                      /* 关闭文件 */
            break; // 成功播放完成后退出循环
        }
    }

    myfree(SRAMIN, i2sbuf[0]);
    myfree(SRAMIN, i2sbuf[1]);
    myfree(SRAMIN, i2sbuf[2]);
    myfree(SRAMIN, i2sbuf[3]);
    myfree(SRAMIN, framebuf);
    myfree(SRAMIN, favi);
}

/**
 * @brief       AVI文件查找
 * @param       favi    : AVI文件
 * @param       aviinfo : AVI信息结构体
 * @param       mbuf    : 数据缓冲区
 * @retval      执行结果
 *   @arg       0    , 成功
 *   @arg       其他 , 错误
 */
uint8_t video_seek(FIL *favi, AVI_INFO *aviinfo, uint8_t *mbuf)
{
    uint32_t fpos = favi->fptr;
    uint8_t *pbuf;
    uint16_t offset;
    uint32_t br;
    uint32_t delta;
    uint32_t totsec;
    uint8_t key;

    totsec = (aviinfo->SecPerFrame / 1000) * aviinfo->TotalFrame;
    totsec /= 1000;     /* 秒钟数 */
    delta = (favi->obj.objsize / totsec) * 5;   /* 每次前进5秒钟的数据量 */

    while (1)
    {
        key = key_scan(1);
        if (key == WKUP_PRES)       /* 快进 */
        {
            if (fpos < favi->obj.objsize)
            {
                fpos += delta;
            }

            if (fpos > (favi->obj.objsize - AVI_VIDEO_BUF_SIZE))
            {
                fpos = favi->obj.objsize - AVI_VIDEO_BUF_SIZE;
            }
        }
        else if (key == KEY1_PRES)  /* 快退 */
        {
            if (fpos > delta)
            {
                fpos -= delta;
            }
            else
            {
                fpos = 0;
            }
        }
        else 
        {
            break;
        }

        f_lseek(favi, fpos);
        f_read(favi, mbuf, AVI_VIDEO_BUF_SIZE, &br);    /* 读入整帧+下一数据流ID信息 */
        pbuf = mbuf;
        
        if (fpos == 0)  /* 从0开始,得先寻找movi ID */
        {
            offset = avi_srarch_id(pbuf, AVI_VIDEO_BUF_SIZE, "movi");
        }
        else
        {
            offset = 0;
        }

        offset += avi_srarch_id(pbuf + offset, AVI_VIDEO_BUF_SIZE, g_avix.VideoFLAG); /* 寻找视频帧 */
        avi_get_streaminfo(pbuf + offset);  /* 获取流信息 */
        f_lseek(favi, fpos + offset + 8);   /* 跳过标志ID,读地址偏移到流数据开始处 */
        
        if (g_avix.StreamID == AVI_VIDS_FLAG)
        {
            f_read(favi, mbuf, g_avix.StreamSize + 8, &br);   /* 读入整帧 */
            mjpegdec_decode(mbuf, g_avix.StreamSize);         /* 显示视频帧 */
        }
        else
        {
            printf("error flag");
        }

        video_time_show(favi, &g_avix);
    }

    return 0;
}

FRESULT read_next_char(char* ch)
{
    // 如果缓冲区已用完，重新加载
    if(text_stream.buffer_pos >= text_stream.buffer_len)
    {
        // 检查是否已到文件末尾
        if(text_stream.file_pos >= text_stream.file_size)
        {
            return FR_DISK_ERR; // 使用标准FATFS错误码替代FR_EOF
        }
        
        // 读取新的数据块
        FRESULT res = f_read(&text_stream.file, text_stream.buffer, TEXT_BUFFER_SIZE, &text_stream.buffer_len);
        if(res != FR_OK)
        {
            return res;
        }
        
        text_stream.buffer_pos = 0;
        text_stream.file_pos += text_stream.buffer_len;
    }
    
    // 返回下一个字符
    *ch = text_stream.buffer[text_stream.buffer_pos++];
    return FR_OK;
}

/**
 * @brief       流式加载文本文件
 * @param       无
 * @retval      无
 */
void load_text_file_stream(void)
{
    FRESULT res;
    char ch;
    
    
    // 初始化结构
    init_text_stream();
    
    // 打开文件
    res = f_open(&text_stream.file, "0:/cyrene.txt", FA_READ);
    if(res != FR_OK)
    {
        printf("无法打开 cyrene.txt 文件，错误代码: %d\r\n", res);
        return;
    }
    
    // 获取文件大小
    text_stream.file_size = f_size(&text_stream.file);
     // printf("文件大小: %d 字节\r\n", text_stream.file_size);
    
    // 读取前几行用于显示
    text_stream.current_line = 0;
    text_stream.line_pos = 0;
    
    while(text_stream.current_line < MAX_DISPLAY_LINES)
    {
        res = read_next_char(&ch);
        if(res == FR_DISK_ERR)
        {
            // 处理最后一行（如果没有换行符）
            if(text_stream.line_pos > 0)
            {
                text_stream.line_buffer[text_stream.line_pos] = '\0';
                if(strlen(text_stream.line_buffer) > 0)
                {
                    text_stream.display_lines[text_stream.current_line] = mymalloc(SRAMIN, strlen(text_stream.line_buffer) + 1);
                    if(text_stream.display_lines[text_stream.current_line])
                    {
                        strcpy(text_stream.display_lines[text_stream.current_line], text_stream.line_buffer);
                        text_stream.current_line++;
                    }
                }
            }
            break;
        }
        else if(res != FR_OK)
        {
            printf("读取文件出错: %d\r\n", res);
            break;
        }
        
        // 处理换行符
        if(ch == '\n' || ch == '\r')
        {
            // 处理空行或有内容的行
            text_stream.line_buffer[text_stream.line_pos] = '\0';
            // 为每行分配内存，即使是空行
            text_stream.display_lines[text_stream.current_line] = mymalloc(SRAMIN, strlen(text_stream.line_buffer) + 1);
            if(text_stream.display_lines[text_stream.current_line])
            {
                strcpy(text_stream.display_lines[text_stream.current_line], text_stream.line_buffer);
                text_stream.current_line++;
                if(text_stream.current_line >= MAX_DISPLAY_LINES) break;
            }
            text_stream.line_pos = 0;
            
            // 处理Windows换行符的第二个字符
            if(ch == '\r')
            {
                res = read_next_char(&ch);
                if(res == FR_OK && ch != '\n')
                {
                    // 如果不是\n，将字符放回
                    text_stream.buffer_pos--;
                }
            }
        }
        else
        {
            // 添加字符到行缓冲区（防止溢出）
            if(text_stream.line_pos < sizeof(text_stream.line_buffer) - 1)
            {
                text_stream.line_buffer[text_stream.line_pos++] = ch;
            }
        }
    }
    
    text_stream.total_lines = text_stream.current_line;
    text_stream.display_start = 0;
    
    // 关闭文件
    f_close(&text_stream.file);
    
}
void display_text_overlay_stream(void)
{
    static uint16_t last_display_start = 0xFFFF;
    static uint16_t last_displayed_lines = 0;
    uint32_t current_time = HAL_GetTick(); 
    uint16_t i;
    uint16_t y_pos = 10;
    char display_str[50];
    
    if(text_stream.total_lines == 0) return;
    

    if(current_time - last_scroll_time >= SCROLL_INTERVAL)
    {

        text_stream.display_start++;
        if(text_stream.display_start >= text_stream.total_lines) 
            text_stream.display_start = 0;
        last_scroll_time = current_time; // ?ι?
        

        uint16_t new_line_index = (text_stream.display_start + MAX_DISPLAY_LINES - 1) % text_stream.total_lines;
        if(text_stream.display_lines[new_line_index])
        {
            printf(text_stream.display_lines[new_line_index]);
        }
        printf("\r\n");
    }
    

    uint16_t current_display_lines = (text_stream.total_lines < MAX_DISPLAY_LINES) ? 
                                     text_stream.total_lines : MAX_DISPLAY_LINES;
    

    for(i = 0; i < current_display_lines; i++)
    {
        uint16_t line_index = (text_stream.display_start + i) % text_stream.total_lines;
        if(text_stream.display_lines[line_index])
        {

            strncpy(display_str, text_stream.display_lines[line_index], sizeof(display_str) - 1);
            display_str[sizeof(display_str) - 1] = '\0';
            

            text_show_string(10, y_pos + i*12, lcddev.width-20, 12, display_str, 12, 1, RED);
        }
    }
    
    last_display_start = text_stream.display_start;
    last_displayed_lines = current_display_lines;
}