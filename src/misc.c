/*
 * misc.c
 */

#include "misc.h"

static void safe_create_dir(const char *dir)
{
    if (mkdir(dir, 0755) < 0) {
        if (errno != EEXIST) {
            perror(dir);
            exit(1);
        }
    }
}

char * get_zip_error(int zc) {

    int ec = errno;
    char buf[256];
    char * buf2 = 0;
    int xx = zip_error_to_str(buf, 256, zc, ec);
    if (xx >= 255) {
        buf2 = f_malloc(xx+1);
        zip_error_to_str(buf2, xx+1, zc, ec);
    }

    if (buf2) { return buf2; }
    return f_strdup(buf);

}

int unzip(char * archive, char * path) {

    int zerr;
    char buf[100];
    int i, len;
    int fd;
    long sum;
    struct zip *za;
    struct zip_file *zf;
    struct zip_stat sb;

    do {

        if (!(za = zip_open(archive, 0, &zerr))) {
            char * aux = get_zip_error(zerr);
            DBG("failed to open file as ZIP %s : %s", archive, aux);
            free(aux);
            break;
        }

        safe_create_dir(path);

        for (i = 0; i < zip_get_num_entries(za, 0); i++) {
            if (zip_stat_index(za, i, 0, &sb) == 0) {
                char fname[strlen(path) + strlen(sb.name) + 1];
                strcpy(fname, path);
                strcat(fname, sb.name);
                len = strlen(fname);
                if (fname[len - 1] == '/') {
                    safe_create_dir(fname);
                } else {
                    zf = zip_fopen_index(za, i, 0);
                    if (!zf) {
                        DBG("failed to open/find %s: %s", sb.name, zip_strerror(za));
                        break;
                    }

                    fd = open(fname, O_RDWR | O_TRUNC | O_CREAT, 0644);
                    if (fd < 0) {
                        DBG("failed to open/create %s: %s", sb.name, zip_strerror(za));
                        break;
                    }

                    sum = 0;
                    while (sum != sb.size) {
                        len = zip_fread(zf, buf, 100);
                        if (len < 0) {
                            DBG("error reading %s : %s", sb.name, zip_file_strerror(za));
                            break;
                        }
                        write(fd, buf, len);
                        sum += len;
                    }
                    close(fd);
                    zip_fclose(zf);
                }
            } else {
                DBG("failed reading stat at index %d: %s", i, zip_strerror(za));
                break;
            }
        }

        if (zip_close(za) == -1) {
            fprintf(stderr, "can't close zip archive `%s'/n", archive);
            return 1;
        }
    } while (0);

    return 0;
}
