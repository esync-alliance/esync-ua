/*
 * Crc32.h
 *
 *  Created on: Nov 29, 2018
 *      Author: lilo
 */

#ifndef CRC32_H_
#define CRC32_H_

#ifdef __cplusplus
extern "C" {
#endif

#define CRC32_INIT_VALUE 0xFFFFFFFF

unsigned int buf_crc32(const unsigned char* pDataBuf, unsigned int uiLen, unsigned int uiOldCrc32);
unsigned int one_time_CRC32(unsigned char* pDataBuf, unsigned int uiLen, unsigned int uiOldCrc32);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CRC32_H_ */
