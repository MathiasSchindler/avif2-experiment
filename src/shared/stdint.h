#ifndef AV2H_STDINT_H
#define AV2H_STDINT_H

#include <limits.h>

#define UINT8_MAX UCHAR_MAX
#define UINT16_MAX USHRT_MAX
#define UINT32_MAX UINT_MAX
#define UINT64_MAX ULLONG_MAX

#ifdef __SIZE_MAX__
#define SIZE_MAX __SIZE_MAX__
#else
#define SIZE_MAX ULONG_MAX
#endif

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;

#ifdef __INTPTR_TYPE__
typedef __INTPTR_TYPE__ intptr_t;
#else
typedef long intptr_t;
#endif

#ifdef __UINTPTR_TYPE__
typedef __UINTPTR_TYPE__ uintptr_t;
#else
typedef unsigned long uintptr_t;
#endif

#endif