#include <stdio.h>

// 函数用于将四个char组成的字节数组转换为float
float charArrayToFloat(char arr[4]) {
    // 将char数组的地址强制转换为float指针，并解引用以获取float值
    float* fptr = (float*)&arr[0];
    return *fptr;
}

int main() {
    // 示例：假设我们有一个float值已经以字节形式给出
    // 注意：这里直接以字节形式表示float值，需要确保这些字节与你的系统的字节序（大端或小端）一致
    // 例如，这里我们使用了一个具体的float值（例如3.14）的字节表示（在特定系统上）
    // 注意：这里的字节值仅为示例，实际值会根据你的系统和编译器有所不同
    char byteArray[4] = {0x8f, 0xc2, 0x21, 0x41}; 

    // 调用函数进行转换
    float value = charArrayToFloat(byteArray);

    // 打印结果
    printf("The float value is: %f\n", value);
    printf("The float value is: %f\n", *((float*)byteArray));
    return 0;
}