#ifndef __TRACE_H__
#define __TRACE_H__
/**
 * @file    trace.h
 * @author  ywang75
 *
 * @brief   tmp for trace log macro
 */

#include <string.h>
#include <stdio.h>


#ifdef __cplusplus
extern "C" {
#endif

#define TRACE_LOG(information, ...)  do{printf("func:%s; "information"\n", __FUNCTION__, ##__VA_ARGS__);fflush(stdout);}while(0)
#define TRACE_MSG(information, ...)    do{printf("func:%s; "information"\n",  __FUNCTION__, ##__VA_ARGS__);fflush(stdout);}while(0)

#define TRACE_ERROR(information, ...) do{fprintf(stderr, "func:%s; "information"\n", __FUNCTION__, ##__VA_ARGS__);fflush(stderr);}while(0)
#define TRACE_WARNING(information, ...)  do{fprintf(stderr, "func:%s; "information"\n", __FUNCTION__, ##__VA_ARGS__);fflush(stderr);}while(0)

#define TRACE_FUNC_HEAD  TRACE_MSG("start");
#define TRACE_FUNC_TAIL  TRACE_MSG("end");

#ifdef __cplusplus
}
#endif

#endif //__TRACE_H__
