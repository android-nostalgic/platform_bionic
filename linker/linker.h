#ifndef _LINKER_H_
#define _LINKER_H_

#include <unistd.h>
#include <sys/types.h>
#include <linux/elf.h>

#undef PAGE_MASK
#undef PAGE_SIZE
#define PAGE_SIZE 4096
#define PAGE_MASK 4095

void debugger_init();
const char *addr_to_name(unsigned addr);

/* magic shared structures that GDB knows about */

struct link_map
{
    uintptr_t l_addr;
    char * l_name;
    uintptr_t l_ld;
    struct link_map * l_next;
    struct link_map * l_prev;
};

/* needed for dl_iterate_phdr to be passed to the callbacks provided */
struct dl_phdr_info
{
    Elf32_Addr dlpi_addr;
    const char *dlpi_name;
    const Elf32_Phdr *dlpi_phdr;
    Elf32_Half dlpi_phnum;
};


// Values for r_debug->state
enum {
    RT_CONSISTENT,
    RT_ADD,
    RT_DELETE
};

struct r_debug
{
    int32_t r_version;
    struct link_map * r_map;
    void (*r_brk)(void);
    int32_t r_state;
    uintptr_t r_ldbase;
};

typedef struct soinfo soinfo;

#define FLAG_LINKED     0x00000001
#define FLAG_ERROR      0x00000002
#define FLAG_EXE        0x00000004 // The main executable
#define FLAG_PRELINKED  0x00000008 // This is a pre-linked lib

#define SOINFO_NAME_LEN 128

struct soinfo
{
    const char name[SOINFO_NAME_LEN];
    Elf32_Phdr *phdr;
    int phnum;
    unsigned entry;
    unsigned base;
    unsigned size;

    unsigned *dynamic;

    unsigned wrprotect_start;
    unsigned wrprotect_end;

    soinfo *next;
    unsigned flags;

    const char *strtab;
    Elf32_Sym *symtab;

    unsigned nbucket;
    unsigned nchain;
    unsigned *bucket;
    unsigned *chain;

    unsigned *plt_got;

    Elf32_Rel *plt_rel;
    unsigned plt_rel_count;

    Elf32_Rel *rel;
    unsigned rel_count;

    unsigned *preinit_array;
    unsigned preinit_array_count;

    unsigned *init_array;
    unsigned init_array_count;
    unsigned *fini_array;
    unsigned fini_array_count;

    void (*init_func)(void);
    void (*fini_func)(void);

#ifdef ANDROID_ARM_LINKER
    /* ARM EABI section used for stack unwinding. */
    unsigned *ARM_exidx;
    unsigned ARM_exidx_count;
#endif

    unsigned refcount;
    struct link_map linkmap;
};


extern soinfo libdl_info;

/* these must all be powers of two */
#define LIBBASE 0x80000000
#define LIBLAST 0x90000000
#define LIBINC  0x00100000


#ifdef ANDROID_ARM_LINKER

#define R_ARM_COPY       20
#define R_ARM_GLOB_DAT   21
#define R_ARM_JUMP_SLOT  22
#define R_ARM_RELATIVE   23

#elif defined(ANDROID_X86_LINKER)

#define R_386_32         1
#define R_386_PC32       2
#define R_386_GLOB_DAT   6
#define R_386_JUMP_SLOT  7
#define R_386_RELATIVE   8

#endif /* ANDROID_*_LINKER */


#ifndef DT_INIT_ARRAY
#define DT_INIT_ARRAY      25
#endif

#ifndef DT_FINI_ARRAY
#define DT_FINI_ARRAY      26
#endif

#ifndef DT_INIT_ARRAYSZ
#define DT_INIT_ARRAYSZ    27
#endif

#ifndef DT_FINI_ARRAYSZ
#define DT_FINI_ARRAYSZ    28
#endif

#ifndef DT_PREINIT_ARRAY
#define DT_PREINIT_ARRAY   32
#endif

#ifndef DT_PREINIT_ARRAYSZ
#define DT_PREINIT_ARRAYSZ 33
#endif

/* in theory we only need the above relative relocations,
   but in practice the following one turns up from time
   to time.  fushigi na.
*/
#define R_ARM_ABS32      2

soinfo *find_library(const char *name);
unsigned unload_library(soinfo *si);
Elf32_Sym *lookup_in_library(soinfo *si, const char *name);
Elf32_Sym *lookup(const char *name, unsigned *base);

#ifdef ANDROID_ARM_LINKER 
typedef long unsigned int *_Unwind_Ptr;
_Unwind_Ptr dl_unwind_find_exidx(_Unwind_Ptr pc, int *pcount);
#elif defined(ANDROID_X86_LINKER)
int dl_iterate_phdr(int (*cb)(struct dl_phdr_info *, size_t, void *), void *);
#endif

#endif
