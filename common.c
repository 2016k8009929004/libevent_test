#include "common.h"

int itoa(int n, char s[]){
    int i, j, sign;

    if(n < 0){
        sign = 1;
    }else{
        sign = 0;
    }

    i = 0;
    do{
        s[i++] = n % 10 + '0';
    }while((n / 10) > 0);

    if(sign == 1){
        s[i++] = '-';
    }

    for(j = 0;j < i/2;j++){
        SWAP(s[j], s[i - j]);
    }

    s[i] = '\0';

    return 0;
}