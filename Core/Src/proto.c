#include "proto.h"
#include <string.h>


#define S_SOF0              0  
#define S_SOF1              1   
#define S_LEN0              2
#define S_LEN1              3
#define S_CMD               4  
#define S_SEQ               5  
#define S_PAY               6  
#define S_CHK               7



proto_frame_t g_rx_frame;

static proto_tx_hook_t s_tx_hook=(proto_tx_hook_t)0;



static uint8_t s_state=S_SOF0;
static uint16_t s_len;
static uint16_t s_got;
static uint8_t s_sum;

void     PROTO_PutU16(uint8_t *p, uint16_t v){
    p[0]=(uint8_t)(v&0xFFu);
    p[1]=(uint8_t)((v>>8)&0xFFu);
}

uint16_t PROTO_GetU16(const uint8_t *p){
    return (uint16_t)(((uint16_t)p[0])
         | ((uint16_t)p[1]<<8));
}

void     PROTO_PutU32(uint8_t *p, uint32_t v){
    p[0]=(uint8_t)(v&0xFFu);
    p[1]=(uint8_t)((v>>8)&0xFFu);
    p[2]=(uint8_t)((v>>16)&0xFFu);
    p[3]=(uint8_t)((v>>24)&0xFFu);
}

uint32_t PROTO_GetU32(const uint8_t *p){
    return (uint32_t)(((uint32_t)p[0])
         | ((uint32_t)p[1]<<8)
         | ((uint32_t)p[2]<<16)
         | ((uint32_t)p[3]<<24));
}



/*   输出：AA 55 | LEN_L LEN_H | CMD | SEQ | PAYLOAD… | SUM
累加和覆盖范围 = CMD + SEQ + PAYLOAD，共 (len + 2) 字节 */

uint16_t PROTO_Build(uint8_t *out, uint8_t cmd, uint8_t seq,
                     const uint8_t *pay, uint16_t len){
    uint8_t sum;
    uint16_t i;
    uint16_t n=0;

    if(len>PROTO_MAX_PAY){
        return 0;           //超出最大载荷
    }

    out[n++]=(uint8_t)PROTO_SOF0;
    out[n++]=(uint8_t)PROTO_SOF1;
    out[n++]=(uint8_t)(len&0xFFu);
    out[n++]=(uint8_t)((len>>8)&0xFFu);
    out[n++]=(uint8_t)cmd;
    out[n++]=(uint8_t)seq;

    if(len>0&&pay!=(const uint8_t*)0){
        memcpy(&out[n],pay,len);
        n=(uint16_t)(len+n);
    }

    sum=0;
    for(i=4;i<(uint16_t)(len+6);i++){
        sum=(uint8_t)(sum+out[i]);
    }
    out[n++]=sum;

    return n;
}


void PROTO_ParserInit(void){
    s_state=S_SOF0;
    s_len=0;
    s_got=0;
    s_sum=0;
}

//解析状态机
int8_t PROTO_FeedByte(uint8_t b){
    switch(s_state){
        case S_SOF0:
            if(b==PROTO_SOF0){
                s_state=S_SOF1;
            }
            break;
        case S_SOF1:
            if(b==PROTO_SOF1){
                s_state=S_LEN0;
            }
            else if(b==PROTO_SOF0){
                s_state=S_SOF1;
            }
            else{
                s_state=S_SOF0;
            }
            break;
        case S_LEN0:
            s_len=(uint16_t)b;
            s_state=S_LEN1;
            break;
        case S_LEN1:
            s_len|=(uint16_t)((uint16_t)b<<8);
            if(s_len>PROTO_MAX_PAY){
                s_state=S_SOF0;
            }
            else{
                g_rx_frame.len=s_len;
                s_got=0;
                s_state=S_CMD;
            }
            break;
        case S_CMD:
            g_rx_frame.cmd=b;
            s_sum=b;
            s_state=S_SEQ;
            break;
        case S_SEQ:
            g_rx_frame.seq=b;
            s_sum=(uint8_t)(s_sum+b);
            if(s_len==0){
                s_state=S_CHK;
            }
            else{
                s_state=S_PAY;
            }
            break;
        case S_PAY:
            g_rx_frame.pay[s_got++]=b;
            s_sum=(uint8_t)(s_sum+b);
            if(s_got>=s_len){
                s_state=S_CHK;
            }
            break;
        case S_CHK:
            if(b==s_sum){
                s_state=S_SOF0;
                return 1;
            }
            s_state=(b==PROTO_SOF0)?(uint8_t)S_SOF1:(uint8_t)S_SOF0;
            return -1;

        default:
            s_state=S_SOF0;
            break;
    }
    return 0;
}

void PROTO_SetTxHook(proto_tx_hook_t hook){
    s_tx_hook=hook;
}

int8_t PROTO_Send(uint8_t cmd,uint8_t seq,const uint8_t *pay,uint16_t len){
    uint8_t buf[PROTO_MAX_FRAME];
    uint16_t n;

    if(s_tx_hook==(proto_tx_hook_t)0){
        return -2;          //没注册发送函数
    }

    n=PROTO_Build(buf,cmd,seq,pay,len);
    if(n==0){
        return -3;          //载荷过长
    }

    return s_tx_hook(buf,n);
}


int8_t PROTO_SendAck(uint8_t ack_cmd,uint8_t seq,uint8_t result){
    uint8_t pay[ACK_PAY_LEN];

    pay[ACK_OFF_CMD]=ack_cmd;
    pay[ACK_OFF_SEQ]=seq;
    pay[ACK_OFF_RESULT]=result;
    return PROTO_Send(CMD_ACK,seq,pay,ACK_PAY_LEN);
}

