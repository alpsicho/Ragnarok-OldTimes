// Copyright (c) Athena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#ifndef _STRLIB_H_
#define _STRLIB_H_

#define J_MAX_MALLOC_SIZE 65535
// String function library.
// code by Jioh L. Jung (ziozzang@4wish.net)
// This code is under license "BSD"
char* jstrescape (char* pt);
char* jstrescapecpy (char* pt, const char* spt);
int jmemescapecpy (char* pt,char* spt, int size);

#ifdef __WIN32
#define HAVE_STRTOK_R
#define strtok_r(s,delim,save_ptr) _strtok_r((s),(delim),(save_ptr))
char *_strtok_r(char *s1, const char *s2, char **lasts);
#endif

// custom functions
int remove_control_chars(unsigned char *);
char *trim(char *str, const char *delim);
const char *stristr(const char *haystack, const char *needle);

#if !defined(WIN32) || (defined(_MSC_VER) && _MSC_VER < 1400)
size_t strnlen (const char* string, size_t maxlen);
#endif

#endif /* _STRLIB_H_ */
