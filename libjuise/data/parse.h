/*
 * $Id: parse.h 346460 2009-11-14 05:06:47Z ssiano $
 *
 *
 * Copyright (c) 1999-2007, Juniper Networks, Inc.
 * All rights reserved.
 */

#ifndef __JNX_PARSE__H__
#define __JNX_PARSE__H__

/**
 * @file parse.h
 * @brief 
 * Common parser APIs and return code definition.
 */

#include <libjuise/common/aux_types.h>

/*
 * Use C linkage when using a C++ compiler
 */
#ifdef __cplusplus
extern "C" { 
    namespace junos {
#endif /* __cplusplus */

/**
 * @brief
 * Return codes for the parse_xxx routines.
 */
typedef enum parse_retcode_e {
    PARSE_OK = 0,            /**< Everything parsed without errors */
    PARSE_ERR = -1,          /**< Could not grok the string */
    PARSE_ERR_RESTRICT = -2  /**< Almost parsed, but failed some restriction */ 
} parse_retcode_t;

/**
 * @brief
 * Gets a number from the unparsed buffer.  
 *
 * The number must be between zero and @a max.
 *
 * @param[in]  unparsed
 *     String to parse
 * @param[out] number
 *     Pointer to where parsed value should be stored
 * @param[in]  max
 *     Maximum value for the number
 *
 * @return 
 *     @c TRUE if succesful; @c FALSE otherwise.
 */
int
parse_number (const char *unparsed, int *number, int max);

/**
 * @brief
 * Gets an integer from the unparsed buffer.  
 *
 * The integer can be specified in decimal or hexadecimal format.
 *
 * @param[in]  unparsed
 *     String to parse
 * @param[out] number
 *     Pointer to where parsed value should be stored
 *
 * @return 
 *     0 if successful; -1 otherwise (@c errno will be set appropriately).
 */
int
parse_integer (const char *unparsed, int *number);

/**
 * @brief
 * Gets a quad from the unparsed buffer.  
 * 
 * The quad can be specified in decimal or hexadecimal format.
 *
 * @param[in]  unparsed
 *     String to parse
 * @param[out] number
 *     Pointer to where parsed value should be stored
 *
 * @return 
 *     0 if successful; -1 otherwise (@c errno will be set appropriately).
 */
int
parse_quad (const char *unparsed, quad_t *number);

#ifdef __cplusplus
    }
}
#endif /* __cplusplus */

#endif /* __JNX_PARSE_H__ */

