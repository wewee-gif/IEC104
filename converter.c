#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "converter.h"


//打印函数
void print_buffer(unsigned char *buf,ssize_t len){
    for(int i=0;i<len;i++){
        printf("%02x ",buf[i]);
    }
    printf("\n");
}


int _charArrayToInt(char arr[3]) {
    char temp[4]={arr[0],arr[1],arr[2],0};
    int* fptr = (int*)&temp[0];
    return *fptr;
}

//其实也可以直接这样套一下 *((float*)byteArray)
float charArrayToFloat(char arr[4]) {
    // 将char数组的地址强制转换为float指针，并解引用以获取float值
    float* fptr = (float*)&arr[0];
    return *fptr;
}

short charArrayToShort(char arr[2]) {
    short* fptr = (short*)&arr[0];
    return *fptr;
}


union converter_ushort {
    unsigned short short_value;
    unsigned char byte_value[2];
} ushort_data;

union converter_int {
    unsigned int int_value;
    unsigned char byte_value[4];
} int_data;


void write_uint16_big_endian(unsigned char *buffer,unsigned short value) {
    ushort_data.short_value = value;
    buffer[0] = ushort_data.byte_value[1]; // 写入高位字节
    buffer[1] = ushort_data.byte_value[0];// 写入低位字节
}

short read_uint16_big_endian(unsigned char *buffer) {
    ushort_data.byte_value[0] = buffer[1]; // 读取高位字节
    ushort_data.byte_value[1] = buffer[0];// 读取低位字节
    return ushort_data.short_value;
}


//去掉头尾空白
void trim_whitespace_simple(char *str) {
    char *end, *start = str;

    // 去除首部的空格
    while (isspace((unsigned char)*str)) str++;

    // 去除尾部的空格
    if (*str) { // 如果字符串非空
        end = str + strlen(str) - 1;
        while (end > str && isspace((unsigned char)*end)) end--;
        *(end + 1) = 0; // 在字符串末尾添加空字符
    }

    // 将非空格的字符串部分移动到起始位置
    if (start != str) {
        while (*str) {
            *start++ = *str++;
        }
        *start = 0;
    }
}

//字符串分割函数
void splitString(char *str, const char *delimiter,char **arg) 
{

    trim_whitespace_simple(str);
    char *token = strtok(str, delimiter);
    int i=0;
    while (token != NULL) 
    {
        arg[i++]=token;
        token = strtok(NULL, delimiter);
    }
    arg[i]=NULL;
}

short stringToShort(const char *str) {
    short num = 0;
    int sign = 1; // 默认为正数

    // 检查是否以'-'或'+'开头
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    // 转换数字
    while (*str >= '0' && *str <= '9') {
        num = num * 10 + (*str - '0');
        if (sign == 1 && num > SHRT_MAX) {
            // 如果超出正数范围，返回最大值
            return SHRT_MAX;
        }
        if (sign == -1 && num > (SHRT_MAX + 1)) {
            // 如果超出负数范围，返回最小值
            return SHRT_MIN;
        }
        str++;
    }

    // 返回时考虑符号
    return sign * num;
}

int string_to_short_arr(char *str,short *arr){
    char *arg[256];
    splitString(str,"/",arg);
    int i=0;
    while(arg[i]!=NULL){
        arr[i]=stringToShort(arg[i]);
        i++;
    }
    return i;
}