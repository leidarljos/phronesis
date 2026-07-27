/* SPDX-License-Identifier: BSD-3-Clause — from libbsd */
#ifndef GROK_BSD_STRING_H
#define GROK_BSD_STRING_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
size_t strlcpy(char *dst, const char *src, size_t size);
size_t strlcat(char *dst, const char *src, size_t size);
#ifdef __cplusplus
}
#endif
#endif
