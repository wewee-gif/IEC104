#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
// #include "hash.h"
#include "converter.h"

#define DEFAULT_BACKLOG 128

//先不考虑电度量

const static unsigned char b1=0b00000001;
const static unsigned char b2=0b00000010;
const static unsigned char b3=0b00000100;
const static unsigned char b4=0b00001000;
const static unsigned char b5=0b00010000;
const static unsigned char b6=0b00100000;
const static unsigned char b7=0b01000000;
const static unsigned char b8=0b10000000;        

static char byteboolean[8]={b1,b2,b3,b4,b5,b6,b7,b8};

char *arg[10];

//源地址 默认0
unsigned char source_address=0;
//公共地址 默认1
unsigned short public_address=1;
//T2时间默认 10秒
unsigned int T2time=10*1000;
//T3时间默认  20秒
unsigned int T3time=20*1000;

//遥控类型 0直控（默认），1选择
unsigned char remote_control=0;

uv_timer_t T3job_req;

uv_loop_t *loop;
uv_tcp_t client;
uv_connect_t connect_req;
struct sockaddr_in addr;
uv_fs_t stdin_watcher;
char buffer[1024];
char SOEbuffer[100];

unsigned short data_send_num =0x00; //发送是从0开始
unsigned short data_rec_num =0x00;  //接收是从0开始

char *arg[10];

//第一个bit表示是否已经添加了 T2work线程
//第二个bit 给T2检验操作和接收序列号++操作加个锁，让他们变成原子操作   如果他们俩不是原子操作，让T2wrok线程里面的发送操作恰进来了，就会出现漏发、重复发的线程安全问题
unsigned char status=0;

typedef struct {
    uv_write_t req;
    uv_buf_t buf;
} write_req_t;

void free_write_req(uv_write_t *req) {
    write_req_t *wr = (write_req_t*) req;
    free(wr->buf.base); 
    free(wr);
}
void on_write(uv_write_t *req, int status) {
    if (status) {
        fprintf(stderr, "Write error %s\n", uv_strerror(status));
    }
    printf("send areadly\n");
    free_write_req(req);
}
void after_T2work(uv_work_t *req,int uv_status){
    //statue T2线程状态还原
    status = status & 0b11111110;
    free(req);
}

void T2work(uv_work_t *req){
    uv_sleep(T2time);
    //如果处于上锁状态
    while((status & byteboolean[1])==1){
        uv_sleep(50);
    }
    write_req_t *req1 = (write_req_t*) malloc(sizeof(write_req_t));
    char *buffer_data=(char *) malloc(sizeof(char)*256);
	memset(buffer_data,0,256);
    buffer_data[0]=0x68;
    buffer_data[1]=0x04;
    buffer_data[2]=0x01;
    buffer_data[3]=0x00;
    memcpy(buffer_data+4,&data_rec_num, 2);
    printf("send T2work data:\n");
    print_buffer(buffer_data,6);
    req1->buf = uv_buf_init(buffer_data, 6);
    uv_write((uv_write_t*)req1, (uv_stream_t*)&client,&req1->buf, 1, on_write);
}

void T3job(){
    write_req_t *req1 = (write_req_t*) malloc(sizeof(write_req_t));
    char *buffer_data=(char *) malloc(sizeof(char)*256);
	memset(buffer_data,0,256);
    buffer_data[0]=0x68;
    buffer_data[1]=0x04;
    buffer_data[2]=0x43;
    buffer_data[3]=0x00;
    buffer_data[4]=0x00;
    buffer_data[5]=0x00;
    printf("send T3job data:\n");
    print_buffer(buffer_data,6);
    req1->buf = uv_buf_init(buffer_data, 6);
    uv_write((uv_write_t*)req1, (uv_stream_t*)&client,&req1->buf, 1, on_write);
}

//也可以用来做对时报文
char* SOEtime(char* buf){
    unsigned short seconds_millisecond;
    //头两个字节是秒+毫秒
    seconds_millisecond=charArrayToShort(buf);
    int seconds;
    int millisecond;
    millisecond=seconds_millisecond%1000;
    seconds=(seconds_millisecond-millisecond)/1000;
    sprintf(SOEbuffer,"%d年%d月%d日--%d时%d分%d秒%d毫秒",2000+*(buf+6),*(buf+5),*(buf+4),*(buf+3),*(buf+2),seconds,millisecond);
    return SOEbuffer;
}

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    *buf = uv_buf_init((char*) malloc(suggested_size), suggested_size);
}

void encode(char *buf){
    //这里对于I帧消息类型的判断和处理的逻辑是 去通过位于第七位的类型标识和第九位传输原因 去判断
    if (*(buf+6) ==0x64 && *(buf+8) ==0x07)  //总招确认回复  
    {
        printf("收到总招确认回复,开始接收总招数据\n");
    }else if(*(buf+6) ==0x64 && *(buf+8) ==0x0a ){
        printf("收到总招激活中止,结束接收总招数据\n");
    }//这里暂时只对 标准化值0b（short、单精度浮点值0d（float）、单点信息01进行解析
    else if(*(buf+6) ==0x0d ){
        if(*(buf+8) ==0x14){
            printf("遥测类型：单精度浮点值             传输原因：回应总招\n");
            
        }else if(*(buf+8) ==0x03){
            printf("遥测类型：单精度浮点值             传输原因：突变消息\n");  
        }
        unsigned char len = *(buf+7);
        char quality[4];
        if( (len & byteboolean[7]) == 0 ){
            printf("非连续性地址\n");
            for(int i=0;i<len;i++){
                if(*(buf+19+i*8)==0){
                    strncpy(quality,"好",4);
                }else{
                    strncpy(quality,"坏",4);
                }
                printf("地址：%d                  数据：%f                品质：%s\n",_charArrayToInt(buf+12+i*8),charArrayToFloat(buf+15+i*8),&quality);
            }
        }else{
            printf("连续性地址\n");
            unsigned char mask=0b01111111;
            len=len & mask;
            int address=_charArrayToInt(buf+12);
             
            for(int i=0;i<len;i++){
                if(*(buf+19+i*5)==0){
                    strncpy(quality,"好",4);
                }else{
                    strncpy(quality,"坏",4);
                }
                printf("地址：%d                  数据：%f                品质：%s\n",address+i,charArrayToFloat(buf+15+i*5),&quality);
            }
        }
        
    }else if(*(buf+6) ==0x0b){
        if(*(buf+8) ==0x14){
            printf("遥测类型：标准化值                 传输原因：回应总招\n");
            
        }else if(*(buf+8) ==0x03){
            printf("遥测类型：标准化值                 传输原因：突变消息\n");  
        }
        unsigned char len = *(buf+7);
        char quality[4];
        if( (len & byteboolean[7]) == 0 ){
            printf("非连续性地址\n");
            for(int i=0;i<len;i++){
                if(*(buf+17+i*6)==0){
                    strncpy(quality,"好",4);
                }else{
                    strncpy(quality,"坏",4);
                }
                printf("地址：%d                  数据：%d                品质：%s\n",_charArrayToInt(buf+12+i*6),charArrayToShort(buf+15+i*6),&quality);
            }
        }else{
            printf("连续性地址\n");
            unsigned char mask=0b01111111;
            len=len & mask;
            int address=_charArrayToInt(buf+12);
             
            for(int i=0;i<len;i++){
                if(*(buf+17+i*3)==0){
                    strncpy(quality,"好",4);
                }else{
                    strncpy(quality,"坏",4);
                }
                printf("地址：%d                  数据：%d                品质：%s\n",address+i,charArrayToShort(buf+15+i*3),&quality);
            }
        }
    }else if(*(buf+6) ==0x01){
        if(*(buf+8) ==0x14){
            printf("遥信类型                          传输原因：回应总招\n");
            
        }else if(*(buf+8) ==0x03){
            printf("遥信类型                          传输原因：突变消息\n");  
        }
        unsigned char len = *(buf+7);
        if( (len & byteboolean[7]) == 0 ){
            printf("非连续性地址\n");
            for(int i=0;i<len;i++){
                printf("地址：%d                  数据：%d                \n",_charArrayToInt(buf+12+i*4),*((char *)(buf+15+i*4)));
            }
        }else{
            printf("连续性地址\n");
            unsigned char mask=0b01111111;
            len=len & mask;
            int address=_charArrayToInt(buf+12);
             
            for(int i=0;i<len;i++){
                printf("地址：%d                  数据：%d                \n",address+i,*((char *)(buf+15+i)));
            }
        }
    }else if(*(buf+6) ==0x1e){
        if(*(buf+8) ==0x14){
            printf("SOE类型                          传输原因：回应总招\n");
            
        }else if(*(buf+8) ==0x03){
            printf("SOE类型                          传输原因：突变消息\n");  
        }
        unsigned char len = *(buf+7);
        if( (len & byteboolean[7]) == 0 ){
            printf("非连续性地址\n");
            for(int i=0;i<len;i++){
                printf("地址：%d                  数据：%d                时间：%s\n",_charArrayToInt(buf+12+i*11),*((char *)(buf+15+i*11)),SOEtime(buf+16+i*11));
            }
        }else{
            printf("连续性地址\n");
            unsigned char mask=0b01111111;
            len=len & mask;
            int address=_charArrayToInt(buf+12);
             
            for(int i=0;i<len;i++){
                printf("地址：%d                  数据：%d                时间：%s\n",address+i,*((char *)(buf+15+i*7)),SOEtime(buf+16+i*7));
            }
        }
    }else if(*(buf+6) ==0x2d){
        if(*(buf+15) ==0x81||*(buf+15) ==0x80){
            write_req_t *req1 = (write_req_t*) malloc(sizeof(write_req_t));
            char *buffer_data=(char *) malloc(sizeof(char)*256);
	        memset(buffer_data,0,256);
            buffer_data[0]=0x68;
            buffer_data[1]=0x0e;
            memcpy(buffer_data+2,&data_send_num, 2);
            memcpy(buffer_data+4,&data_rec_num, 2);
            buffer_data[6]=0x2d;
            buffer_data[7]=0x01;
            buffer_data[8]=0x06;
            buffer_data[9]=source_address;
            memcpy(buffer_data+10,&public_address, 2);
            memcpy(buffer_data+12,buf+12,3);
            if(*(buf+15) ==0x81){
                buffer_data[15]=0x01;
            }else if(*(buf+15) ==0x80){
                buffer_data[15]=0x00;
            }
            print_buffer(buffer_data,16);
            req1->buf = uv_buf_init(buffer_data, 16);
            uv_write((uv_write_t*)req1, (uv_stream_t*)&client,&req1->buf, 1, on_write);
        }else if(*(buf+8) ==0x07){
            printf("子站回应遥控执行\n");
        }else if(*(buf+8) ==0x0a){
            printf("子站遥控执行完毕\n");
        }
    }else if(*(buf+6) ==0x32){
        if(*(buf+15) ==0x80){
            write_req_t *req1 = (write_req_t*) malloc(sizeof(write_req_t));
            char *buffer_data=(char *) malloc(sizeof(char)*256);
	        memset(buffer_data,0,256);
            buffer_data[0]=0x68;
            buffer_data[1]=0x12;
            memcpy(buffer_data+2,&data_send_num, 2);
            memcpy(buffer_data+4,&data_rec_num, 2);
            buffer_data[6]=0x32;
            buffer_data[7]=0x01;
            buffer_data[8]=0x06;
            buffer_data[9]=source_address;
            memcpy(buffer_data+10,&public_address, 2);
            memcpy(buffer_data+12,buf+12,7);
            buffer_data[19]=0x00;
            print_buffer(buffer_data,20);
            req1->buf = uv_buf_init(buffer_data, 20);
            uv_write((uv_write_t*)req1, (uv_stream_t*)&client,&req1->buf, 1, on_write);
        }else if(*(buf+8) ==0x07){
            printf("子站回应遥调执行\n");
        }else if(*(buf+8) ==0x0a){
            printf("子站遥调执行完毕\n");
        }
    }else if(*(buf+1) ==0x04 && *(buf+2) ==0x43){
        write_req_t *req1 = (write_req_t*) malloc(sizeof(write_req_t));
        char *buffer_data=(char *) malloc(sizeof(char)*256);
	    memset(buffer_data,0,256);
        buffer_data[0]=0x68;
        buffer_data[1]=0x04;
        buffer_data[2]=0x83;
        buffer_data[3]=0x00;
        buffer_data[4]=0x00;
        buffer_data[5]=0x00;
        printf("send T3 call back data:\n");
        print_buffer(buffer_data,6);
        req1->buf = uv_buf_init(buffer_data, 6);
        uv_write((uv_write_t*)req1, (uv_stream_t*)&client,&req1->buf, 1, on_write);
    }
}

void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    if (nread < 0) {
        if (nread != UV_EOF)
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t*) stream, NULL);
        free(buf->base);
        return;
    }
    //重置T3
    uv_timer_again(&T3job_req);
    // 在这里处理接收到的数据...
    printf("Received:\n");
    print_buffer((unsigned char*)(buf->base),nread);

    if(*(buf->base+0) !=0x68){
        printf("报文首字节内容不等于0x68,非104协议！！！");
        return;
    }
    int packet_num=0;
    //第一次的启动传输报文检验 嗯。。。这个检验好像没必要，直接发总招吧
    if(data_rec_num==0&&data_send_num==0){
        if(*(buf->base+1) !=0x04 || *(buf->base+2) !=0x0b){
            fprintf(stderr,"从站未响应开启链路 或回复错误报文\n");
            printf("Received:\n");
            print_buffer((unsigned char*)(buf->base),nread);
            return;
        }
        write_req_t *req1 = (write_req_t*) malloc(sizeof(write_req_t));
        char *buffer_data=(char *) malloc(sizeof(char)*256);
	    memset(buffer_data,0,256);
        //开头标识 68
        buffer_data[0]=0x68;
        //长度
        buffer_data[1]=0x0e;
        //发送序列号
        memcpy(buffer_data+2,&data_send_num, 2);
        //接收序列号
        memcpy(buffer_data+4,&data_rec_num, 2);
        //ASDU类型标识 64总招
        buffer_data[6]=0x64;
        //可变结构限定词
        buffer_data[7]=0x01;
        //传输原因
        buffer_data[8]=0x06;
        //源地址
        buffer_data[9]=source_address;
        //公共地址
        memcpy(buffer_data+10,&public_address, 2);
        //三个 00
        buffer_data[12]=0x00;
        buffer_data[13]=0x00;
        buffer_data[14]=0x00;
        //总招的结束符 14
        buffer_data[15]=0x14;
        printf("send 总招 data:\n");
        //总招的报文固定长度16 甚至是固定内容的
        print_buffer(buffer_data,16);
        req1->buf = uv_buf_init(buffer_data, 16);
        uv_write((uv_write_t*)req1, (uv_stream_t*)&client,&req1->buf, 1, on_write);
        //发送完后，发送序列号就马上++
        data_send_num=data_send_num+2;
    }else{
        int buf_start[10];
        //i等于每一个packet的起始地址
        int i=0;
        //对端一次发送可能将多条信息一起发送 所以得来个拆包
        if(*(buf->base+1)+2 < nread){
            while(i<nread){
                //printf("i=%d,nread=%d\n",i,nread);
                //有数组越界的风险
                buf_start[packet_num]=i;
                i=*(buf->base+i+1)+2+i;
                packet_num++;
            }
            for(int j=0;j<packet_num;j++){
                encode(buf->base+buf_start[j]);
            }
        }else{
            encode(buf->base);
            packet_num++;
        }
    }
    //只有I帧才会导致序列号++ U帧、S帧长度都是6，通过长度判断U、S和I的区别
    if(*(buf->base+1) !=0x04){
        //上锁
        status = status | byteboolean[1];
        data_rec_num=data_rec_num+2*packet_num;
        //检验是否已经有T2work线程
        if((status & byteboolean[0])==0){
            uv_work_t *uv_work=(uv_work_t *)malloc(sizeof(uv_work_t));
            uv_queue_work(loop,uv_work,T2work,after_T2work);
            //把T2线程已有的状态加上
            status = status | byteboolean[0];
        }
        //开锁
        status = status & 0b11111101;
    }
    free(buf->base);
}

void on_type(uv_fs_t *req) {
    if (stdin_watcher.result > 0) {
        buffer[stdin_watcher.result] = '\0';
        printf("Typed %s\n", buffer);
        splitString(buffer,",",arg);
        //1遥控，2遥调
        if(strcmp(arg[0],"1")==0){
            short address=stringToShort(arg[1]);
            write_req_t *req1 = (write_req_t*) malloc(sizeof(write_req_t));
            char *buffer_data=(char *) malloc(sizeof(char)*256);
	        memset(buffer_data,0,256);
            buffer_data[0]=0x68;
            buffer_data[1]=0x0e;
            memcpy(buffer_data+2,&data_send_num, 2);
            memcpy(buffer_data+4,&data_rec_num, 2);
            buffer_data[6]=0x2d;
            buffer_data[7]=0x01;
            buffer_data[8]=0x06;
            buffer_data[9]=source_address;
            memcpy(buffer_data+10,&public_address, 2);
            memcpy(buffer_data+12,&address,2);
            buffer_data[14]=0x00;
            if(remote_control==0){
                if(strcmp(arg[2],"1")==0){
                    buffer_data[15]=0x01;
                }else{
                    buffer_data[15]=0x00;
                }
            }else if(remote_control==1){
                if(strcmp(arg[2],"1")==0){
                    buffer_data[15]=0x81;
                }else{
                    buffer_data[15]=0x80;
                }
            }
            print_buffer(buffer_data,16);
            req1->buf = uv_buf_init(buffer_data, 16);
            uv_write((uv_write_t*)req1, (uv_stream_t*)&client,&req1->buf, 1, on_write);
        }else if(strcmp(arg[0],"2")==0){
            printf("1111111");
            short address=stringToShort(arg[1]);
            float valve=atof(arg[2]);
            write_req_t *req1 = (write_req_t*) malloc(sizeof(write_req_t));
            char *buffer_data=(char *) malloc(sizeof(char)*256);
	        memset(buffer_data,0,256);
            buffer_data[0]=0x68;
            buffer_data[1]=0x12;
            memcpy(buffer_data+2,&data_send_num, 2);
            memcpy(buffer_data+4,&data_rec_num, 2);
            buffer_data[6]=0x32;
            buffer_data[7]=0x01;
            buffer_data[8]=0x06;
            buffer_data[9]=source_address;
            memcpy(buffer_data+10,&public_address, 2);
            memcpy(buffer_data+12,&address,2);
            buffer_data[14]=0x00;
            memcpy(buffer_data+15,&valve,4);
            if(remote_control==0){
                buffer_data[19]=0x00;
            }else if(remote_control==1){
                buffer_data[19]=0x80;
            }else{
                printf("22222");
            }
            print_buffer(buffer_data,20);
            req1->buf = uv_buf_init(buffer_data, 20);
            uv_write((uv_write_t*)req1, (uv_stream_t*)&client,&req1->buf, 1, on_write);
        }else{
            fprintf(stderr,"首位请输入1或2，1遥控，2遥调");
        }

    }
    else if (stdin_watcher.result < 0) {
        fprintf(stderr, "error opening file: %s\n", uv_strerror(req->result));
    }

    uv_buf_t buf1 = uv_buf_init(buffer, 1024);
    uv_fs_read(loop, &stdin_watcher, 0, &buf1, 1, -1, on_type);
}

void on_connect(uv_connect_t *req, int status) {
    if (status < 0) {
        fprintf(stderr, "Connection error %s\n", uv_strerror(status));
        return;
    }
    uv_read_start((uv_stream_t*) &client, alloc_buffer, on_read);
    //启动T3
    uv_timer_start(&T3job_req, T3job, T3time, T3time);
    // 发送数据到服务器
    //iec104可以直接用memcpy因为电脑是小端，直接用也是小端写进去，iec104 TCP/IP用的是小端
    //memcpy(buffer+0,&data_num, 2);
    buffer[0]=0x68;
    buffer[1]=0x04;
    buffer[2]=0x07;
    buffer[3]=0x00;
    buffer[4]=0x00;
    buffer[5]=0x00;
    uv_write_t write_req;
    uv_buf_t send_buf = uv_buf_init(buffer, 6);
    uv_write(&write_req, (uv_stream_t*)&client, &send_buf, 1, NULL);
    printf("send initialize data:\n");
    print_buffer(buffer,6);

}

int main() {
    loop = uv_default_loop();
    uv_tcp_init(loop, &client);
    struct sockaddr_in server_addr;
    uv_ip4_addr("192.168.1.21", 2404, &server_addr); // 使用你的服务器地址和端口
    uv_tcp_connect(&connect_req, &client, (const struct sockaddr*)&server_addr, on_connect);
    uv_buf_t buf = uv_buf_init(buffer, 1024);
    uv_fs_read(loop, &stdin_watcher, 0, &buf, 1, -1, on_type);
    uv_timer_init(loop, &T3job_req);
    uv_run(loop, UV_RUN_DEFAULT);
    return 0;
}