/*
  Copyright (C) 1999 Aladdin Enterprises.  All rights reserved.

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  L. Peter Deutsch
  ghost@@aladdin.com

 */
/*$Id: md5.h,v 1.1 2004/01/05 18:54:19 icculus Exp $ */
/*
  Independent implementation of MD5 (RFC 1321).

  This code implements the MD5 Algorithm defined in RFC 1321.
  It is derived directly from the text of the RFC and not from the
  reference implementation.

  The original and principal author of md5.h is L. Peter Deutsch
  <ghost@@aladdin.com>.  Other authors are noted in the change history
  that follows (in reverse chronological order):

  1999-11-04 lpd Edited comments slightly for automatic TOC extraction.
  1999-10-18 lpd Fixed typo in header comment (ansi2knr rather than md5);
	added conditionalization for C++ compilation from Martin
	Purschke <purschke@@bnl.gov>.
  1999-05-03 lpd Original version.
 */

#ifndef md5_INCLUDED
#  define md5_INCLUDED

#if ((PLATFORM_WIN32) || (defined WIN32))
#define ARCH_IS_BIG_ENDIAN 0

#elif (defined PLATFORM_UNIX)

/* thanks to SDL headers for this. */
#if  defined(__i386__) || defined(__ia64__) || defined(WIN32) || \
    (defined(__alpha__) || defined(__alpha)) || \
     defined(__arm__) || \
    (defined(__mips__) && defined(__MIPSEL__)) || \
     defined(__LITTLE_ENDIAN__)
  #define ARCH_IS_BIG_ENDIAN 0
#else
  #define ARCH_IS_BIG_ENDIAN 1
#endif

#else
#error Please add your platform.
#endif

/*
 * This code has some adaptations for the Ghostscript environment, but it
 * will compile and run correctly in any environment with 8-bit chars and
 * 32-bit ints.  Specifically, it assumes that if the following are
 * defined, they have the same meaning as in Ghostscript: P1, P2, P3,
 * ARCH_IS_BIG_ENDIAN.
 */

typedef unsigned char md5_byte_t; /* 8-bit byte */
typedef unsigned int md5_word_t; /* 32-bit word */

/* Define the state of the MD5 Algorithm. */
typedef struct md5_state_s {
    md5_word_t count[2];	/* message length in bits, lsw first */
    md5_word_t abcd[4];		/* digest buffer */
    md5_byte_t buf[64];		/* accumulate block */
} md5_state_t;

#ifdef __cplusplus
extern "C" 
{
#endif

/* Initialize the algorithm. */
#ifdef P1
void md5_init(P1(md5_state_t *pms));
#else
void md5_init(md5_state_t *pms);
#endif

/* Append a string to the message. */
#ifdef P3
void md5_append(P3(md5_state_t *pms, const md5_byte_t *data, int nbytes));
#else
void md5_append(md5_state_t *pms, const md5_byte_t *data, int nbytes);
#endif

/* Finish the message and return the digest. */
#ifdef P2
void md5_finish(P2(md5_state_t *pms, md5_byte_t digest[16]));
#else
void md5_finish(md5_state_t *pms, md5_byte_t digest[16]);
#endif

#ifdef __cplusplus
}  /* end extern "C" */
#endif

#endif /* md5_INCLUDED */

