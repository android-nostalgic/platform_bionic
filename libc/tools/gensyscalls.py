#!/usr/bin/python
#
# this tool is used to generate the syscall assmbler templates
# to be placed into arch-x86/syscalls, as well as the content
# of arch-x86/linux/_syscalls.h
#

import sys, os.path, glob, re, string, commands, filecmp, shutil

from bionic_utils import *

# set this to 1 if you want to generate thumb stubs
gen_thumb_stubs = 0

# set this to 1 if you want to generate ARM EABI stubs
gen_eabi_stubs = 1

# get the root Bionic directory, simply this script's dirname
#
bionic_root = find_bionic_root()
if not bionic_root:
    print "could not find the Bionic root directory. aborting"
    sys.exit(1)

if bionic_root[-1] != '/':
    bionic_root += "/"

print "bionic_root is %s" % bionic_root

# temp directory where we store all intermediate files
bionic_temp = "/tmp/bionic_gensyscalls/"

# all architectures, update as you see fit
all_archs = [ "arm", "x86" ]

def make_dir( path ):
    if not os.path.exists(path):
        parent = os.path.dirname(path)
        if parent:
            make_dir(parent)
        os.mkdir(path)

def create_file( relpath ):
    dir = os.path.dirname( bionic_temp + relpath )
    make_dir(dir)
    return open( bionic_temp + relpath, "w" )

# x86 assembler templates for each syscall stub
#

x86_header = """/* autogenerated by gensyscalls.py */
#include <sys/linux-syscalls.h>

    .text
    .type %(fname)s, @function
    .globl %(fname)s
    .align 4

%(fname)s:
"""

x86_registers = [ "%ebx", "%ecx", "%edx", "%esi", "%edi", "%ebp" ]

x86_call = """    movl    $%(idname)s, %%eax
    int     $0x80
    cmpl    $-129, %%eax
    jb      1f
    negl    %%eax
    pushl   %%eax
    call    __set_errno
    addl    $4, %%esp
    orl     $-1, %%eax
1:
"""

x86_return = """    ret
"""

# ARM assembler templates for each syscall stub
#
arm_header = """/* autogenerated by gensyscalls.py */
#include <sys/linux-syscalls.h>

    .text
    .type %(fname)s, #function
    .globl %(fname)s
    .align 4
    .fnstart

%(fname)s:
"""

arm_call_default = arm_header + """\
    swi   #%(idname)s
    movs    r0, r0
    bxpl    lr
    b       __set_syscall_errno
    .fnend
"""

arm_call_long = arm_header + """\
    .save   {r4, r5, lr}
    stmfd   sp!, {r4, r5, lr}
    ldr     r4, [sp, #12]
    ldr     r5, [sp, #16]
    swi     # %(idname)s
    ldmfd   sp!, {r4, r5, lr}
    movs    r0, r0
    bxpl    lr
    b       __set_syscall_errno
    .fnend
"""

arm_eabi_call_default = arm_header + """\
    .save   {r4, r7}
    stmfd   sp!, {r4, r7}
    ldr     r7, =%(idname)s
    swi     #0
    ldmfd   sp!, {r4, r7}
    movs    r0, r0
    bxpl    lr
    b       __set_syscall_errno
    .fnend
"""

arm_eabi_call_long = arm_header + """\
    mov     ip, sp
    .save   {r4, r5, r6, r7}
    stmfd   sp!, {r4, r5, r6, r7}
    ldmfd   ip, {r4, r5, r6}
    ldr     r7, =%(idname)s
    swi     #0
    ldmfd   sp!, {r4, r5, r6, r7}
    movs    r0, r0
    bxpl    lr
    b       __set_syscall_errno
    .fnend
"""

# ARM thumb assembler templates for each syscall stub
#
thumb_header = """/* autogenerated by gensyscalls.py */
    .text
    .type %(fname)s, #function
    .globl %(fname)s
    .align 4
    .thumb_func
    .fnstart

#define  __thumb__
#include <sys/linux-syscalls.h>


%(fname)s:
"""

thumb_call_default = thumb_header + """\
    .save   {r7,lr}
    push    {r7,lr}
    ldr     r7, =%(idname)s
    swi     #0
    tst     r0, r0
    bmi     1f
    pop     {r7,pc}
1:
    neg     r0, r0
    ldr     r1, =__set_errno
    blx     r1
    pop     {r7,pc}
    .fnend
"""

thumb_call_long = thumb_header + """\
    .save  {r4,r5,r7,lr}
    push   {r4,r5,r7,lr}
    ldr    r4, [sp,#16]
    ldr    r5, [sp,#20]
    ldr    r7, =%(idname)s
    swi    #0
    tst    r0, r0
    bmi    1f
    pop    {r4,r5,r7,pc}
1:
    neg    r0, r0
    ldr    r1, =__set_errno
    blx    r1
    pop    {r4,r5,r7,pc}
    .fnend
"""


class State:
    def __init__(self):
        self.old_stubs = []
        self.new_stubs = []
        self.other_files = []
        self.syscalls = []

    def x86_genstub(self,fname, numparams, idname):
        t = { "fname"  : fname,
              "idname" : idname }

        result     = x86_header % t
        stack_bias = 4
        for r in range(numparams):
            result     += "    pushl   " + x86_registers[r] + "\n"
            stack_bias += 4

        for r in range(numparams):
            result += "    mov     %d(%%esp), %s" % (stack_bias+r*4, x86_registers[r]) + "\n"

        result += x86_call % t

        for r in range(numparams):
            result += "    popl    " + x86_registers[numparams-r-1] + "\n"

        result += x86_return
        return result


    def arm_genstub(self,fname, flags, idname):
        t = { "fname"  : fname,
              "idname" : idname }
        if flags:
            numargs = int(flags)
            if numargs > 4:
                return arm_call_long % t
        return arm_call_default % t


    def arm_eabi_genstub(self,fname, flags, idname):
        t = { "fname"  : fname,
              "idname" : idname }
        if flags:
            numargs = int(flags)
            if numargs > 4:
                return arm_eabi_call_long % t
        return arm_eabi_call_default % t


    def thumb_genstub(self,fname, flags, idname):
        t = { "fname"  : fname,
              "idname" : idname }
        if flags:
            numargs = int(flags)
            if numargs > 4:
                return thumb_call_long % t
        return thumb_call_default % t


    def process_file(self,input):
        parser = SysCallsTxtParser()
        parser.parse_file(input)
        self.syscalls = parser.syscalls
        parser = None

        for t in self.syscalls:
            syscall_func   = t["func"]
            syscall_params = t["params"]
            syscall_name   = t["name"]

            if t["id"] >= 0:
                if gen_thumb_stubs:
                    t["asm-thumb"] = self.thumb_genstub(syscall_func,len(syscall_params),"__NR_"+syscall_name)
                else:
                    if gen_eabi_stubs:
                        t["asm-arm"]   = self.arm_eabi_genstub(syscall_func,len(syscall_params),"__NR_"+syscall_name)
                    else:
                        t["asm-arm"]   = self.arm_genstub(syscall_func,len(syscall_params),"__NR_"+syscall_name)

            if t["id2"] >= 0:
                t["asm-x86"] = self.x86_genstub(syscall_func,len(syscall_params),"__NR_"+syscall_name)

    def gen_NR_syscall(self,fp,name,id):
        fp.write( "#define __NR_%-25s    (__NR_SYSCALL_BASE + %d)\n" % (name,id) )

    # now dump the content of linux/_syscalls.h
    def gen_linux_syscalls_h(self):
        path = "include/sys/linux-syscalls.h"
        D( "generating "+path )
        fp = create_file( path )
        fp.write( "/* auto-generated by gensyscalls.py, do not touch */\n" )
        fp.write( "#ifndef _BIONIC_LINUX_SYSCALLS_H_\n\n" )
        fp.write( "#if !defined __ASM_ARM_UNISTD_H && !defined __ASM_I386_UNISTD_H\n" )
        fp.write( "#if defined __arm__ && !defined __ARM_EABI__ && !defined __thumb__\n" )
        fp.write( "  #  define __NR_SYSCALL_BASE  0x900000\n" )
        fp.write( "  #else\n" )
        fp.write( "  #  define  __NR_SYSCALL_BASE  0\n" )
        fp.write( "  #endif\n\n" )

        # first, all common syscalls
        for sc in self.syscalls:
            sc_id  = sc["id"]
            sc_id2 = sc["id2"]
            sc_name = sc["name"]
            if sc_id == sc_id2 and sc_id >= 0:
                self.gen_NR_syscall( fp, sc_name, sc_id )

        # now, all arm-specific syscalls
        fp.write( "\n#ifdef __arm__\n" );
        for sc in self.syscalls:
            sc_id  = sc["id"]
            sc_id2 = sc["id2"]
            sc_name = sc["name"]
            if sc_id != sc_id2 and sc_id >= 0:
                self.gen_NR_syscall( fp, sc_name, sc_id )
        fp.write( "#endif\n" );

        # finally, all i386-specific syscalls
        fp.write( "\n#ifdef __i386__\n" );
        for sc in self.syscalls:
            sc_id  = sc["id"]
            sc_id2 = sc["id2"]
            sc_name = sc["name"]
            if sc_id != sc_id2 and sc_id2 >= 0:
                self.gen_NR_syscall( fp, sc_name, sc_id2 )
        fp.write( "#endif\n" );

        fp.write( "\n#endif\n" )
        fp.write( "\n#endif /* _BIONIC_LINUX_SYSCALLS_H_ */\n" );
        fp.close()
        self.other_files.append( path )


    # now dump the content of linux/_syscalls.h
    def gen_linux_unistd_h(self):
        path = "include/sys/linux-unistd.h"
        D( "generating "+path )
        fp = create_file( path )
        fp.write( "/* auto-generated by gensyscalls.py, do not touch */\n" )
        fp.write( "#ifndef _BIONIC_LINUX_UNISTD_H_\n\n" );
        fp.write( "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n" )

        for sc in self.syscalls:
            fp.write( sc["decl"]+"\n" )

        fp.write( "#ifdef __cplusplus\n}\n#endif\n" )
        fp.write( "\n#endif /* _BIONIC_LINUX_UNISTD_H_ */\n" );
        fp.close()
        self.other_files.append( path )

    # now dump the contents of syscalls.mk
    def gen_arch_arm_syscalls_mk(self):
        path = "arch-arm/syscalls.mk"
        D( "generating "+path )
        fp = create_file( path )
        fp.write( "# auto-generated by gensyscalls.py, do not touch\n" )
        fp.write( "syscall_src := \n" )
        for sc in self.syscalls:
                if sc["id"] >= 0:
                        fp.write( "syscall_src += arch-arm/syscalls/%s.S\n" % sc["func"] )
        fp.close()
        self.other_files.append( path )

    # now generate each syscall stub
    def gen_syscall_stubs(self):
        for sc in self.syscalls:
            if sc.has_key("asm-arm"):
                fname = "arch-arm/syscalls/%s.S" % sc["func"]
                D( ">>> generating "+fname )
                fp = create_file( fname )
                fp.write(sc["asm-arm"])
                fp.close()
                self.new_stubs.append( fname )

            if sc.has_key("asm-thumb"):
                fname = "arch-arm/syscalls/%s.S" % sc["func"]
                D( ">>> generating "+fname )
                fp = create_file( fname )
                fp.write(sc["asm-thumb"])
                fp.close()
                self.new_stubs.append( fname )

            if sc.has_key("asm-x86"):
                fname = "arch-x86/syscalls/%s.S" % sc["func"]
                D( ">>> generating "+fname )
                fp = create_file( fname )
                fp.write(sc["asm-x86"])
                fp.close()
                self.new_stubs.append( fname )


    def  regenerate(self):
        D( "scanning for existing architecture-specific stub files" )

        bionic_root_len = len(bionic_root)

        for arch in all_archs:
            arch_path = bionic_root + "arch-" + arch
            D( "scanning " + arch_path )
            files = glob.glob( arch_path + "/syscalls/*.S" )
            for f in files:
                self.old_stubs.append( f[bionic_root_len:] )

        D( "found %d stub files" % len(self.old_stubs) )

        if not os.path.exists( bionic_temp ):
            D( "creating %s" % bionic_temp )
            os.mkdir( bionic_temp )

#        D( "p4 editing source files" )
#        for arch in all_archs:
#            commands.getoutput( "p4 edit " + arch + "/syscalls/*.S " )
#            commands.getoutput( "p4 edit " + arch + "/syscalls.mk" )
#        commands.getoutput( "p4 edit " + bionic_root + "include/sys/linux-syscalls.h" )

        D( "re-generating stubs and support files" )

        self.gen_linux_syscalls_h()
        self.gen_arch_arm_syscalls_mk()
        self.gen_linux_unistd_h()
        self.gen_syscall_stubs()

        D( "comparing files" )
        adds    = []
        edits   = []

        for stub in self.new_stubs + self.other_files:
            if not os.path.exists( bionic_root + stub ):
                # new file, P4 add it
                D( "new file:     " + stub)
                adds.append( bionic_root + stub )
                shutil.copyfile( bionic_temp + stub, bionic_root + stub )

            elif not filecmp.cmp( bionic_temp + stub, bionic_root + stub ):
                D( "changed file: " + stub)
                edits.append( stub )

        deletes = []
        for stub in self.old_stubs:
            if not stub in self.new_stubs:
                D( "deleted file: " + stub)
                deletes.append( bionic_root + stub )


        if adds:
            commands.getoutput( "p4 add " + string.join( adds, " " ) )
        if deletes:
            commands.getoutput( "p4 delete " + string.join( deletes, " " ) )
        if edits:
            commands.getoutput( "p4 edit " + string.join( edits, " " ) )
            for file in edits:
                shutil.copyfile( bionic_temp + file, bionic_root + file )

        D("ready to go !!")

D_setlevel(1)

state = State()
state.process_file(bionic_root+"SYSCALLS.TXT")
state.regenerate()
