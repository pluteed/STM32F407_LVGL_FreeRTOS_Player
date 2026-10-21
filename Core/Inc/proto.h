#ifndef __PROTO_H
#define __PROTO_H


#include "stm32f4xx_hal.h"
#include "stdint.h"

//定义帧头——0xAA 0x55魔法帧，易于检测跳变是否正常，且在传输中不易出现
#define PROTO_SOF0          0xAAu   
#define PROTO_SOF1          0x55u   

#define PROTO_HDR_LEN       6u
#define PROTO_CHK_LEN       1u

#define PROTO_OVERHEAD      (PROTO_HDR_LEN+PROTO_CHK_LEN)
#define PROTO_MAX_PAY       260u

#define PROTO_MAX_FRAME    (PROTO_OVERHEAD+PROTO_MAX_PAY)

//FATFS中开启lfn后最长中文名为255
#define PROTO_NAME_MAX      255u



//指令字节
//下位机f103--->f407  0x02-0x09预留，方便扩充
#define CMD_ACK             0x01u
#define CMD_LIST_BEGIN      0x10u   //歌曲列表头部，后面载荷放列表长度
#define CMD_LIST_ITEM       0x11u   //紧跟列表头部后，后跟序号+歌名
#define CMD_LIST_END        0x12u   //同begin，后跟统计总数，如不一致则丢帧
#define CMD_STATUS          0x13u   //播放状态
#define CMD_EVENT           0x14u   //事件通知
#define CMD_PONG            0x15u   //回应PING，联通测试


//上位机f407--->f103 
#define CMD_PING            0x20u
#define CMD_LIST_REQ        0x21u
#define CMD_PLAY_IDX        0x22u
#define CMD_PAUSE           0x23u
#define CMD_RESUME          0x24u
#define CMD_STOP            0x25u
#define CMD_NEXT            0x26u
#define CMD_PREV            0x27u
#define CMD_SET_VOL         0x28u
#define CMD_STATUS_REQ      0x29u
#define CMD_SET_MODE        0x2Au   //设置播放模式，载荷 = u8 模式码（PLAYER_MODE_*）

//状态字 CMD_STATUS用
#define ST_STOP             0u
#define ST_PLAYING          1u
#define ST_PAUSE            2u
#define ST_ERROR            3u


//事件字 CMD_EVENT用
#define EVT_TRACK_END       0u
#define EVT_AUTO_NEXT       1u
#define EVT_SD_ERR          2u
#define EVT_NO_FILE         3u


//应答字 CMD_ACK用
#define ACK_OK              0u
#define ACK_ERR_CMD         1u
#define ACK_ERR_PAR         2u
#define ACK_ERR_BUSY        3u
#define ACK_ERR_FS          4u

//应答帧载荷在CMD_ACK中的索引，固定3字节
#define ACK_OFF_CMD         0u      //u8  被应答的命令字，上位机靠它对号入座
#define ACK_OFF_SEQ         1u      //u8  回显请求序号，跟帧头SEQ同值，冗余存一份便于自校验
#define ACK_OFF_RESULT      2u      //u8  应答字，取值见上面的 ACK_*
#define ACK_PAY_LEN         3u      //固定3字节 = 1+1+1


typedef struct{
    uint8_t state;
    uint8_t vol;
    uint16_t index;
    uint16_t total;
    uint32_t pos_sec;
    uint32_t dur_sec;
}proto_status_t;

//状态帧发送的顺序，所需状态字在载荷中的索引
#define ST_OFF_STATE        0u
#define ST_OFF_VOL          1u      //u8  音量0~100，与CMD_SET_VOL同一标度            
#define ST_OFF_INDEX        2u             
#define ST_OFF_TOTAL        4u                    
#define ST_OFF_POS          6u                     
#define ST_OFF_DUR          10u                    
#define ST_PAY_LEN          14u     

//事件帧载荷在CMD_EVENT中的索引
#define EVT_OFF_TYPE        0u      //u8  事件类型，取值见上面的 EVT_*
#define EVT_OFF_NEWIDX      1u      //u16 关联的曲目序号
#define EVT_PAY_LEN         3u      //固定3字节 = 1+2



typedef struct{
    uint16_t index;
    char name[PROTO_NAME_MAX];
    uint16_t name_len;
}proto_item_t;

/* CMD_LIST_ITEM 载荷布局：歌名变长，故 LEN = 2 + name_len */
#define IT_OFF_INDEX        0u
#define IT_OFF_NAME         2u

//列表开始/结束帧载荷索引，CMD_LIST_BEGIN与CMD_LIST_END共用
#define LST_OFF_TOTAL       0u      //u16 曲目总数，BEGIN和END两帧应一致，不一致整批作废
#define LST_PAY_LEN         2u

//CMD_PLAY_IDX载荷索引
#define PLY_OFF_INDEX       0u      //u16 曲目序号，从0开始
#define PLY_PAY_LEN         2u

//CMD_SET_VOL载荷索引，只有百分比，不掺VS1053的衰减语义
#define VOL_OFF_VOL         0u      //u8  音量0~100，100=最大声，0=静音
#define VOL_PAY_LEN         1u

//CMD_SET_MODE载荷索引，只有模式码，不掺其它语义
#define MODE_OFF_CODE       0u      //u8  播放模式码，取值见 PLAYER_MODE_*
#define MODE_PAY_LEN        1u


//存一整帧的结构体
typedef struct{
    uint8_t cmd;
    uint8_t seq;
    uint16_t len;
    uint8_t pay[PROTO_MAX_PAY];
}proto_frame_t;
extern proto_frame_t g_rx_frame;


//组帧:将一帧转换成uint8_t数组格式循环输出
uint16_t PROTO_Build(uint8_t *out, uint8_t cmd, uint8_t seq,
                     const uint8_t *pay, uint16_t len);

//初始化解析机
void PROTO_ParserInit(void);


//返回 1收到一整个合法帧
//返回 0收取中
//返回 -1校验失败，状态机复位，喂下一帧
int8_t PROTO_FeedByte(uint8_t b);


void     PROTO_PutU16(uint8_t *p, uint16_t v);
uint16_t PROTO_GetU16(const uint8_t *p);
void     PROTO_PutU32(uint8_t *p, uint32_t v);
uint32_t PROTO_GetU32(const uint8_t *p);

typedef int8_t (*proto_tx_hook_t)(const uint8_t *buf, uint16_t len);
void     PROTO_SetTxHook(proto_tx_hook_t hook);

/* 组帧并发送。返回 0 成功，<0 表示底层忙（本帧未发出，需重试） */
int8_t   PROTO_Send(uint8_t cmd, uint8_t seq, const uint8_t *pay, uint16_t len);

/* 便捷封装：发一个带 1 字节结果的 ACK（回显被应答的命令字与序号，便于配对） */
int8_t   PROTO_SendAck(uint8_t ack_cmd, uint8_t seq, uint8_t result);


#endif
