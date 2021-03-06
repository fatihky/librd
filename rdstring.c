/*
 * librd - Rapid Development C library
 *
 * Copyright (c) 2012-2013, Magnus Edenhill
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met: 
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "rd.h"
#include "rdstring.h"
#include "rdmem.h"

/*
 * Thread-local states
 */

struct rdstr_cyclic {
	int    size;
	char **buf;
	int   *len;
	int    i;
};

static __thread struct {
	
	/* rd_tsprintf() states */
	struct rdstr_cyclic tsp;
} rdstr_states;


/**
 * Initializes a cyclic state or updates it to the next index.
 */
static inline struct rdstr_cyclic *rdstr_cyclic_get (struct rdstr_cyclic *cyc,
						     int size) {
	
	if (unlikely(cyc->size == 0)) {
		/* Not initialized. */

		/* Allocate cyclic buffer array. */
		cyc->size = size;
		cyc->buf = calloc(sizeof(*cyc->buf), cyc->size);
		cyc->len = calloc(sizeof(*cyc->len), cyc->size);
	} else /* Update to the next pointer. */
		cyc->i = (cyc->i + 1) % cyc->size;

	return cyc;
}



char *rd_tsprintf (const char *format, ...) {
	va_list ap;
	int len;
	struct rdstr_cyclic *cyc;

	cyc = rdstr_cyclic_get(&rdstr_states.tsp, RD_TSPRINTF_BUFCNT);

	va_start(ap, format);
	len = vsnprintf(NULL, 0, format, ap);
	va_end(ap);

	if (len < 0) /* Error */
		return NULL;

	len++; /* Include nul-byte */

	if (cyc->buf[cyc->i] == NULL ||
	    cyc->len[cyc->i] < len ||
	    (cyc->len[cyc->i] > (len * 4) &&
	     cyc->len[cyc->i] > 64)) {
		if (cyc->buf[cyc->i])
			free(cyc->buf[cyc->i]);

		cyc->len[cyc->i] = len;
		cyc->buf[cyc->i] = malloc(len);
	}


	va_start(ap, format);
	vsnprintf(cyc->buf[cyc->i], cyc->len[cyc->i], format, ap);
	va_end(ap);

	return cyc->buf[cyc->i];
}


int rd_snprintf_cat (char *str, size_t size, const char *format, ...) {
	va_list ap;
	int of;

	of = strlen(str);

	if (of >= size) {
		errno = ERANGE;
		return -1;
	}

	va_start(ap, format);
	of += vsnprintf(str+of, size-of, format, ap);
	va_end(ap);
	
	return of;
}


void rd_string_thread_cleanup (void) {
	int i;
	struct rdstr_cyclic *cyc;

	/* rd_tsprintf() states */
	cyc = &rdstr_states.tsp;
	if (cyc->size) {
		for (i = 0 ; i < cyc->size ; i++)
			if (cyc->buf[i])
				free(cyc->buf[i]);

		free(cyc->buf);
		free(cyc->len);
		cyc->size = 0;
	}
}



char *rd_strnchrs (const char *s, ssize_t size, const char *delimiters,
		   int match_eol) {
	const char *end = s + size;
	char map[256] = {};

	while (*delimiters) {
		map[(int)*delimiters] = 1;
		delimiters++;
	}

	while ((size == -1 || s < end) && *s) {
		if (map[(unsigned char)*s])
			return (char *)s;

		s++;
	}

	if (match_eol)
		return (char *)s;

	return NULL;
}




size_t rd_strnspn_map (const char *s, size_t size,
		       int accept, const char map[256]) {
	const char *end = s + size;
	int cnt = 0;

	while ((size == -1 || s < end) && *s) {
		if (map[(int)*s] != accept)
			return cnt;
		s++;
		cnt++;
	}

	return cnt;
}

size_t rd_strnspn (const char *s, size_t size, const char *accept) {
	char map[256] = {};

	while (*accept) {
		map[(int)*accept] = 1;
		accept++;
	}

	return rd_strnspn_map(s, size, 1, map);
}



size_t rd_strncspn (const char *s, size_t size, const char *reject) {
	char map[256] = {};

	while (*reject) {
		map[(int)*reject] = 1;
		reject++;
	}

	return rd_strnspn_map(s, size, 0, map);
}



ssize_t rd_strndiffpos (const char *s1, size_t size1,
		       const char *s2, size_t size2) {
	ssize_t i;
	size_t minlen = RD_MIN(size1, size2);

	for (i = 0 ; i < minlen ; i++)
		if (s1[i] != s2[i])
			return i;

	if (size1 != size2)
		return minlen;

	return -1;
}


ssize_t rd_strdiffpos (const char *s1, const char *s2) {
	const char *begin = s1;
	
	while (*s1 == *s2) {
		if (!*s1)
			return -1;
		s1++;
		s2++;
	}

	return (ssize_t)(s1 - begin);
}
