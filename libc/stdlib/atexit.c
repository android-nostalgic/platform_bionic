/*	$OpenBSD: atexit.c,v 1.12 2006/02/22 07:16:32 otto Exp $ */
/*
 * Copyright (c) 2002 Daniel Hartmeier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include "atexit.h"
#include "thread_private.h"

int __atexit_invalid = 1;
struct atexit *__atexit;

/*
 * Function pointers are stored in a linked list of pages. The list
 * is initially empty, and pages are allocated on demand. The first
 * function pointer in the first allocated page (the last one in
 * the linked list) is reserved for the cleanup function.
 *
 * Outside the following two functions, all pages are mprotect()'ed
 * to prevent unintentional/malicious corruption.
 */

/*
 * Register a function to be performed at exit.
 */
int
atexit(void (*fn)(void))
{
	struct atexit *p;
	int pgsize = getpagesize();
	int ret = -1;

	if (pgsize < (int)sizeof(*p))
		return (-1);
	_ATEXIT_LOCK();
	p = __atexit;
	if (p != NULL) {
		if (p->ind + 1 >= p->max)
			p = NULL;
		else if (mprotect(p, pgsize, PROT_READ | PROT_WRITE))
			goto unlock;
	}
	if (p == NULL) {
		p = mmap(NULL, pgsize, PROT_READ | PROT_WRITE,
		    MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		if (p == MAP_FAILED)
			goto unlock;
		if (__atexit == NULL) {
			p->fns[0] = NULL;
			p->ind = 1;
		} else
			p->ind = 0;
		p->max = (pgsize - ((char *)&p->fns[0] - (char *)p)) /
		    sizeof(p->fns[0]);
		p->next = __atexit;
		__atexit = p;
		if (__atexit_invalid)
			__atexit_invalid = 0;
	}
	p->fns[p->ind++] = fn;
	if (mprotect(p, pgsize, PROT_READ))
		goto unlock;
	ret = 0;
unlock:
	_ATEXIT_UNLOCK();
	return (ret);
}

/*
 * Register the cleanup function
 */
void
__atexit_register_cleanup(void (*fn)(void))
{
	struct atexit *p;
	int pgsize = getpagesize();

	if (pgsize < (int)sizeof(*p))
		return;
	_ATEXIT_LOCK();
	p = __atexit;
	while (p != NULL && p->next != NULL)
		p = p->next;
	if (p == NULL) {
		p = mmap(NULL, pgsize, PROT_READ | PROT_WRITE,
		    MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		if (p == MAP_FAILED)
			goto unlock;
		p->ind = 1;
		p->max = (pgsize - ((char *)&p->fns[0] - (char *)p)) /
		    sizeof(p->fns[0]);
		p->next = NULL;
		__atexit = p;
		if (__atexit_invalid)
			__atexit_invalid = 0;
	} else {
		if (mprotect(p, pgsize, PROT_READ | PROT_WRITE))
			goto unlock;
	}
	p->fns[0] = fn;
	mprotect(p, pgsize, PROT_READ);
unlock:
	_ATEXIT_UNLOCK();
}
