#ifndef OSS_LOGGING_H
#define OSS_LOGGING_H

#include <stdio.h>

/* __FUNCTION__ is both defined by GCC and VC */
#define olog(LEVEL, FMT, ...) printf("[" LEVEL "] [%s] " FMT "\n", __FUNCTION__, __VA_ARGS__)

#define ologi(FMT, ...) olog("II", FMT, __VA_ARGS__)
#define ologe(FMT, ...) olog("EE", FMT, __VA_ARGS__)
#define ologd(FMT, ...) olog("DD", FMT, __VA_ARGS__)

#endif
