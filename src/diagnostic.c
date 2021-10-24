/*
 * diagnostic.c
 */

#include "diagnostic.h"
#include "delta.h"
#include "handler.h"
#include "debug.h"
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>

extern ua_internal_t ua_intl;
static diag_t diag;

char* get_time(void) 
{
    time_t mytime = time(NULL);
    char *time_str = ctime(&mytime);
    time_str[strlen(time_str) - 1] = '\0';
    return time_str;
}

int persistent_file_exist(const char *filename)
{
    struct stat buffer;
    int err = E_UA_OK;

    if (stat(filename, &buffer) != E_UA_OK)
    {
        A_INFO_MSG("art-ua %s : file not present.\n\n", filename);
        err = E_UA_ERR;
    }
    return err;
}

int store_data(char* id, char* pkgname, char* status){
    diag.id = id;
    diag.pkgname = pkgname;
    diag.d_status = status;
    diag.d_start = 0;
    diag.d_complete = 0;
    diag.p_start = 0;
    diag.p_stop = 0;

    
    int ret = E_UA_OK;
    if (persistent_file_exist(ua_intl.diag_file) == E_UA_ERR)
    {
        FILE *fp = fopen(ua_intl.diag_file, "w");
        if (!fp) {
            A_ERROR_MSG("Can't open file\n");
            return E_UA_ERR;
        }
        else
        {
            fprintf(fp, "%s, %s, %s, %s, %s, %s\n", "Campaign_ID", "Package_Name", "Download_Start", "Download_Complete",
                        "Patch_start", "Patch_Complete");
            fclose(fp);
            A_INFO_MSG("data.csv file open success\n");
            ret = E_UA_OK;
        }
    }

    FILE *fp = fopen(ua_intl.diag_file, "a+");
    if (!fp)
    {
        A_ERROR_MSG("can't open the file");
        return E_UA_ERR;
    }
    else if (!strcmp(diag.d_status,DOWNLOAD_START)) 
    {
        diag.d_start = get_time();
        fprintf(fp, "\n%s, %s, %s", diag.id, diag.pkgname, diag.d_start);
        fclose(fp);
    }
    else if (!strcmp(diag.d_status,DOWNLOAD_COMPLETE))
    {
        diag.d_complete = get_time();
        fprintf(fp, "%s, %s", "", diag.d_complete);
        fclose(fp);
    }
    else if (!strcmp(diag.d_status, PATCH_START))
    {
        diag.p_start = get_time();
        fprintf(fp, "%s, %s", "", diag.p_start);
        fclose(fp);
    }
    else if (!strcmp(diag.d_status, PATCH_COMPLETE))
    {
        diag.p_stop = get_time();
        fprintf(fp, "%s, %s\n", "", diag.p_stop);
        fclose(fp);
    }
    return ret;
}
