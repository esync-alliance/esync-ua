/**
 * Copyright @ 2017 - 2018 Excelfore(Shanghai) Co., Ltd.
 * All Rights Reserved.
 */
#ifndef BASE64_H
#   include "base64.h"
#endif

#include <stdio.h>
#include <string.h>
#include <linux/limits.h>

const char * base64char = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const char padding_char = '=';

void print_buffer(void* buff, unsigned int len)
{
    unsigned char* p;
    char szHex[3*16+1];
    unsigned int i;
    int idx;
    if (!buff || 0 == len) {
        return;
    }

    p = (unsigned char*)buff;

    printf("-----------------begin-------------------\n");
    for (i=0; i<len; ++i) {
        idx = 3 * (i % 16);
        if (0 == idx) {
            memset(szHex, 0, sizeof(szHex));
        }

        snprintf(&szHex[idx], 4, "%02x ", p[i]);
        
        if (0 == ((i+1) % 16)) {
            printf("%s\n", szHex);
        }
    }

    if (0 != (len % 16)) {
        printf("%s\n", szHex);
    }
    printf("------------------end-------------------\n");
}

int num_strchr(const char *str, char c)
{
    const char *pindex = strchr(str, c);
    if (NULL == pindex){
        return -1;
    }
    return pindex - str;
}

int base64_encode_v(const char * sourcedata, char * base64)
{
    int i=0, j=0;
    unsigned char trans_index=0;
    const int datalength = strlen((const char*)sourcedata);
    
    for (; i < datalength; i += 3){
        trans_index = ((sourcedata[i] >> 2) & 0x3f);
        base64[j++] = base64char[(int)trans_index];
        trans_index = ((sourcedata[i] << 4) & 0x30);
        if (i + 1 < datalength){
            trans_index |= ((sourcedata[i + 1] >> 4) & 0x0f);
            base64[j++] = base64char[(int)trans_index];
        }else{
            base64[j++] = base64char[(int)trans_index];
            base64[j++] = padding_char;
            base64[j++] = padding_char;
            break;
        }

        trans_index = ((sourcedata[i + 1] << 2) & 0x3c);
        if (i + 2 < datalength){
            trans_index |= ((sourcedata[i + 2] >> 6) & 0x03);
            base64[j++] = base64char[(int)trans_index];

            trans_index = sourcedata[i + 2] & 0x3f;
            base64[j++] = base64char[(int)trans_index];
        }
        else{
            base64[j++] = base64char[(int)trans_index];

            base64[j++] = padding_char;

            break;
        }
    }

    base64[j] = '\0'; 
    return 0;
}

int base64_decode(const char * base64, char * dedata)
{
    int i = 0, j=0;
    int trans[4] = {0,0,0,0};
    for (;base64[i]!='\0';i+=4){
        trans[0] = num_strchr(base64char, base64[i]);
        trans[1] = num_strchr(base64char, base64[i+1]);
        dedata[j++] = ((trans[0] << 2) & 0xfc) | ((trans[1]>>4) & 0x03);

        if (base64[i+2] == '='){
            continue;
        }
        else{
            trans[2] = num_strchr(base64char, base64[i + 2]);
        }
        dedata[j++] = ((trans[1] << 4) & 0xf0) | ((trans[2] >> 2) & 0x0f);

        if (base64[i + 3] == '='){
            continue;
        }
        else{
            trans[3] = num_strchr(base64char, base64[i + 3]);
        }

        dedata[j++] = ((trans[2] << 6) & 0xc0) | (trans[3] & 0x3f);
    }

    dedata[j] = '\0';
    return j;
}

int base64_decode_size(int size)
{
    return (size +3) / 4 * 3;
}

/* EOF */

