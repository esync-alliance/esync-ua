/**
 * Copyright @ 2017 - 2018 Excelfore(Shanghai) Co., Ltd.
 * All Rights Reserved.
 */
#ifndef BASE64_H
#define BASE64_H

#ifdef __cplusplus
extern "C" {
#endif

void print_buffer(void* buff, unsigned int len);
int base64_encode_v(const char* sourcedata, char* base64);
int base64_decode(const char* base64, char* dedata);
int base64_decode_size(int size);


#ifdef __cplusplus
}
#endif

#endif /* BASE64_H */
