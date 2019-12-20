/* Copyright Excelfore Corporation, - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Proprietary and confidential.
 * Its use or disclosure, in whole or in part, without
 * written permission of Excelfore Corp. is prohibited.
 *
 * ua_downlaod.c
 *
 */

#include "ua_download.h"
#include "debug.h"
#include "handler.h"
#include "base64.h"
#include "utils.h"
#include <unistd.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <sys/stat.h>

#ifndef CRC32_H_
#   include "Crc32.h"
#endif

#define DATA_FOLDER_MODE 0755
static ua_dl_context_t* ua_dlc = 0;
static char ua_dl_filename_buffer[PATH_MAX];
extern ua_internal_t ua_intl;

static void ua_dl_release(ua_dl_context_t* dlc);
static void ua_dl_init_dl_rec(ua_dl_record_t* dl_rec);
static int ua_dl_calc_file_crc32(FILE* fd, int len, unsigned int* crc32);
static int ua_dl_init(pkg_info_t* pkgInfo, ua_dl_context_t** dlc);
static int dmc_pre_download_cb(struct dmclient_download_context const* ddc);
static int dmc_recv_cb(struct dmclient_download_context const* ddc, void const* data, size_t len);
static int ua_dl_save_dl_rc(ua_dl_context_t* dlc);
static int ua_dl_step_download(ua_dl_context_t* dlc);
static int ua_dl_step_encrypt(ua_dl_context_t* dlc);
static int ua_dl_step_verify(ua_dl_context_t* dlc);
static int ua_dl_step_done(ua_dl_context_t* dlc);
static int ua_dl_step_action(ua_dl_context_t* dlc);
static int ua_dl_save_data(ua_dl_context_t* dlc, const char* data, size_t len);
static void trigger_session_request();

static void ua_dl_release(ua_dl_context_t* dlc)
{
    if (!dlc) {
        return;
    }

    f_free(dlc->name);
    dlc->name = 0;
    f_free(dlc->version);
    dlc->version = 0;

    f_free(dlc->dl_pkg_filename);
    dlc->dl_pkg_filename = 0;
    f_free(dlc->dl_rec_filename);
    dlc->dl_rec_filename = 0;
    f_free(dlc->dl_encrytion_filename);
    dlc->dl_encrytion_filename = 0;
    f_free(dlc->dl_encrytion_filename);
    dlc->dl_encrytion_filename = 0;

    //f_free(dlc->dl_info.error);   // free bug
    dlc->dl_info.error = 0;
    
    free(dlc);
}

static void ua_dl_init_dl_rec(ua_dl_record_t* dl_rec)
{
    DBG("====ua_dl_init_dl_rec====");

    dl_rec->bytes_written = 0;
    dl_rec->step = UA_DL_STEP_NONE;
    dl_rec->crc32 = 0;
    dl_rec->last_bytes_written = 0;
    dl_rec->last_crc32 = 0;
    dl_rec->e_tag_valid = 0;
    dl_rec->e_tag[0] = '\0';
}

static int ua_dl_calc_file_crc32(FILE* fd, int len, unsigned int* crc32)
{
    char buf[1024];
    int read_len = 0;
    unsigned int tmp_crc = 0;
    int remain = len;
    int tmplen;
    if (!fd || len <= 0) {
        return E_UA_ERR;
    }
    
    fseek(fd, 0, SEEK_SET);
    *crc32 = 0;

    for (;;) {
        if (remain <= 0) {
            break;
        }
        read_len = (remain <= 1024)?remain:1024;
        remain -= read_len;

        tmplen = fread(buf, 1, read_len, fd);
        if (read_len != tmplen) {
            return E_UA_ERR;
        }

        tmp_crc = buf_crc32((const unsigned char *)buf, (unsigned int)read_len, tmp_crc);
    }

    *crc32 = tmp_crc;
    return E_UA_OK;
}

static int ua_dl_init(pkg_info_t* pkgInfo, ua_dl_context_t** dlc)
{
    ua_dl_context_t* tmp_dlc = 0;
    char tmp_filename[PATH_MAX];
    FILE* rec_fd = 0;
    FILE* data_fd = 0;
    unsigned int crc32 = 0;
    if (!pkgInfo || !dlc) {
        return E_UA_ERR;
    }

    DBG("====ua_dl_init====");

    snprintf(tmp_filename, PATH_MAX, "%s/%s", ua_intl.ua_dl_dir, pkgInfo->name);
    if (0 != access(tmp_filename, F_OK)) {
        if (0 != mkdir(tmp_filename, DATA_FOLDER_MODE)) {
            DBG("mkdir %s error \n", tmp_filename);
            return E_UA_ERR;
        }
    }

    snprintf(tmp_filename, PATH_MAX, "%s/%s/%s", ua_intl.ua_dl_dir, pkgInfo->name, pkgInfo->version);
    if (0 != access(tmp_filename, F_OK)) {
        if (0 != mkdir(tmp_filename, DATA_FOLDER_MODE)) {
            DBG("mkdir %s error \n", tmp_filename);
            return E_UA_ERR;
        }
    }

    tmp_dlc = (ua_dl_context_t*)malloc(sizeof(ua_dl_context_t));
    memset(tmp_dlc, 0, sizeof(ua_dl_context_t));
    if (!tmp_dlc) {
        DBG("malloc error \n");
        return E_UA_ERR;
    }

    snprintf(tmp_filename, PATH_MAX, "%s/%s/%s.x", 
            pkgInfo->name, pkgInfo->version, 
            pkgInfo->version);
    tmp_dlc->dl_pkg_filename = JOIN(ua_intl.ua_dl_dir, tmp_filename);

    snprintf(tmp_filename, PATH_MAX, "%s/%s/%s.dlr",
            pkgInfo->name, pkgInfo->version,
            pkgInfo->version);
    tmp_dlc->dl_rec_filename = JOIN(ua_intl.ua_dl_dir, tmp_filename);

    snprintf(tmp_filename, PATH_MAX, "%s/%s/%s.e",
            pkgInfo->name, pkgInfo->version,
            pkgInfo->version);
    tmp_dlc->dl_encrytion_filename = JOIN(ua_intl.ua_dl_dir, tmp_filename);
    strcpy(ua_dl_filename_buffer, tmp_dlc->dl_encrytion_filename);
    ua_intl.ua_downloaded_filename = ua_dl_filename_buffer;

    snprintf(tmp_filename, PATH_MAX, "%s/%s/%s.x.z",
            pkgInfo->name, pkgInfo->version,
            pkgInfo->version);
    tmp_dlc->dl_zip_folder = JOIN(ua_intl.ua_dl_dir, tmp_filename);

    // No record file
    if (0 != access(tmp_dlc->dl_rec_filename, F_OK)) {
        rec_fd = fopen(tmp_dlc->dl_rec_filename, "w+");
        if (!rec_fd) {
            DBG("fopen error [%s] \n", tmp_dlc->dl_rec_filename);
            ua_dl_release(tmp_dlc);
            return E_UA_ERR;
        }

        ua_dl_init_dl_rec(&tmp_dlc->dl_rec);
        fwrite(&tmp_dlc->dl_rec, 1, sizeof(tmp_dlc->dl_rec), rec_fd);
        fclose(rec_fd);
        remove(tmp_dlc->dl_pkg_filename);
        DBG("No record file, init_dl_rec");
    } else {
        rec_fd = fopen(tmp_dlc->dl_rec_filename, "a+");
        if (!rec_fd) {
            DBG("fopen error [%s] \n", tmp_dlc->dl_rec_filename);
            ua_dl_release(tmp_dlc);
            return E_UA_ERR;
        }
        fseek(rec_fd, 0, SEEK_SET);

        if (sizeof(tmp_dlc->dl_rec) != fread(&tmp_dlc->dl_rec, 1, sizeof(tmp_dlc->dl_rec), rec_fd)) {
            ua_dl_init_dl_rec(&tmp_dlc->dl_rec);

            fseek(rec_fd, 0, SEEK_SET);
            fwrite(&tmp_dlc->dl_rec, 1, sizeof(tmp_dlc->dl_rec), rec_fd);
            fclose(rec_fd);
            remove(tmp_dlc->dl_pkg_filename);
            DBG("Can't read fread record file, init_dl_rec");
        } else {
            data_fd = fopen(tmp_dlc->dl_pkg_filename, "r");
            if (!data_fd) {
                ua_dl_init_dl_rec(&tmp_dlc->dl_rec);

                fseek(rec_fd, 0, SEEK_SET);
                fwrite(&tmp_dlc->dl_rec, 1, sizeof(tmp_dlc->dl_rec), rec_fd);
                fclose(rec_fd);
                remove(tmp_dlc->dl_pkg_filename);
                DBG("Can't open pkg file, init_dl_rec");
            } else {
                // drop the last package
                if (tmp_dlc->dl_rec.last_bytes_written != tmp_dlc->dl_rec.bytes_written
                    || tmp_dlc->dl_rec.crc32 != tmp_dlc->dl_rec.last_crc32) {
                    DBG("drop the last package last data[%d]->[%d]",  
                        tmp_dlc->dl_rec.bytes_written, tmp_dlc->dl_rec.last_bytes_written);
                    tmp_dlc->dl_rec.bytes_written = tmp_dlc->dl_rec.last_bytes_written;
                    tmp_dlc->dl_rec.crc32 = tmp_dlc->dl_rec.last_crc32;
                    ua_dl_save_dl_rc(tmp_dlc);
                }
                
                if (E_UA_OK != ua_dl_calc_file_crc32(data_fd, tmp_dlc->dl_rec.bytes_written, &crc32)
                    || crc32 != tmp_dlc->dl_rec.crc32) {
                    ua_dl_init_dl_rec(&tmp_dlc->dl_rec);

                    fseek(rec_fd, 0, SEEK_SET);
                    fwrite(&tmp_dlc->dl_rec, 1, sizeof(tmp_dlc->dl_rec), rec_fd);
                    fclose(rec_fd);
                    fclose(data_fd);
                    remove(tmp_dlc->dl_pkg_filename);
                    DBG("Crc incorrect, init_dl_rec");
                } else {
                    if (UA_DL_STEP_DOWNLOADED < tmp_dlc->dl_rec.step) {
                        tmp_dlc->dl_rec.step = UA_DL_STEP_DOWNLOADED;
                    }

                    fseek(rec_fd, 0, SEEK_SET);
                    fwrite(&tmp_dlc->dl_rec, 1, sizeof(tmp_dlc->dl_rec), rec_fd);
                    fclose(rec_fd);

                    fclose(data_fd);
                    truncate(tmp_dlc->dl_pkg_filename, tmp_dlc->dl_rec.bytes_written);
                    DBG("Continue download and do truncate [%d]", tmp_dlc->dl_rec.bytes_written);
                }
            }
        }
    }

    tmp_dlc->pkg_info = pkgInfo;
    tmp_dlc->ca_file = ua_intl.ua_dl_ca_file;
    tmp_dlc->dl_info.total_bytes = pkgInfo->vi.downloadable.length;
    tmp_dlc->dl_info.downloaded_bytes = tmp_dlc->dl_rec.bytes_written;
    tmp_dlc->dl_info.expected_bytes = pkgInfo->vi.downloadable.length;
    tmp_dlc->dl_info.no_download = 0;
    tmp_dlc->dl_info.completed_download = 0;

    *dlc = tmp_dlc;
    return E_UA_OK;
}

static int dmc_pre_download_cb(struct dmclient_download_context const* ddc)
{
    return XL4_DME_OK;
}

#if 0
static void print_buffer2(const void* buf, uint32_t len)
{
    unsigned char* p;
    char szHex[3*16+1];
    unsigned int i;
    int idx;
    if (NULL == buf || 0 == len) {
        return;
    }

    p = (unsigned char*)buf;

    DBG("-----------------begin-------------------len[%d]\n", len);
    for (i=0; i<len; ++i) {
        idx = 3 * (i % 16);
        if (0 == idx) {
            memset(szHex, 0, sizeof(szHex));
        }

        snprintf(&szHex[idx], 4, "%02x ", p[i]);
        
        if (0 == ((i+1) % 16)) {
            DBG("%s\n", szHex);
        }
    }

    if (0 != (len % 16)) {
        DBG("%s\n", szHex);
    }
    DBG("------------------end-------------------\n");
}


FILE* tmp_fp = 0;
char tmp_data[1024*1024] = {0};
static void do_memcmp(int pos, int len, const char* data, char* name)
{
    if (!tmp_fp) {
        tmp_fp = fopen("/cache/IVI-7901010-DD01-0x0020/1007", "r");
    }

    if (!tmp_fp) {
        DBG("-----------------Open /cache/IVI-7901010-DD01-0x0020/1007 error-------\n");
        return;
    }

    fseek(tmp_fp, pos, SEEK_SET);
    fread(tmp_data, len, 1, tmp_fp);

    if (0 == memcmp(data, tmp_data, len)) {
        DBG("----YANYEXING-------------mem the same---offset[%d]len[%d]----\n", pos, len);
    } else {
        DBG("----YANYEXING-----!!!!!!!!!!!!!!!!!--------mem not the same---!!!!!!!!!!!!!!!-offset[%d]len[%d]---\n", pos, len);        
        print_buffer2(data, len);
        print_buffer2(tmp_data, len);
    }

    {
         FILE* fp = 0;      
         fp = fopen(name, "r");
         if (!tmp_fp) {
             DBG("-----------------Open %s error-------\n", name);
             return;
         }
         fseek(fp, pos, SEEK_SET);
         fread(tmp_data, len, 1, fp);
         fclose(fp);

         if (0 == memcmp(data, tmp_data, len)) {
             DBG("----YANYEXING-------------file the same---offset[%d]len[%d]----\n", pos, len);
         } else {
             DBG("----YANYEXING-----!!!!!!!!!!!!!!!!!--------file not the same---!!!!!!!!!!!!!!!-offset[%d]len[%d]---\n", pos, len);        
             print_buffer2(data, len);
             print_buffer2(tmp_data, len);
         }
    }
}
#endif

static int dmc_recv_cb(struct dmclient_download_context const* ddc, void const* data, size_t len)
{
    int rc = XL4_DME_OK;

    if (!ddc || !data || !ddc->download) {
        DBG("NULL pointer(s) ddc(%p), data(%p), ddc->download(%p)", ddc, data, ddc->download);
        return XL4_DME_SYS;
    }

    DBG("dmc_recv_cb len[%d] http_code[%d]bytes_downloaded[%llu]e_tag[%s]content_byte_offset[%llu/%llu]content_length[%llu]result[%d]", 
            len, 
            ddc->http_code, ddc->bytes_downloaded, ddc->e_tag, ddc->content_byte_offset,
            ddc->download->content_byte_offset,
            ddc->content_length, ddc->result);

    ua_dl_context_t* dlc = (ua_dl_context_t* )ddc->download->user_context;
    pkg_info_t* pkg_info = dlc->pkg_info;

    if (ddc->download->content_byte_offset != ddc->content_byte_offset) {
        DBG("content_byte_offset changed [%llu]->[%llu]etag[%s]->[%s]", ddc->download->content_byte_offset, ddc->content_byte_offset, 
            ddc->download->e_tag, ddc->e_tag);
        FILE* data_fd = 0;
        dlc->dl_info.downloaded_bytes -= (ddc->download->content_byte_offset - ddc->content_byte_offset);
        ddc->download->content_byte_offset = ddc->content_byte_offset;
        
        dlc->dl_rec.bytes_written = ddc->content_byte_offset;
        dlc->dl_rec.last_bytes_written = ddc->content_byte_offset;

        dlc->dl_rec.crc32 = 0;
        dlc->dl_rec.last_crc32 = 0;
        truncate(dlc->dl_pkg_filename, dlc->dl_rec.bytes_written);

        data_fd = fopen(dlc->dl_pkg_filename, "r");
        if (data_fd) {
            if (E_UA_OK != ua_dl_calc_file_crc32(data_fd, dlc->dl_rec.bytes_written, &dlc->dl_rec.crc32)) {
                rc = XL4_DME_SYS;
                DBG("Calc CRC failed [%s][%llu]", dlc->dl_pkg_filename, dlc->dl_rec.bytes_written);
            }
            
            dlc->dl_rec.last_crc32 = dlc->dl_rec.crc32;
            fclose(data_fd);
        } else {
            rc = XL4_DME_SYS;
            DBG("Can't open file [%s]", dlc->dl_pkg_filename);
        }

        ua_dl_save_dl_rc(dlc);
        if (rc != XL4_DME_OK) {
            DBG("Return");
            return rc;
        }
    }

    if (ddc->result == XL4_DME_OK && dlc && pkg_info) {
        
        if (data && len > 0 && ua_dl_save_data(dlc, data, len) == E_UA_OK) {
            //dlc->dl_info.total_bytes        = ddc->content_length;
            dlc->dl_info.downloaded_bytes   += len;
            dlc->dl_info.no_download        = 0;
            dlc->dl_info.completed_download = (dlc->dl_info.downloaded_bytes >= dlc->dl_info.total_bytes) ? 1 : 0;
            dlc->dl_info.error              = NULL;

            dlc->bytes_to_report     += len;

            dlc->dl_rec.last_bytes_written = dlc->dl_rec.bytes_written;
            dlc->dl_rec.bytes_written = dlc->dl_info.downloaded_bytes;
            //dlc->dl_info.expected_bytes = ddc->content_length;
            if (ddc->e_tag) {
                dlc->dl_rec.e_tag_valid = 1;
                strncpy(dlc->dl_rec.e_tag, ddc->e_tag, E_TAG_MAX_SIZE);
            } else {
                memset(dlc->dl_rec.e_tag, 0, E_TAG_MAX_SIZE);
                dlc->dl_rec.e_tag_valid = 0;
            }

            DBG("====dmc_recv_cb=len[%d]bytes_written[%d]downloaded[%llu]content_length[%llu]http_code[%d]===", 
                len , dlc->dl_rec.bytes_written, ddc->bytes_downloaded, ddc->content_length, ddc->http_code);
            // do_memcmp(dlc->dl_info.downloaded_bytes - len, len, data, dlc->dl_pkg_filename);

            dlc->dl_rec.last_crc32 = dlc->dl_rec.crc32;
            dlc->dl_rec.crc32 = buf_crc32((const unsigned char *)data, len, dlc->dl_rec.crc32);
            ua_dl_save_dl_rc(dlc);

            if (dlc->bytes_to_report >= UA_DL_REPORT_BLK_SIZE || dlc->dl_info.completed_download) {
                if (send_dl_report(pkg_info, dlc->dl_info, 0) != E_UA_OK) {
                    DBG("Failed to send dl err report");
                }
                dlc->bytes_to_report = 0;
            }

            if (dlc->dl_info.completed_download) {
                DBG("UA DL COMPLETED");
                dlc->bytes_to_report = 0;
            }

            rc = XL4_DME_OK;
        } else {
            rc = XL4_DME_SYS;
            DBG("Data recv failed");
        }
    }else {
        DBG("DL ERR: result: %d, http_code: %d", ddc->result, ddc->http_code);
        if (dlc && pkg_info) {
            dlc->dl_info.error = "DLE_DOWNLOAD";
            if (send_dl_report(pkg_info, dlc->dl_info, 0) != E_UA_OK) {
                DBG("Failed to send dl err report");
            }
        }else {
            DBG("NULL pointer(s) dlc(%p), pkg_info(%p)", dlc, pkg_info);
        }

        rc = ddc->result;
    }

    return rc;
}

static int ua_dl_save_dl_rc(ua_dl_context_t* dlc)
{
    FILE* fp;
    if (!dlc) {
        return E_UA_ERR;
    }

    fp = fopen(dlc->dl_rec_filename, "w+");
    if (!fp) {
        return E_UA_ERR;
    }

    fwrite(&dlc->dl_rec, 1, sizeof(dlc->dl_rec), fp);
    fclose(fp);

    return E_UA_OK;
}

static int ua_dl_step_download(ua_dl_context_t* dlc)
{
    int rc = E_UA_OK;
    dmclient_download_context_t* ddc = 0;
    dmclient_download_t dd           = {0};

    if (!dlc) {
        return E_UA_ERR;
    }

    dd.user_context = (void*)dlc;
    dd.url = dlc->pkg_info->vi.downloadable.url;

    dd.connect_timeout_ms  = ua_intl.ua_dl_connect_timout_ms;
    dd.download_timeout_ms = ua_intl.ua_dl_download_timeout_ms;
    dd.ca_file             = ua_intl.ua_dl_ca_file;
    dd.f_receive           = dmc_recv_cb;
    dd.e_tag               = dlc->dl_rec.e_tag;
    dd.f_pre_download      = dmc_pre_download_cb;
    dd.content_byte_offset = dlc->dl_rec.bytes_written;

    DBG("===ua_dl_step_download=offset[%d]==", dd.content_byte_offset);

    if (dmclient_download(&dd, &ddc) == XL4_DME_OK && ddc->result == XL4_DME_OK
        && (200 == ddc->http_code || 206 == ddc->http_code)) {
        DBG("UA download completed http_code[%d]", ddc->http_code);
        dlc->dl_rec.last_bytes_written = dlc->dl_rec.bytes_written;
        dlc->dl_rec.last_crc32 = dlc->dl_rec.crc32;
        dlc->dl_rec.step = UA_DL_STEP_DOWNLOADED;
        if (E_UA_OK != ua_dl_save_dl_rc(dlc)) {
            DBG("ua_dl_save_dl_rc error");
            rc = E_UA_ERR;
        }
    } else {
        DBG("ERR: UA download failed truncate last data[%d]->[%d]http_code[%d]", 
            dlc->dl_rec.bytes_written, dlc->dl_rec.last_bytes_written, ddc->http_code);
        dlc->dl_info.downloaded_bytes = dlc->dl_rec.last_bytes_written;
        dlc->dl_rec.bytes_written = dlc->dl_rec.last_bytes_written;
        dlc->dl_rec.crc32 = dlc->dl_rec.last_crc32;
        if (E_UA_OK != ua_dl_save_dl_rc(dlc)) {
            DBG("ua_dl_save_dl_rc error");
            rc = E_UA_ERR;
        }
        truncate(dlc->dl_encrytion_filename, dlc->dl_rec.bytes_written);
        rc = E_UA_ERR;

        trigger_session_request();
    }

    dmclient_download_release(ddc);
    return rc;
}

static int ua_dl_step_encrypt(ua_dl_context_t* dlc)
{
    int ret = E_UA_OK;
    char* key_decode;
    int key_len;
    int file_size;
    
    if (!dlc) {
        return E_UA_ERR;
    }

    if (0 != f_copy(dlc->dl_pkg_filename, dlc->dl_encrytion_filename)) {
        DBG("f_copy failed");
        return E_UA_ERR;
    }

    if (0 != f_size(dlc->dl_encrytion_filename, &file_size)) {
        DBG("f_size failed");
        return E_UA_ERR;
    }

    key_decode = f_malloc(base64_decode_size(strlen(dlc->pkg_info->vi.encryption.key)) + 32);
    key_len = base64_decode(dlc->pkg_info->vi.encryption.key, key_decode);

    if(XL4_DME_OK != dmclient_decrypt_binary(dlc->dl_encrytion_filename, file_size, 
        dlc->pkg_info->vi.encryption.method, 
        (void* )key_decode, 
        key_len, 0)) {              
        DBG("ERR: decrypt binary failed");
        ret = E_UA_ERR;

        dlc->dl_info.error = "DLE_VERIFY";
        if (send_dl_report(dlc->pkg_info, dlc->dl_info, 0) != E_UA_OK) {
            DBG("Failed to send dl err report");
        }
    } else {
        DBG("dmclient_decrypt_binary completed");
        dlc->dl_rec.step = UA_DL_STEP_ENCRYPTED;
        if (E_UA_OK != ua_dl_save_dl_rc(dlc)) {
            DBG("ua_dl_save_dl_rc error");
            ret = E_UA_ERR;
        }
    }

    if(ret == E_UA_OK) {
        truncate(dlc->dl_encrytion_filename, dlc->pkg_info->vi.downloadable.length);
    }

    return ret;
}

// Defines function to use for custom certificate validation
// Nothing to do, just return XL4_DME_OK (0)
static int ua_dl_dmclient_cert_ck_f(void * context,
        dmclient_certificate_chain_t * chain, char ** error)
{
    return 0;
}

static int ua_dl_step_verify(ua_dl_context_t* dlc)
{
    DBG("ua_dl_step_verify enter");
    int ret = E_UA_ERR;
    int i = 0;

    if (!dlc) {
        DBG("ua_dl_step_verify exit E_UA_ERR");
        return E_UA_ERR;
    }

    DBG("ua_dl_setp_verify dlc->dl_encrytion_filename=%s", dlc->dl_encrytion_filename);
    for(i = 0; i < MAX_VERIFY_CA_COUNT; i++) {
        if (!ua_intl.verify_ca_file[i]) {
            break;
        }
            
        DBG("ua_dl_setp_verify dmclient_verify_binary() cnt=%d", i);
        if (XL4_DME_OK == dmclient_verify_binary(
            dlc->dl_encrytion_filename,
            ua_intl.verify_ca_file[i],
            ua_dl_dmclient_cert_ck_f, 0, 0, NULL)) {
            DBG("ua_dl_setp_verify verify binary OK. cnt=%d", i);
            dlc->dl_rec.step = UA_DL_STEP_VERIFIED;
            if (E_UA_OK != ua_dl_save_dl_rc(dlc)) {
                DBG("ua_dl_step_verify error");
                ret = E_UA_ERR;
            } else {
                ret = E_UA_OK;
            }
            break;
        } else {
            DBG("ua_dl_setp_verify verify binary fail. cnt=%d", i);
            continue;
        }
    }

    if(ret == E_UA_OK) {
        DBG("ua_dl_setp_verify f_remove_dir");
        f_remove_dir(dlc->dl_zip_folder);
    } else {
        dlc->dl_info.error = "DLE_VERIFY";
        if (send_dl_report(dlc->pkg_info, dlc->dl_info, 0) != E_UA_OK) {
            DBG("Failed to send dl err report");
        }
    }
    remove(dlc->dl_pkg_filename);

    DBG("ua_dl_step_verify exit. ret=%d", ret);
    return ret;
}

static int ua_dl_step_done(ua_dl_context_t* dlc)
{
    if (!dlc) {
        return E_UA_ERR;
    }

    dlc->dl_info.total_bytes        = dlc->dl_rec.bytes_written;
    dlc->dl_info.downloaded_bytes   = dlc->dl_rec.bytes_written;
    dlc->dl_info.no_download        = 0;
    dlc->dl_info.completed_download = 1;
    dlc->dl_info.error              = NULL;
                
    return send_dl_report(dlc->pkg_info, dlc->dl_info, 1);
}

static int ua_dl_step_action(ua_dl_context_t* dlc)
{
    if (!dlc) {
        return E_UA_ERR;
    }

    switch (dlc->dl_rec.step) {
        case UA_DL_STEP_NONE:
            {
                if (E_UA_OK != ua_dl_step_download(dlc)) {
                    return E_UA_ERR;
                }

                return ua_dl_step_action(dlc);
            }
            break;
            
        case UA_DL_STEP_DOWNLOADED:
            {
                if (E_UA_OK != ua_dl_step_encrypt(dlc)) {
                        return E_UA_ERR;
                }

                return ua_dl_step_action(dlc);
            }
            break;
            
        case UA_DL_STEP_ENCRYPTED:
            {
                if (E_UA_OK != ua_dl_step_verify(dlc)) {
                        return E_UA_ERR;
                }

                return ua_dl_step_action(dlc);
            }
            break;
            
        case UA_DL_STEP_VERIFIED:
        case UA_DL_STEP_DONE:
            {
                return ua_dl_step_done(dlc);
            }
            break;
        default:
            {
                return E_UA_ERR;
            }
            break;
    };

    return E_UA_ERR;
}

int ua_dl_start_download(pkg_info_t* pkgInfo)
{
    int ret = E_UA_OK;
    if (ua_dlc) {
        if (0 != strcmp(ua_dlc->version, pkgInfo->version)
            || 0 != strcmp(ua_dlc->name, pkgInfo->name)) {
            ua_dl_release(ua_dlc);
            ua_dlc = 0;
        } else {
            return E_UA_OK;
        }
    }

    DBG("====ua_dl_start_download====");

    if (E_UA_OK != ua_dl_init(pkgInfo, &ua_dlc)) {
        return E_UA_ERR;
    }

    send_query_trust();
    ret = ua_dl_step_action(ua_dlc);
    ua_dl_release(ua_dlc);
    ua_dlc = 0;

    return ret;
}

static int ua_dl_save_data(ua_dl_context_t* dlc, const char* data, size_t len)
{
    int err  = E_UA_OK;
    FILE* fd = NULL;

    if (!dlc || !data || 0 == len) {
        return E_UA_ERR;
    }

    if ((fd = fopen(dlc->dl_pkg_filename, "a"))) {
        if (len != fwrite(data, 1, len, fd)) {
            err = E_UA_ERR;
        }

        fclose(fd);
    } else {
        DBG("Error open file %s", dlc->dl_pkg_filename);
        err = E_UA_ERR;
    }

    return err;
}

int ua_dl_set_trust_info(ua_dl_trust_t* trust)
{
    if (ua_dlc && trust) {
        ua_dlc->dl_trust.sync_trust = trust->sync_trust;
        ua_dlc->dl_trust.sync_crl   = trust->sync_crl;
        ua_dlc->dl_trust.pkg_trust  = trust->pkg_trust;
        ua_dlc->dl_trust.pkg_crl    = trust->pkg_crl;
        return E_UA_OK;
    }

    return E_UA_ERR;
}

static void trigger_session_request()
{
    json_object * bodyObject = json_object_new_object();
    json_object_object_add(bodyObject, "server-check", json_object_new_boolean(0));
    json_object * jObject = json_object_new_object();
    json_object_object_add(jObject, "type", json_object_new_string("xl4.session-request"));
    json_object_object_add(jObject, "body", bodyObject);
    ua_send_message(jObject);
    json_object_put(jObject);
}


