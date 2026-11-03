#ifndef AV2H_SHARED_ASSERT_H
#define AV2H_SHARED_ASSERT_H

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include_next <assert.h>

#else
#define assert(condition) ((void)0)
#endif

#endif