/*
 * $Id$
 *
 * Copyright (c) 1996-2007, 2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

#ifndef	__JNX_PATRICIA_H__
#define	__JNX_PATRICIA_H__

#include <stdlib.h>
#include <stddef.h>
#include <assert.h>

#include <libjuise/common/bits.h>

/*
 * Use C linkage when using a C++ compiler
 */
#ifdef __cplusplus
extern "C" { 
    namespace junos {
#endif /* __cplusplus */

/**
 * @file patricia.h
 * @brief Patricia tree APIs
 *
 * This file contains the public data structures for the patricia tree
 * package.  This package is applicable to searching a non-overlapping
 * keyspace of pseudo-random data with fixed size keys.  The patricia tree
 * is not balanced, so this package is NOT appropriate for highly skewed
 * data.  The package can deal with variable length (in byte increments)
 * keys, but only when it can be guaranteed that no key in the tree is a
 * prefix of another (null-terminated strings have this property if you
 * include the '\\0' in the key).
 *
 * This package supports keys up to 256 bytes in length.  For random data,
 * all operations are O(log n).  There are two necessary data structures:
 * one is the root of a tree (type patroot), and the contents of this are
 * hidden from you.  The other is a node (type patnode).  The contents of
 * this data structure are (unfortunately) public to make the compiler
 * happy.
 *
 * To use this package:
 * First imbed the patnode structure in your data structure.  You have
 * two choices for the key.  The first is to embed the key into the
 * data structure immediately following the patnode stucture, as in:
 *
 * @code
 *		struct foobar {
 *		    ...;
 *		    patnode patricia;
 *		    u_char  key[KEYSIZE];
 *		    ...;
 *	        }
 * @endcode
 *
 * The other choice is to embed a pointer to the key material immediately
 * following the data structure, as in
 *
 * @code
 *		struct blech {
 *		    ...;
 *		    patnode patricia;
 *		    sockaddr_un *key_ptr;
 *		    ...;
 *	        }
 * @endcode
 *
 * In either case you can also specify an offset to the actual key material.
 * The choice of key location and offset, and the length of keys stored
 * in the tree if this is fixed, is specified in a call to
 * patricia_root_init().  If no patroot pointer is passed in one is
 * allocated and returned, otherwise the specified root pointer is
 * filled in.
 *
 * If you want to use your own allocate & free routines for patroots,
 * set them using patricia_set_allocator().
 *
 * For each node that you wish to add to the tree, you must first
 * initialize the node with a call to patricia_node_init_length() with the
 * node and the length of the associated key, in bytes.  You can also
 * call patricia_node_init() if the key length was fixed at root
 * initialization.  Then, once the key is installed in the node, you may
 * call patricia_add().  Note that after calling patricia_add(), you may
 * NOT change the key.  You should also note that the entire key field to
 * the length specified to patricia_length() (or all the way to `KEYSIZE'
 * if the tree is built with fixed-length keys) may be examined.
 *
 * Once the tree is initialized you can use the following functions:
 *
 * patricia_add()
 * patricia_delete()
 * patricia_get()
 * patricia_getnext()
 * patricia_find_next()
 * patricia_find_prev()
 * patricia_subtree_match()
 * patricia_subtree_next()
 *
 * When you're done with the tree, you can call patricia_root_delete() on
 * an empty tree to get rid of the root information if the root was allocated
 * at initialization time.
 *
 * Generally you will not want to deal with the patricia structure
 * directly, so it's helpful to be able to be able to get back to the
 * primary structure.  This can be done with the PATNODE_TO_STRUCT() macro.
 * Using this, you can then easily define functions which completely hide
 * the patricia structure from the rest of your code.  This is STRONGLY
 * recommended.
 */

/**
 * @brief
 * Patricia tree node.
 */
typedef struct patnode_ {
    u_int16_t		length;		/**< length of key, formated like bit */
    u_int16_t		bit;		/**< bit number to test for patricia */
    struct patnode_	*left;		/**< left branch for patricia search */
    struct patnode_	*right;		/**< right branch for same */
    union {
	u_int8_t	key[0];		/**< start of key */
	u_int8_t	*key_ptr[0];	/**< pointer to key */
    } patnode_keys;
} patnode;

/**
 * @brief
 * The maximum length of a key, in bytes.
 */
#define	PAT_MAXKEY		256

/**
 * @brief
 * A macro to initialize the `length' in a patnode at compile time given
 * the length of a key.  Good for the keyword tree.  Note the length
 * must be greater than zero.
 */
#define	PATRICIA_LEN_TO_BIT(len)  ((u_int16_t) ((((len) - 1) << 8) | 0xff))

/**
 * @brief
 * Patricia tree root.
 */
typedef struct patroot_ {
    patnode *root;			/**< root patricia node */
    u_int16_t key_bytes;		/**< (maximum) key length in bytes */
    u_int8_t  key_offset;		/**< offset to key material */
    u_int8_t  key_is_ptr;		/**< really boolean */
} patroot;

typedef struct patnode_ patnode_t; /* backward compatibility */
typedef struct patroot_ patroot_t; /* backward compatibility */

/**
 * @brief
 * Typedef for user-specified patroot allocation function.
 * @sa patricia_set_allocator
 */
 typedef patroot *(*patricia_root_alloc_fn)(void);
 
/**
 * @brief
 * Typedef for user-specified patroot free function.
 * @sa patricia_set_allocator
 */
 typedef void (*patricia_root_free_fn)(patroot *);

/*
 * Prototypes
 */

/**
 * @brief
 * Initializes a patricia tree root.
 *
 * @param[in] root
 *     Pointer to patricia tree root
 * @param[in] key_is_ptr
 *     Indicator that the key located at the offset is actually a pointer or not
 * @param[in] key_bytes
 *     Number of bytes in the key
 * @param[in] key_offset
 *     Offset to the key from the end of the patricia tree node structure
 *
 * @return 
 *     A pointer to the patricia tree root.
 */
patroot *
patricia_root_init (patroot *root, boolean key_is_ptr, u_int16_t key_bytes,
		    u_int8_t key_offset); 

/**
 * @brief
 * Deletes a patricia tree root.
 *
 * @note An assertion failure will occur if the tree is not empty when this
 *       function is called.
 *
 * @param[in] root
 *     Pointer to patricia tree root
 */
void
patricia_root_delete (patroot *root);

/**
 * @brief
 * Initializes a patricia tree node using the key length specified by @c key_bytes. 
 * If @c key_bytes is zero, then it falls back to the length specified during
 * root initialization (@c patricia_root_init).
 *
 * @param[in] node
 *     Pointer to patricia tree node
 * @param[in] key_bytes
 *     Length of the key, in bytes
 */
void
patricia_node_init_length (patnode *node, u_int16_t key_bytes);   

/**
 * @brief
 * Adds a new node to the tree.
 *
 * @param[in] root
 *     Pointer to patricia tree root
 * @param[in] node
 *     Pointer to patricia tree node
 *
 * @return 
 *     @c TRUE if the node is successfully added; 
 *     @c FALSE if the key you are adding is the same as, or overlaps 
 *      with (variable length keys), something already in the tree.
 */
boolean
patricia_add (patroot *root, patnode *node);

/**
 * @brief
 * Deletes a node from the tree.
 *
 * @param[in] root
 *     Pointer to patricia tree root
 * @param[in] node
 *     Pointer to patricia tree node
 *
 * @return 
 *     @c TRUE if the node is successfully deleted; 
 *     @c FALSE if the specified node is not in the tree.
 */
boolean
patricia_delete (patroot *root, patnode *node);

/**
 * @brief
 * Given a node in the tree, find the node with the next numerically larger
 * key.  If the given node is NULL, the numerically smallest node in the tree
 * will be returned.
 *
 * @note Good for tree walks.
 *
 * @param[in] root
 *     Pointer to patricia tree root
 * @param[in] node
 *     Pointer to patricia tree node
 *
 * @return 
 *     @c NULL if the specified node is already the largest; 
 *     otherwise a pointer to the node with the next numerically larger key.
 */
patnode *
patricia_find_next (patroot *root, patnode *node);

/**
 * @brief
 * Given a node in the tree, find the node with the next numerically smaller
 * key.  If the given node is NULL, returns the numerically largest node in
 * the tree.
 *
 * @note The definitions of patricia_find_next() and patricia_find_prev() are
 *       such that
 *
 * @code
 * node == patricia_find_prev(root, patricia_find_next(root, node));
 * @endcode
 *
 * will always be @c TRUE for any node in the tree or @c NULL.
 *
 * @param[in] root
 *     Pointer to patricia tree root
 * @param[in] node
 *     Pointer to patricia tree node
 *
 * @return 
 *     @c NULL if the specified node was the smallest; 
 *      otherwise a pointer to the patricia tree node with next numerically 
 *      smaller key.
 */
patnode *
patricia_find_prev (patroot *root, patnode *node);

/**
 * @brief
 * Given a prefix and a prefix length in bits, find the node with the
 * numerically smallest key in the tree which includes this prefix.
 *
 * @param[in] root
 *     Pointer to patricia tree root
 * @param[in] prefix_len
 *     Length of prefix, in bits
 * @param[in] prefix
 *     Pointer to prefix
 *
 * @return 
 *     @c NULL if no node in the tree has the prefix; 
 *     otherwise a pointer to the patricia tree node with the 
 *     numerically smallest key which includes the prefix.
 */
patnode *
patricia_subtree_match (patroot *root, u_int16_t prefix_len,
			const void *prefix);

/**
 * @brief
 * Given a node in the tree, and a prefix length in bits, return the next
 * numerically larger node in the tree which shares a prefix with the node
 * specified.
 *
 * @param[in] root
 *     Pointer to patricia tree root
 * @param[in] node
 *     Pointer to patricia tree node
 * @param[in] prefix_len
 *     Length of prefix, in bits
 *
 * @return 
 *     A pointer to next numerically larger patricia tree node.
 */
patnode *
patricia_subtree_next (patroot *root, patnode *node, u_int16_t prefix_len);

/**
 * @brief
 * Looks up a node having the specified key and key length in bytes.  
 *
 * @param[in] root
 *     Pointer to patricia tree root
 * @param[in] key_bytes
 *     Number of bytes in key
 * @param[in] key
 *     Pointer to key value
 *
 * @return 
 *     @c NULL if a match is not found; 
 *     otherwise a pointer to the matching patricia tree node
 */
patnode *
patricia_get (patroot *root, u_int16_t key_bytes, const void *key);

/**
 * @brief
 * Given a key and key length in bytes, return a node in the tree which is at
 * least as large as the key specified.  
 *
 * The call has a parameter which
 * modifies its behaviour when an exact match is found in the tree; you can
 * either choose to have it return the exact match, if there is one, or you
 * can have it always return a node with a larger key (a la SNMP getnext).
 *
 * @param[in] root
 *     Pointer to patricia tree root
 * @param[in] key_bytes
 *     Number of bytes in key
 * @param[in] key
 *     Pointer to key value
 * @param[in] return_eq
 *     FALSE for classic getnext
 *
 * @return 
 *     A pointer to patricia tree node.
 */
patnode *
patricia_getnext (patroot *root, u_int16_t key_bytes, const void *key,
		  boolean return_eq);

/**
 * @brief
 * Determines if a patricia tree node is contained within a tree.
 *
 * @param[in] node
 *     Pointer to patricia tree node
 *
 * @return 
 *      @c TRUE if the node is in the tree; @c FALSE otherwise.
 */
boolean
patricia_node_in_tree (const patnode *node);

/**
 * @brief
 * Given two patricia tree nodes, determine if the first has a key which is
 * numerically lesser, equal, or greater than the key of the second.
 *
 * @param[in] root
 *     Pointer to patricia tree root
 * @param[in] left
 *     Pointer to patricia tree node
 * @param[in] right
 *     Pointer to patricia tree node
 *
 * @return 
 *     Result of the comparison:
 *     @li -1 if the key of the left node is numerically lesser than the right
 *     @li 0 if the keys match
 *     @li 1 if the left key is numerically greater than the right
 */
int
patricia_compare_nodes (patroot *root, patnode *left, patnode *right);

/**
 * @brief
 * Sets allocation and free routines for patricia tree root structures.
 *
 * @note The initialization APIs contained in libjunos-sdk or libmp-sdk may
 *       use Patricia tree functionality.  Therefore, if you intend to 
 *       change the allocator, this function should be called before any 
 *       libjunos-sdk or libmp-sdk APIs are used in the JUNOS SDK 
 *       application.
 *
 * @param[in] my_alloc
 *     Function to call when patricia tree root is allocated
 * @param[in] my_free
 *     Function to call when patricia tree root is freed
 */
void
patricia_set_allocator (patricia_root_alloc_fn my_alloc,
			patricia_root_free_fn my_free);

/*
 * utility functions for dealing with const trees -- useful for 
 * iterator functions that shouldn't be able to change the contents
 * of the tree, both as far as tree structure and as far as node content.. 
 */

/**
 * @brief
 * Constant tree form of patricia_get()
 *
 * @param[in] root
 *     Pointer to patricia tree root
 * @param[in] key_bytes
 *     Number of bytes in key
 * @param[in] key
 *     Pointer to key value
 *
 * @return
 *     @c NULL if a match is not found; 
 *     Otherwise a @c const pointer to the matching patricia tree node.
 *
 * @sa patricia_get
 */
const patnode *
patricia_cons_get (const patroot *root, const u_int16_t key_bytes, 
		   const void *key);

/**
 * @brief
 * Constant tree form of patricia_find_next()
 *
 * @param[in] root
 *     Pointer to patricia tree root
 * @param[in] node
 *     Pointer to patricia tree node
 *
 * @return
 *     @c NULL if a match is not found; 
 *     Otherwise a @c const pointer to the next patricia tree node.
 *
 * @sa patricia_find_next
 */
const patnode *
patricia_cons_find_next (const patroot *root, const patnode *node);

/**
 * @brief
 * Constant tree form of patricia_find_prev()
 *
 * @param[in] root
 *     Pointer to patricia tree root
 * @param[in] node
 *     Pointer to patricia tree node
 *
 * @return
 *     @c NULL if a match is not found; 
 *     Otherwise a @c const pointer to the previous patricia tree node.
 *
 * @sa patricia_find_prev
 */
const patnode *
patricia_cons_find_prev (const patroot *root, const patnode *node);

/**
 * @brief
 * Constant tree form of patricia_subtree_match()
 *
 * @param[in] root
 *     Pointer to patricia tree root
 * @param[in] prefix_len
 *     Length of prefix, in bits
 * @param[in] prefix
 *     Pointer to prefix
 *
 * @return
 *     @c NULL if no node in the tree has the prefix; 
 *     Otherwise a pointer to the patricia tree node with the 
 *     numerically smallest key which includes the prefix.
 * 
 * @sa patricia_subtree_match
 */
const patnode *
patricia_cons_subtree_match (const patroot *root, const u_int16_t prefix_len,
			     const void *prefix);

/**
 * @brief
 * Constant tree form of patricia_subtree_next()
 *
 * @param[in] root
 *     Pointer to patricia tree root
 * @param[in] node
 *     Pointer to patricia tree node
 * @param[in] prefix_len
 *     Length of prefix, in bits
 *
 * @return
 *     A @c const pointer to next numerically larger patricia tree node.
 * 
 * @sa patricia_subtree_next
 */
const patnode *
patricia_cons_subtree_next (const patroot *root, const patnode *node,
			    const u_int16_t prefix_len);

/*
 * Inlines, for performance
 * 
 * All contents below this line are subject to change without notice.
 * Don't go poking into the implementation details here...
 */

/**
 * @brief
 * Initializes a patricia tree node using the the key length specified during root 
 * initialization (@c patricia_root_init).
 * 
 * @param[in] node
 *     Pointer to patricia tree node
 */
static inline void
patricia_node_init (patnode *node) {
    patricia_node_init_length((node), 0);
} 

/**
 * @brief
 * Bit number when no external node
 */
#define	PAT_NOBIT  (0)

/**
 * @brief
 * Obtains a pointer to the start of the key material for a patricia node.
 *
 * @param[in]  root
 *     Pointer to patricia tree root
 * @param[in] node
 *     Pointer to patricia tree node
 *
 * @return 
 *     A pointer to the start of node key.
 */
static inline const u_int8_t *
patricia_key (patroot *root, patnode *node)
{
    if (root->key_is_ptr) {
	return (node->patnode_keys.key_ptr[0] + root->key_offset);
    }
    return (node->patnode_keys.key + root->key_offset);
}

/**
 * @brief
 * Performs a bit test on a key.
 *
 * @param[in]  key
 *     Pointer to key material
 * @param[in]  bit
 *     Bit number to test
 *
 * @return 
 *     1 if the bit was set, 0 otherwise.
 */
static inline u_int8_t
pat_key_test (const u_int8_t *key, u_int16_t bit)
{
    return(BIT_TEST(key[bit >> 8], (~bit & 0xff)));
}

/**
 * @brief
 * Given a node, determines the key length in bytes.
 *
 * @param[in]  node
 *     Pointer to patricia tree node
 *
 * @return 
 *     The key length, in bytes.
 */
static inline u_int16_t
patricia_length (patnode *node)
{
    return (((node->length) >> 8) + 1);
}

/**
 * @brief
 * Given the length of a key in bytes, converts it to patricia bit format.
 *
 * @param[in]  length
 *     Length of a key, in bytes
 *
 * @return 
 *     Patricia bit format or @c PAT_NOBIT if length is 0.
 */
static inline u_int16_t
patricia_length_to_bit (u_int16_t length)
{
    if (length) {
	return (((length - 1) << 8) | 0xff);
    }
    return (PAT_NOBIT);
}

/**
 * @brief
 * Finds an exact match for the specified key and key length.
 * 
 * @param[in] root
 *     Pointer to patricia tree root
 * @param[in] key_bytes
 *     Length of the key in bytes
 * @param[in] v_key
 *     Key to match
 *
 * @return 
 *     A pointer to the patnode containing the matching key;
 *     @c NULL if not found
 */
static inline patnode *
patricia_get_inline (patroot *root, u_int16_t key_bytes, const void *v_key)
{
    patnode *current;
    u_int16_t bit, bit_len;
    const u_int8_t *key = (const u_int8_t *)v_key;

    if (!key_bytes) {
	abort();
    }
    current = root->root;
    if (!current) {
	return(0);
    }

    /*
     * Waltz down the tree.  Stop when the bits appear to go backwards.
     */
    bit = PAT_NOBIT;
    bit_len = patricia_length_to_bit(key_bytes);
    while (bit < current->bit) {
	bit = current->bit;
	if (bit < bit_len && pat_key_test(key, bit)) {
	    current = current->right;
	} else {
	    current = current->left;
	}
    }

    /*
     * If the lengths don't match we're screwed.  Otherwise do a compare.
     */
    if (current->length != bit_len
	|| bcmp(patricia_key(root, current), key, key_bytes)) {
	return (0);
    }
    return (current);
}

/**
 * @brief
 * Determines if a patricia tree is empty.
 *
 * @param[in]  root
 *     Pointer to patricia tree root
 *
 * @return 
 *     1 if the tree is empty, 0 otherwise.
 */
static inline u_int8_t
patricia_isempty (patroot *root)
{
    return (root->root == NULL);
}


/**
 * @brief
 * Returns the sizeof for an element in a structure.
 */
 
#define STRUCT_SIZEOF(_structure, _element) \
           (sizeof(((_structure*)0)->_element))

/**
 * @brief
 * Returns the offset of @a _elt2 from the END of @a _elt1 within @a structure.
 */

#define STRUCT_OFFSET(_structure, _elt1, _elt2) \
           (offsetof(_structure, _elt2) - (offsetof(_structure, _elt1) + \
                                      STRUCT_SIZEOF(_structure, _elt1)))

/**
 * @brief
 * Macro to define an inline to map from a patnode entry back to the
 * containing data structure.
 *
 * This is just a handy way of defining the inline, which will return
 * @c NULL if the patnode pointer is @c NULL, or the enclosing structure
 * if not.
 *
 * The @c assert() will be removed by the compiler unless your code 
 * is broken -- this is quite useful as a way to validate that you've
 * given the right field for fieldname (a common mistake is to give
 * the KEY field instead of the NODE field).  It's harmless.
 */
#define PATNODE_TO_STRUCT(procname, structname, fieldname) \
    static inline structname * procname (patnode *ptr)\
    {\
        assert(STRUCT_SIZEOF(structname, fieldname) == sizeof(patnode));\
	if (ptr)\
	    return(QUIET_CAST(structname *, ((u_char *) ptr) - \
				    offsetof(structname, fieldname))); \
	return(NULL); \
     }

/**
 * @brief
 * Constant version of the macro to define an inline to map from a patnode 
 * entry back to the containing data structure.
 */
#define PATNODE_TO_CONS_STRUCT(procname, structname, fieldname)      \
static inline const structname *                                     \
procname (patnode *ptr)                                              \
{                                                                    \
    assert(STRUCT_SIZEOF(structname, fieldname) == sizeof(patnode)); \
    if (ptr) {                                                       \
        return ((const structname *) ((uchar *) ptr) -               \
            offsetof(structname, fieldname));                        \
    }                                                                \
    return NULL;                                                     \
}


/**
 * @brief
 * Initialize a patricia root with a little more 
 * compile-time checking. 
 */

#define PATRICIA_ROOT_INIT(_rootptr, _bool_key_is_ptr, _nodestruct, \
			   _nodeelement, _keyelt) \
           patricia_root_init(_rootptr, _bool_key_is_ptr,              \
                      STRUCT_SIZEOF(_nodestruct, _keyelt),             \
                      STRUCT_OFFSET(_nodestruct, _nodeelement, _keyelt))

/**
 * @internal
 *
 * @brief
 * Look up a node having the specified fixed length key.
 *
 * The key length provided at initialization time will be used.  For trees
 * with non-fixed lengths, patricia_get() should be used instead, as the length
 * will need to be specified.
 *
 * @param[in] root
 *     Pointer to patricia tree root
 * @param[in] key
 *     Pointer to key value
 *
 * @return
 *     @c NULL if a match is not found;
 *     otherwise a pointer to the matchin patricia tree node
 *
 * @sa patricia_get
 */
static inline patnode *
patricia_lookup(patroot *root, const void *key)
{
	return (patricia_get(root, root->key_bytes, key));
}

/**
 * @internal
 *
 * @brief
 * Given a fixed length key, return a node in the tree which is at least
 * as large as the key specified.
 *
 * The key length provided at initialization time will be used.  For trees
 * with non-fixed length keys, patricia_getnext() should be used instead, as
 * the length of the key will need to be specified.
 *
 * @param[in] root
 *     Pointer to patricia tree root
 * @param[in] key
 *     Pointer to key value
 *
 * @return
 *     A pointer to patricia tree node.
 */
static inline patnode *
patricia_lookup_geq(patroot *root, void *key)
{
    return (patricia_getnext(root, root->key_bytes, key, TRUE));
}

/*
 * pat_makebit
 */
extern const u_int8_t patricia_hi_bit_table[];

/**
 * @interal
 * @brief
 * Given a byte number and a bit mask, make a bit index.
 *
 * @param[in]  offset
 *     Offset byte number
 * @param[in]  bit_in_byte
 *     Bit within the byte
 *
 * @return 
 *     Bit index value
 */
static inline u_int16_t
pat_makebit (u_int16_t offset, u_int8_t bit_in_byte)
{
    return ((((offset) & 0xff) << 8) | ((~patricia_hi_bit_table[bit_in_byte]) & 0xff));
}

#ifdef __cplusplus
    }
}
#endif /* __cplusplus */

#endif	/* __JNX_PATRICIA_H__ */

