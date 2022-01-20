/*
 * diagnostic.h
 */

#ifndef UA_DIAGNOSTIC_H_
#define UA_DIAGNOSTIC_H_

#define DOWNLOAD_START      "DS_DOWNLOAD"
#define DOWNLOAD_COMPLETE   "DS_VERIFY"
#define PATCH_START         "D_START"
#define PATCH_COMPLETE      "D_COMPLETE"
#define PATCH_SUCESS        "SUCCESS"
#define PATCH_FAILURE       "FAILED"

int persistent_file_exist(const char *filename);
char* get_time(void);
int store_data(char *id, char *pkgname, char *status, unsigned int value, char *pkgversion, char *p_status);

typedef struct diag_csv
{
    const char* id;
    const char* pkgname;
    char* pkgversion;
    const char* d_start;
    const char* d_complete;
    char* p_start;
    char* p_stop;
    char* d_status;
    unsigned int t_length;
    float t_size;
    char *p_status;
} diag_t;

#endif /* UA_DIAGNOSTIC_H_ */