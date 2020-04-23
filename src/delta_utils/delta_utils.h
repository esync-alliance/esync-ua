#ifndef __DELTA_UTILS__
#define __DELTA_UTILS__

int xzdec(const char *infile, const char *outfile);
const char *espatch_get_version(void);
int espatch(const char *oldfile, const char *newfile, const char *patchfile);

#endif
