/*
 * Copyright (C) 2008-2010 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _LINKER_DEBUG_H_
#define _LINKER_DEBUG_H_

#include <stdio.h>

#ifndef LINKER_DEBUG
#error LINKER_DEBUG should be defined to either 1 or 0 in CamLinux.mk
#endif

/* set LINKER_DEBUG_TO_LOG to 1 to send the logs to logcat,
 * or 0 to use stdout instead.
 */
#define LINKER_DEBUG_TO_LOG  1
#define TRACE_DEBUG          1
#define DO_TRACE_LOOKUP      1
#define DO_TRACE_RELO        1
#define TIMING               0
#define STATS                0
#define COUNT_PAGES          0

/*********************************************************************
 * You shouldn't need to modify anything below unless you are adding
 * more debugging information.
 *
 * To enable/disable specific debug options, change the defines above
 *********************************************************************/


/*********************************************************************/

/* Only use printf() during debugging.  We have seen occasional memory
 * corruption when the linker uses printf().
 */
#if LINKER_DEBUG
#include "linker_format.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int debug_verbosity;
#if LINKER_DEBUG_TO_LOG
extern int format_log(int, const char *, const char *, ...);
#define _PRINTVF(v,x...)                                        \
    do {                                                          \
        if (debug_verbosity > (v)) format_log(5-(v),"linker",x);  \
    } while (0)
#else /* !LINKER_DEBUG_TO_LOG */
extern int format_fd(int, const char *, ...);
#define _PRINTVF(v,x...)                           \
    do {                                             \
        if (debug_verbosity > (v)) format_fd(1, x);  \
    } while (0)
#endif /* !LINKER_DEBUG_TO_LOG */

#ifdef __cplusplus
};
#endif

#else /* !LINKER_DEBUG */
#define _PRINTVF(v,f,x...)   do {} while(0)
#endif /* LINKER_DEBUG */

#define PRINT(x...)          _PRINTVF(-1, x)
#define INFO(x...)           _PRINTVF(0, x)
#define TRACE(x...)          _PRINTVF(1, x)
#define WARN(fmt,args...)    \
        _PRINTVF(-1, "%s:%d| WARNING: " fmt, __FILE__, __LINE__, ## args)
#define ERROR(fmt,args...)    \
        _PRINTVF(-1, "%s:%d| ERROR: " fmt, __FILE__, __LINE__, ## args)


#if TRACE_DEBUG
#define DEBUG(x...)          _PRINTVF(2, "DEBUG: " x)
#else /* !TRACE_DEBUG */
#define DEBUG(x...)          do {} while (0)
#endif /* TRACE_DEBUG */

#if LINKER_DEBUG
#define TRACE_TYPE(t,x...)   do { if (DO_TRACE_##t) { TRACE(x); } } while (0)
#else  /* !LINKER_DEBUG */
#define TRACE_TYPE(t,x...)   do {} while (0)
#endif /* LINKER_DEBUG */

#if TIMING
#undef WARN
#define WARN(x...)           do {} while (0)
#endif /* TIMING */

#define DEBUG_DUMP_PHDR(phdr, name, pid) do { \
        DEBUG("%5d %s (phdr = 0x%08x)\n", (pid), (name), (unsigned)(phdr));   \
        DEBUG("\t\tphdr->offset   = 0x%08x\n", (unsigned)((phdr)->p_offset)); \
        DEBUG("\t\tphdr->p_vaddr  = 0x%08x\n", (unsigned)((phdr)->p_vaddr));  \
        DEBUG("\t\tphdr->p_paddr  = 0x%08x\n", (unsigned)((phdr)->p_paddr));  \
        DEBUG("\t\tphdr->p_filesz = 0x%08x\n", (unsigned)((phdr)->p_filesz)); \
        DEBUG("\t\tphdr->p_memsz  = 0x%08x\n", (unsigned)((phdr)->p_memsz));  \
        DEBUG("\t\tphdr->p_flags  = 0x%08x\n", (unsigned)((phdr)->p_flags));  \
        DEBUG("\t\tphdr->p_align  = 0x%08x\n", (unsigned)((phdr)->p_align));  \
    } while (0)

#endif /* _LINKER_DEBUG_H_ */
