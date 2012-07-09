/* Copyright (c) 2011, Avian Contributors

   Permission to use, copy, modify, and/or distribute this software
   for any purpose with or without fee is hereby granted, provided
   that the above copyright notice and this permission notice appear
   in all copies.

   There is NO WARRANTY for this software.  See license.txt for
   details. */

#ifndef AVIAN_TARGET_FIELDS_H
#define AVIAN_TARGET_FIELDS_H


#ifdef TARGET_BYTES_PER_WORD
#  if (TARGET_BYTES_PER_WORD == 8)

#define TARGET_THREAD_EXCEPTION 80
#define TARGET_THREAD_EXCEPTIONSTACKADJUSTMENT 2256
#define TARGET_THREAD_EXCEPTIONOFFSET 2264
#define TARGET_THREAD_EXCEPTIONHANDLER 2272

#define TARGET_THREAD_IP 2216
#define TARGET_THREAD_STACK 2224
#define TARGET_THREAD_NEWSTACK 2232
#define TARGET_THREAD_SCRATCH 2240
#define TARGET_THREAD_CONTINUATION 2248
#define TARGET_THREAD_TAILADDRESS 2280
#define TARGET_THREAD_VIRTUALCALLTARGET 2288
#define TARGET_THREAD_VIRTUALCALLINDEX 2296
#define TARGET_THREAD_HEAPIMAGE 2304
#define TARGET_THREAD_CODEIMAGE 2312
#define TARGET_THREAD_THUNKTABLE 2320
#define TARGET_THREAD_STACKLIMIT 2368

#  elif (TARGET_BYTES_PER_WORD == 4)

#define TARGET_THREAD_EXCEPTION 44
#define TARGET_THREAD_EXCEPTIONSTACKADJUSTMENT 2164
#define TARGET_THREAD_EXCEPTIONOFFSET 2168
#define TARGET_THREAD_EXCEPTIONHANDLER 2172

#define TARGET_THREAD_IP 2144
#define TARGET_THREAD_STACK 2148
#define TARGET_THREAD_NEWSTACK 2152
#define TARGET_THREAD_SCRATCH 2156
#define TARGET_THREAD_CONTINUATION 2160
#define TARGET_THREAD_TAILADDRESS 2176
#define TARGET_THREAD_VIRTUALCALLTARGET 2180
#define TARGET_THREAD_VIRTUALCALLINDEX 2184
#define TARGET_THREAD_HEAPIMAGE 2188
#define TARGET_THREAD_CODEIMAGE 2192
#define TARGET_THREAD_THUNKTABLE 2196
#define TARGET_THREAD_STACKLIMIT 2220

#  else
#    error
#  endif
#else
#  error
#endif

#endif