#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
// #include "hash.h"
#include "converter.h"

#define DEFAULT_BACKLOG 128

//�Ȳ����ǵ����

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

//Դ��ַ Ĭ��0
unsigned char source_address=0;
//������ַ Ĭ��1
unsigned short public_address=1;
//T2ʱ��Ĭ�� 10��
unsigned int T2time=10*1000;
//T3ʱ��Ĭ��  20��
unsigned int T3time=20*1000;

//ң������ 0ֱ�أ�Ĭ�ϣ���1ѡ��
unsigned char remote_control=0;

uv_timer_t T3job_req;

uv_loop_t *loop;
uv_tcp_t client;
uv_connect_t connect_req;
struct sockaddr_in addr;
uv_fs_t stdin_watcher;
char buffer[1024];
char SOEbuffer[100];

unsigned short data_send_num =0x00; //�����Ǵ�0��ʼ
unsigned short data_rec_num =0x00;  //�����Ǵ�0��ʼ

char *arg[10];

//��һ��bit��ʾ�Ƿ��Ѿ������ T2work�߳�
//�ڶ���bit ��T2��������ͽ������к�++�����Ӹ����������Ǳ��ԭ�Ӳ���   �������������ԭ�Ӳ�������T2wrok�߳�����ķ��Ͳ���ǡ�����ˣ��ͻ����©�����ظ������̰߳�ȫ����
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
    //statue T2�߳�״̬��ԭ
    status = status & 0b11111110;
    free(req);
}

void T2work(uv_work_t *req){
    uv_sleep(T2time);
    //�����������״̬
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

//Ҳ������������ʱ����
char* SOEtime(char* buf){
    unsigned short seconds_millisecond;
    //ͷ�����ֽ�����+����
    seconds_millisecond=charArrayToShort(buf);
    int seconds;
    int millisecond;
    millisecond=seconds_millisecond%1000;
    seconds=(seconds_millisecond-millisecond)/1000;
    sprintf(SOEbuffer,"%d��%d��%d��--%dʱ%d��%d��%d����",2000+*(buf+6),*(buf+5),*(buf+4),*(buf+3),*(buf+2),seconds,millisecond);
    return SOEbuffer;
}

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    *buf = uv_buf_init((char*) malloc(suggested_size), suggested_size);
}

void encode(char *buf){
    //�������I֡��Ϣ���͵��жϺʹ�����߼��� ȥͨ��λ�ڵ���λ�����ͱ�ʶ�͵ھ�λ����ԭ�� ȥ�ж�
    if (*(buf+6) ==0x64 && *(buf+8) ==0x07)  //����ȷ�ϻظ�  
    {
        printf("�յ�����ȷ�ϻظ�,��ʼ������������\n");
    }else if(*(buf+6) ==0x64 && *(buf+8) ==0x0a ){
        printf("�յ����м�����ֹ,����������������\n");
    }//������ʱֻ�� ��׼��ֵ0b��short�������ȸ���ֵ0d��float����������Ϣ01���н���
    else if(*(buf+6) ==0x0d ){
        if(*(buf+8) ==0x14){
            printf("ң�����ͣ������ȸ���ֵ             ����ԭ�򣺻�Ӧ����\n");
            
        }else if(*(buf+8) ==0x03){
            printf("ң�����ͣ������ȸ���ֵ             ����ԭ��ͻ����Ϣ\n");  
        }
        unsigned char len = *(buf+7);
        char quality[4];
        if( (len & byteboolean[7]) == 0 ){
            printf("�������Ե�ַ\n");
            for(int i=0;i<len;i++){
                if(*(buf+19+i*8)==0){
                    strncpy(quality,"��",4);
                }else{
                    strncpy(quality,"��",4);
                }
                printf("��ַ��%d                  ���ݣ�%f                Ʒ�ʣ�%s\n",_charArrayToInt(buf+12+i*8),charArrayToFloat(buf+15+i*8),&quality);
            }
        }else{
            printf("�����Ե�ַ\n");
            unsigned char mask=0b01111111;
            len=len & mask;
            int address=_charArrayToInt(buf+12);
             
            for(int i=0;i<len;i++){
                if(*(buf+19+i*5)==0){
                    strncpy(quality,"��",4);
                }else{
                    strncpy(quality,"��",4);
                }
                printf("��ַ��%d                  ���ݣ�%f                Ʒ�ʣ�%s\n",address+i,charArrayToFloat(buf+15+i*5),&quality);
            }
        }
        
    }else if(*(buf+6) ==0x0b){
        if(*(buf+8) ==0x14){
            printf("ң�����ͣ���׼��ֵ                 ����ԭ�򣺻�Ӧ����\n");
            
        }else if(*(buf+8) ==0x03){
            printf("ң�����ͣ���׼��ֵ                 ����ԭ��ͻ����Ϣ\n");  
        }
        unsigned char len = *(buf+7);
        char quality[4];
        if( (len & byteboolean[7]) == 0 ){
            printf("�������Ե�ַ\n");
            for(int i=0;i<len;i++){
                if(*(buf+17+i*6)==0){
                    strncpy(quality,"��",4);
                }else{
                    strncpy(quality,"��",4);
                }
                printf("��ַ��%d                  ���ݣ�%d                Ʒ�ʣ�%s\n",_charArrayToInt(buf+12+i*6),charArrayToShort(buf+15+i*6),&quality);
            }
        }else{
            printf("�����Ե�ַ\n");
            unsigned char mask=0b01111111;
            len=len & mask;
            int address=_charArrayToInt(buf+12);
             
            for(int i=0;i<len;i++){
                if(*(buf+17+i*3)==0){
                    strncpy(quality,"��",4);
                }else{
                    strncpy(quality,"��",4);
                }
                printf("��ַ��%d                  ���ݣ�%d                Ʒ�ʣ�%s\n",address+i,charArrayToShort(buf+15+i*3),&quality);
            }
        }
    }else if(*(buf+6) ==0x01){
        if(*(buf+8) ==0x14){
            printf("ң������                          ����ԭ�򣺻�Ӧ����\n");
            
        }else if(*(buf+8) ==0x03){
            printf("ң������                          ����ԭ��ͻ����Ϣ\n");  
        }
        unsigned char len = *(buf+7);
        if( (len & byteboolean[7]) == 0 ){
            printf("�������Ե�ַ\n");
            for(int i=0;i<len;i++){
                printf("��ַ��%d                  ���ݣ�%d                \n",_charArrayToInt(buf+12+i*4),*((char *)(buf+15+i*4)));
            }
        }else{
            printf("�����Ե�ַ\n");
            unsigned char mask=0b01111111;
            len=len & mask;
            int address=_charArrayToInt(buf+12);
             
            for(int i=0;i<len;i++){
                printf("��ַ��%d                  ���ݣ�%d                \n",address+i,*((char *)(buf+15+i)));
            }
        }
    }else if(*(buf+6) ==0x1e){
        if(*(buf+8) ==0x14){
            printf("SOE����                          ����ԭ�򣺻�Ӧ����\n");
            
        }else if(*(buf+8) ==0x03){
            printf("SOE����                          ����ԭ��ͻ����Ϣ\n");  
        }
        unsigned char len = *(buf+7);
        if( (len & byteboolean[7]) == 0 ){
            printf("�������Ե�ַ\n");
            for(int i=0;i<len;i++){
                printf("��ַ��%d                  ���ݣ�%d                ʱ�䣺%s\n",_charArrayToInt(buf+12+i*11),*((char *)(buf+15+i*11)),SOEtime(buf+16+i*11));
            }
        }else{
            printf("�����Ե�ַ\n");
            unsigned char mask=0b01111111;
            len=len & mask;
            int address=_charArrayToInt(buf+12);
             
            for(int i=0;i<len;i++){
                printf("��ַ��%d                  ���ݣ�%d                ʱ�䣺%s\n",address+i,*((char *)(buf+15+i*7)),SOEtime(buf+16+i*7));
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
            printf("��վ��Ӧң��ִ��\n");
        }else if(*(buf+8) ==0x0a){
            printf("��վң��ִ�����\n");
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
            printf("��վ��Ӧң��ִ��\n");
        }else if(*(buf+8) ==0x0a){
            printf("��վң��ִ�����\n");
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
    //����T3
    uv_timer_again(&T3job_req);
    // �����ﴦ����յ�������...
    printf("Received:\n");
    print_buffer((unsigned char*)(buf->base),nread);

    if(*(buf->base+0) !=0x68){
        printf("�������ֽ����ݲ�����0x68,��104Э�飡����");
        return;
    }
    int packet_num=0;
    //��һ�ε��������䱨�ļ��� �š���������������û��Ҫ��ֱ�ӷ����а�
    if(data_rec_num==0&&data_send_num==0){
        if(*(buf->base+1) !=0x04 || *(buf->base+2) !=0x0b){
            fprintf(stderr,"��վδ��Ӧ������· ��ظ�������\n");
            printf("Received:\n");
            print_buffer((unsigned char*)(buf->base),nread);
            return;
        }
        write_req_t *req1 = (write_req_t*) malloc(sizeof(write_req_t));
        char *buffer_data=(char *) malloc(sizeof(char)*256);
	    memset(buffer_data,0,256);
        //��ͷ��ʶ 68
        buffer_data[0]=0x68;
        //����
        buffer_data[1]=0x0e;
        //�������к�
        memcpy(buffer_data+2,&data_send_num, 2);
        //�������к�
        memcpy(buffer_data+4,&data_rec_num, 2);
        //ASDU���ͱ�ʶ 64����
        buffer_data[6]=0x64;
        //�ɱ�ṹ�޶���
        buffer_data[7]=0x01;
        //����ԭ��
        buffer_data[8]=0x06;
        //Դ��ַ
        buffer_data[9]=source_address;
        //������ַ
        memcpy(buffer_data+10,&public_address, 2);
        //���� 00
        buffer_data[12]=0x00;
        buffer_data[13]=0x00;
        buffer_data[14]=0x00;
        //���еĽ����� 14
        buffer_data[15]=0x14;
        printf("send ���� data:\n");
        //���еı��Ĺ̶�����16 �����ǹ̶����ݵ�
        print_buffer(buffer_data,16);
        req1->buf = uv_buf_init(buffer_data, 16);
        uv_write((uv_write_t*)req1, (uv_stream_t*)&client,&req1->buf, 1, on_write);
        //������󣬷������кž�����++
        data_send_num=data_send_num+2;
    }else{
        int buf_start[10];
        //i����ÿһ��packet����ʼ��ַ
        int i=0;
        //�Զ�һ�η��Ϳ��ܽ�������Ϣһ���� ���Ե��������
        if(*(buf->base+1)+2 < nread){
            while(i<nread){
                //printf("i=%d,nread=%d\n",i,nread);
                //������Խ��ķ���
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
    //ֻ��I֡�Żᵼ�����к�++ U֡��S֡���ȶ���6��ͨ�������ж�U��S��I������
    if(*(buf->base+1) !=0x04){
        //����
        status = status | byteboolean[1];
        data_rec_num=data_rec_num+2*packet_num;
        //�����Ƿ��Ѿ���T2work�߳�
        if((status & byteboolean[0])==0){
            uv_work_t *uv_work=(uv_work_t *)malloc(sizeof(uv_work_t));
            uv_queue_work(loop,uv_work,T2work,after_T2work);
            //��T2�߳����е�״̬����
            status = status | byteboolean[0];
        }
        //����
        status = status & 0b11111101;
    }
    free(buf->base);
}

void on_type(uv_fs_t *req) {
    if (stdin_watcher.result > 0) {
        buffer[stdin_watcher.result] = '\0';
        printf("Typed %s\n", buffer);
        splitString(buffer,",",arg);
        //1ң�أ�2ң��
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
            fprintf(stderr,"��λ������1��2��1ң�أ�2ң��");
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
    //����T3
    uv_timer_start(&T3job_req, T3job, T3time, T3time);
    // �������ݵ�������
    //iec104����ֱ����memcpy��Ϊ������С�ˣ�ֱ����Ҳ��С��д��ȥ��iec104 TCP/IP�õ���С��
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
    uv_ip4_addr("192.168.1.21", 2404, &server_addr); // ʹ����ķ�������ַ�Ͷ˿�
    uv_tcp_connect(&connect_req, &client, (const struct sockaddr*)&server_addr, on_connect);
    uv_buf_t buf = uv_buf_init(buffer, 1024);
    uv_fs_read(loop, &stdin_watcher, 0, &buf, 1, -1, on_type);
    uv_timer_init(loop, &T3job_req);
    uv_run(loop, UV_RUN_DEFAULT);
    return 0;
}