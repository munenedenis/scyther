/***********************************************************************/
/* Based on public-domain code from Berkeley Yacc */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "mymalloc.h"

/*  machine-dependent definitions                              */
/*  the following definitions are for the Tahoe                */
/*  they might have to be changed for other machines           */

/*  MAXCHAR is the largest unsigned character value            */
/*  MAXSHORT is the largest value of a C short                 */
/*  MINSHORT is the most negative value of a C short           */
/*  MAXTABLE is the maximum table size                         */
/*  BITS_PER_WORD is the number of bits in a C unsigned        */
/*  WORDSIZE computes the number of words needed to            */
/*        store n bits                                         */
/*  BIT returns the value of the n-th bit starting             */
/*        from r (0-indexed)                                   */
/*  SETBIT sets the n-th bit starting from r                   */

#define        MAXCHAR                UCHAR_MAX
#define        MAXSHORT        SHRT_MAX
#define MINSHORT        SHRT_MIN
#define MAXTABLE        32500

#define BITS_PER_WORD        (8*sizeof(unsigned))
#define        WORDSIZE(n)        (((n)+(BITS_PER_WORD-1))/BITS_PER_WORD)
#define        BIT(r, n)        ((((r)[(n)/BITS_PER_WORD])>>((n)%BITS_PER_WORD))&1)
#define        SETBIT(r, n)        ((r)[(n)/BITS_PER_WORD]|=(1<<((n)%BITS_PER_WORD)))

/*  storage allocation macros  */

#define CALLOC(k,n)      (calloc((unsigned)(k),(unsigned)(n)))
#define FREE(x)          (free((char*)(x)))
#define MALLOC(n)        (malloc((unsigned)(n)))
//#define REALLOC(p,n)     (realloc((char*)(p),(unsigned)(n)))

/* actual functions */

void transitive_closure (unsigned int *R, int n);
void reflexive_transitive_closure (unsigned int *R, int n);
