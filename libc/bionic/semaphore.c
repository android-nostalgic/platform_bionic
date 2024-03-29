/*
 * Copyright (C) 2008 The Android Open Source Project
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
#include <semaphore.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/atomics.h>

int sem_init(sem_t *sem, int pshared, unsigned int value)
{
    if (sem == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (pshared != 0) {
        errno = ENOSYS;
        return -1;
    }

    sem->count = value;
    return 0;
}


int sem_destroy(sem_t *sem)
{
    if (sem == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (sem->count == 0) {
        errno = EBUSY;
        return -1;
    }
    return 0;
}


sem_t *sem_open(const char *name, int oflag, ...)
{
    name=name;
    oflag=oflag;

    errno = ENOSYS;
    return SEM_FAILED;
}


int sem_close(sem_t *sem)
{
    if (sem == NULL) {
        errno = EINVAL;
        return -1;
    }
    errno = ENOSYS;
    return -1;
}


int sem_unlink(const char * name)
{
    errno = ENOSYS;
    return -1;
}


static int
__atomic_dec_if_positive( volatile unsigned int*  pvalue )
{
    unsigned int  old;

    do {
        old = *pvalue;
    }
    while ( old != 0 && __atomic_cmpxchg( (int)old, (int)old-1, (volatile int*)pvalue ) != 0 );

    return old;
}

int sem_wait(sem_t *sem)
{
    if (sem == NULL) {
        errno = EINVAL;
        return -1;
    }

    for (;;) {
        if (__atomic_dec_if_positive(&sem->count))
            break;

        __futex_wait(&sem->count, 0, 0);
    }
    return 0;
}

int sem_post(sem_t *sem)
{
    if (sem == NULL)
        return EINVAL;

    if (__atomic_inc((volatile int*)&sem->count) == 0)
        __futex_wake(&sem->count, 1);

    return 0;
}

int  sem_trywait(sem_t *sem)
{
    if (sem == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (__atomic_dec_if_positive(&sem->count) > 0) {
        return 0;
    } else {
        return EAGAIN;
    }
}

int  sem_getvalue(sem_t *sem, int *sval)
{
    if (sem == NULL || sval == NULL) {
        errno = EINVAL;
        return -1;
    }

    *sval = sem->count;
    return 0;
}
