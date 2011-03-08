/*
 * $Id: bits.h 346460 2009-11-14 05:06:47Z ssiano $
 *
 * Bit manipulation package
 *
 * Copyright (c) 1997-2008, Juniper Networks, Inc.
 * All rights reserved.
 */

#ifndef __JNX_BITS_H__
#define __JNX_BITS_H__

/*
 * Use C linkage when using a C++ compiler
 */
#ifdef __cplusplus
extern "C" {
    namespace junos {
#endif

typedef u_int32_t flag_t;           /**< Flag */
typedef u_int64_t flag64_t;

typedef struct bits_s {
    flag_t	t_bits;
    const char *t_name;
} bits;

typedef struct bits64_t_s {
    flag64_t	b64_bits;
    const char *b64_name;
} bits64_t;

/*
 * Define the standard terminology used in the software
 * with regards to bytes, words, etc.
 * BYTE = 8 bits
 * HWORD (halfword) = 2 bytes or 16 bits
 * WORD = 4 bytes or 32 bits
 * QUAD = 8 bytes or 64 bits
 *
 * (The term QUAD seems less-than-intuitive here, but it is
 * derived from BSD sources where it is defined as int64_t.)
 *
 * For consistency use the following defines wherever appropriate.
 */

typedef enum {
    NBI_BYTE  = (sizeof(u_int8_t) * 8),
    NBI_HWORD = (sizeof(u_int16_t) * 8),
#ifndef NBI_WORD
    NBI_WORD  = (sizeof(u_int32_t) * 8),
#endif
    NBI_QUAD  = (sizeof(u_int64_t) * 8)
} num_bits_t;

typedef enum {
    NBY_BYTE  = sizeof(u_int8_t),
    NBY_HWORD = sizeof(u_int16_t),
    NBY_WORD  = sizeof(u_int32_t),
    NBY_QUAD  = sizeof(u_int64_t)
} num_bytes_t;

#define BITS_IN_BYTE NBI_BYTE

#define	BIT(b)	b ## ul

#define	BIT_SET(f, b) do { \
                          (f) |= (b); \
                      } while (0)

#define	BIT_RESET(f, b) do { \
                            (f) &= ~(b); \
                        } while (0)

#define	BIT_FLIP(f, b) do { \
                           (f) ^= (b); \
                       } while (0)

#define	BIT_TEST(f, b)	((f) & (b))
#define	BIT_MASK(f, b)	BIT_TEST(f, b)
#define	BIT_ISSET(f, b)	(BIT_TEST(f, b) != 0)
#define	BIT_MATCH(f, b)	(((f) & (b)) == (b))
#define	BIT_COMPARE(f, b1, b2)	((((f) & (b1)) == (b2)))
#define	BIT_MASK_MATCH(f, g, b)	(!(((f) ^ (g)) & (b)))
#define BITT(type, bitnum) (((type) 1) << bitnum)
#define BIT32(bitnum) BITT(u_int32_t, bitnum)
#define BIT64(bitnum) BITT(u_int64_t, bitnum)

extern const char * bit_value_encode __P((const bits *, flag_t));
extern flag_t bit_value_decode __P((const bits *, const char *));
extern const char * bits_encode __P((const bits *, flag_t));
extern char *bits_decode __P((const bits *, const char *, flag_t *));

extern const char * bit64_value_encode __P((const bits64_t *, const flag64_t));
extern flag64_t bit64_value_decode __P((const bits64_t *, const char *));
extern const char * bits64_encode __P((const bits64_t *, flag64_t));
extern char *bits64_decode __P((const bits64_t *, const char *, flag64_t *));


#ifdef __cplusplus
    }
}
#endif /* __cplusplus */


#endif /* __JNX_BITS_H__ */

