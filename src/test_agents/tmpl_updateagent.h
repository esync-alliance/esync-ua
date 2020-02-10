/*
 * tmpl_updateagent.h
 */

#ifndef _TMPL_UPDATEAGENT_H_
#define _TMPL_UPDATEAGENT_H_

#include <xl4ua.h>
#include "build_config.h"
#if TMPL_UA_SUPPORT_SCP_TRANSFER
#include "util.h"
#endif

ua_routine_t* get_tmpl_routine(void);

#endif /* _TMPL_UPDATEAGENT_H_ */
