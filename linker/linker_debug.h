#ifndef _LINKER_DEBUG_H_
#define _LINKER_DEBUG_H_

#include <stdio.h>

/* WARNING: For linker debugging only.. Be careful not to leave  any of
 * this on when submitting back to repository */
#define LINKER_DEBUG         0
#define TRACE_DEBUG          0
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
#undef TRUE
#undef FALSE
#define TRUE                 1
#define FALSE                0

/* Only use printf() during debugging.  We have seen occasional memory
 * corruption when the linker uses printf().
 */
#if LINKER_DEBUG
extern int debug_verbosity;
#warning "*** LINKER IS USING printf(); DO NOT CHECK THIS IN ***"
#define _PRINTVF(v,f,x...)   do {                                        \
        (debug_verbosity > (v)) && (printf(x), ((f) && fflush(stdout))); \
    } while (0)
#else /* !LINKER_DEBUG */
#define _PRINTVF(v,f,x...)   do {} while(0)
#endif /* LINKER_DEBUG */

#define PRINT(x...)          _PRINTVF(-1, FALSE, x)
#define INFO(x...)           _PRINTVF(0, TRUE, x)
#define TRACE(x...)          _PRINTVF(1, TRUE, x)
#define WARN(fmt,args...)    \
        _PRINTVF(-1, TRUE, "%s:%d| WARNING: " fmt, __FILE__, __LINE__, ## args)
#define ERROR(fmt,args...)   \
        _PRINTVF(-1, TRUE, "%s:%d| ERROR: " fmt, __FILE__, __LINE__, ## args)

#if TRACE_DEBUG
#define DEBUG(x...)          _PRINTVF(2, TRUE, "DEBUG: " x)
#else /* !TRACE_DEBUG */
#define DEBUG(x...)          do {} while (0)
#endif /* TRACE_DEBUG */

#if LINKER_DEBUG
#define TRACE_TYPE(t,x...)   do { if (DO_TRACE_##t) { TRACE(x); } } while (0)
#else  /* !LINKER_DEBUG */
#define TRACE_TYPE(t,x...)   do {} while (0)
#endif /* LINKER_DEBUG */

#if STATS
#define RELOC_ABSOLUTE        0
#define RELOC_RELATIVE        1
#define RELOC_COPY            2
#define RELOC_SYMBOL          3
#define NUM_RELOC_STATS       4

struct _link_stats {
    int reloc[NUM_RELOC_STATS];
};
extern struct _link_stats linker_stats;

#define COUNT_RELOC(type)                                 \
        do { if (type >= 0 && type < NUM_RELOC_STATS) {   \
                linker_stats.reloc[type] += 1;            \
             } else  {                                    \
                PRINT("Unknown reloc stat requested\n");  \
             }                                            \
           } while(0)
#else /* !STATS */
#define COUNT_RELOC(type)     do {} while(0)
#endif /* STATS */

#if TIMING
#undef WARN
#define WARN(x...)           do {} while (0)
#endif /* TIMING */

#if COUNT_PAGES
extern unsigned bitmask[];
#define MARK(offset)         do {                                        \
        bitmask[((offset) >> 12) >> 3] |= (1 << (((offset) >> 12) & 7)); \
    } while(0)
#else
#define MARK(x)              do {} while (0)
#endif

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
