#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "converter.h"


int main(){
    char buffer[1024]="hello world,god bless you";
    char *words[20];
    char *p=buffer;
    int i=0;
    words[i++]=buffer;
    while (*p!=0)
    {
        if(((unsigned char)*p)==' '){
            words[i++]=p+1;
            *p=0;
        }else if(((unsigned char)*p)==','){
            *p=0;
            while(i>0){
                printf("%s ",words[i-1]);
                i--;
            }
            printf(",");
            i=0;
            words[i++]=p+1;
        }
        p++;
    }
    while(i>0){
        printf("%s ",words[i-1]);
        i--;
    }
}


// int main(){
//     char buffer[1024]="hello world,god bless you";
//     char *words[20];
//     char *douhao[10];
//     char *p=buffer;
//     int i=0;
//     int j=0;
//     words[i++]=buffer;
//     while (*p!=0)
//     {
//         printf("char=%c\n",(unsigned char)*p);
//         if(((unsigned char)*p)==' '){
//             printf("´¥·¢¿Õ¸ñ\n");
//             words[i++]=p+1;
//             *p=0;
//         }else if(((unsigned char)*p)==','){
//             printf("´¥·¢¶ººÅ\n");
//             douhao[j++]=p;
//             words[i++]=p+1;
//             *p=0;
//         }
//         p++;
//     }
//     printf("%s\n",words[i-1]);
//     while(i>0){
//         if(j>0){
//             if(words[i-1]>douhao[j-1]){
//                 printf("%s ",words[i-1]);
//             }else{
//                 printf(",%s ",words[i-1]);
//                 j--;
//             }
//         }else{
//             printf("%s ",words[i-1]);
//         }
//         i--;
//     }
    
// }

