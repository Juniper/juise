/*
 * $Id$
 *
 * Copyright (c) 1996-2003, 2005-2006, 2011, 2015, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

/*
 * Include files
 */
#include <sys/types.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <libjuise/common/aux_types.h>
#include <libjuise/common/bits.h>
#include <libjuise/memory/memory.h>
#include <libjuise/memory/dmalloc.h>
#include "vatricia.h"

/*
 * This table contains a one bit mask of the highest order bit
 * set in a byte for each index value.
 */
const u_int8_t vatricia_hi_bit_table[256] = {
    0x00, 0x01, 0x02, 0x02, 0x04, 0x04, 0x04, 0x04,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
};

/*
 * Table to translate from a bit index to the mask part of a vatricia
 * bit number.
 */
const u_int8_t vatricia_bit_masks[8] = {
    0x7f, 0xbf, 0xdf, 0xef, 0xf7, 0xfb, 0xfd, 0xfe
};

/*
 * Mask for the bit index part of a prefix length.
 */
#define	VAT_PLEN_BIT_MASK		0x7

#define	VAT_PLEN_BYTE_MASK		(0xff << 3)

/*
 * Built-in memory allocator
 */
static void *
vat_dbm_memory_alloc (dbm_memory_t *dbmp, size_t size)
{
    return dbm_malloc(dbmp, size);
}

/*
 * Built-in deallocator
 */
static void
vat_dbm_memory_free (dbm_memory_t *dbmp, void *ptr)
{
    assert(ptr);

    dbm_free(dbmp, ptr);
}

/*
 * Built-in libc-based memory allocator
 */
static void *
vat_libc_alloc (size_t size)
{
    return malloc(size);
}

/*
 * Built-in libc-based deallocator
 */
static void
vat_libc_free (void *ptr)
{
    assert(ptr);

    free(ptr);
}

/*
 * Vatricia tree users can specify their own root alloc & free
 * functions if they desire. These are used ONLY to allocate
 * and free vat_root_t structures. They are NOT used for vat_node_t
 * structures (the caller is responsible for vat_node_ts). If the
 * user doesn't specify his own functions then the built-in
 * functions using malloc/free are used.
 */
static struct vatricia_alloc_s {
    vatricia_db_alloc_fn vat_db_alloc;
    vatricia_db_free_fn vat_db_free;
    vatricia_user_alloc_fn vat_user_alloc;
    vatricia_user_free_fn vat_user_free;
} vat_alloc_info = {
    vat_dbm_memory_alloc,
    vat_dbm_memory_free,
    vat_libc_alloc,
    vat_libc_free
};

void
vatricia_set_allocator (vatricia_db_alloc_fn my_db_alloc,
			vatricia_db_free_fn my_db_free,
			vatricia_user_alloc_fn my_user_alloc,
			vatricia_user_free_fn my_user_free)
{
    if (my_db_alloc)
	vat_alloc_info.vat_db_alloc = my_db_alloc;
    if (my_db_free)
	vat_alloc_info.vat_db_free = my_db_free;
    if (my_user_alloc)
	vat_alloc_info.vat_user_alloc = my_user_alloc;
    if (my_user_free)
	vat_alloc_info.vat_user_free = my_user_free;
}

static inline void *
vat_db_alloc (dbm_memory_t *dbmp, size_t size)
{
    return vat_alloc_info.vat_db_alloc(dbmp, size);
}

static inline void *
vat_db_calloc (dbm_memory_t *dbmp, size_t size)
{
    void *ptr = vat_alloc_info.vat_db_alloc(dbmp, size);
    if (ptr)
	bzero(ptr, size);
    return ptr;
}

static inline void
vat_db_free (dbm_memory_t *dbmp, void *ptr)
{
    return vat_alloc_info.vat_db_free(dbmp, ptr);
}

void *
vat_alloc (vat_handle_t *handle, size_t size)
{
    return vat_alloc_info.vat_db_alloc(handle->vhn_dbmp, size);
}

void *
vat_calloc (vat_handle_t *handle, size_t size)
{
    void *ptr = vat_alloc_info.vat_db_alloc(handle->vhn_dbmp, size);
    if (ptr)
	bzero(ptr, size);
    return ptr;
}

void
vat_free (vat_handle_t *handle, void *ptr)
{
    return vat_alloc_info.vat_db_free(handle->vhn_dbmp, ptr);
}

static inline void *
vat_user_alloc (size_t size)
{
    return vat_alloc_info.vat_user_alloc(size);
}

static inline void *
vat_user_calloc (size_t size)
{
    void *ptr = vat_alloc_info.vat_user_alloc(size);
    if (ptr)
	bzero(ptr, size);
    return ptr;
}

static inline void
vat_user_free (void *ptr)
{
    return vat_alloc_info.vat_user_free(ptr);
}

/*
 * Given the length of a key in bytes (not to exceed 256), return the
 * length in vatricia bit format.
 */
static inline u_int16_t
vat_plen_to_bit (u_int16_t plen)
{
    u_int16_t result;

    result = (plen & VAT_PLEN_BYTE_MASK) << 5;
    if (BIT_TEST(plen, VAT_PLEN_BIT_MASK)) {
	result |= vatricia_bit_masks[plen & VAT_PLEN_BIT_MASK];
    } else {
	result--;	/* subtract 0x100, or in 0xff */
    }
    return result;
}


/*
 * Given the length of a key in vatricia bit format, return its length
 * in bytes.
 */
#define	VAT_BIT_TO_LEN(bit)	(((bit) >> 8) + 1)

/*
 * Given a key and a key length, traverse a tree to find a match
 * possibility.
 */
static inline vat_node_t *
vatricia_search (dbm_memory_t *dbmp, vat_node_t *node,
		 u_int16_t keylen, const u_int8_t *key)
{
    u_int16_t bit = VAT_NOBIT;

    while (bit < node->vn_bit) {
	bit = node->vn_bit;
	if (bit < keylen && vat_key_test(key, bit)) {
	    node = vat_ptr(dbmp, node->vn_right);
	} else {
	    node = vat_ptr(dbmp, node->vn_left);
	}
    }
    return node;
}

/*
 * Given pointers to two keys, and a bit-formatted key length, return
 * the first bit of difference between the keys.
 */
static inline u_int16_t
vatricia_mismatch (const u_int8_t *k1, const u_int8_t *k2, u_int16_t bitlen)
{
    u_int16_t i, len;

    /*
     * Get the length of the key in bytes.
     */
    len = VAT_BIT_TO_LEN(bitlen);

    /*
     * Run through looking for a difference.
     */
    for (i = 0; i < len; i++) {
	if (k1[i] != k2[i]) {
	    bitlen = vat_makebit(i, k1[i] ^ k2[i]);
	    break;
	}
    }

    /*
     * Return what we found, or the original length if no difference.
     */
    return bitlen;
}

/*
 * Given a bit number and a starting node, find the leftmost leaf
 * in the (sub)tree.
 */
static inline vat_node_t *
vatricia_find_leftmost (dbm_memory_t *dbmp, u_int16_t bit,
			vat_node_t *node)
{
    while (bit < node->vn_bit) {
	bit = node->vn_bit;
	node = vat_ptr(dbmp, node->vn_left);
    }
    return node;
}


/*
 * Given a bit number and a starting node, find the rightmost leaf
 * in the (sub)tree.
 */
static inline vat_node_t *
vatricia_find_rightmost (dbm_memory_t *dbmp UNUSED, u_int16_t bit,
			 vat_node_t *node)
{
    while (bit < node->vn_bit) {
	bit = node->vn_bit;
	node = vat_ptr(dbmp, node->vn_right);
    }
    return node;
}


/*
 * vatricia_root_init()
 * Initialize a vatricia root node.  Allocate one if not provided.
 */
vat_root_t *
vatricia_root_init (dbm_memory_t *dbmp UNUSED, vat_root_t *root,
		    u_int16_t klen, u_int8_t off)
{
    assert(klen <= VAT_MAXKEY);
    if (klen == 0)
	klen = VAT_MAXKEY;

    if (root == NULL)
	root = vat_db_alloc(dbmp, sizeof(*root));

    if (root) {
	root->vr_root = VAT_PTR_NULL;
	root->vr_key_bytes = klen;
	root->vr_key_offset = off;
    }

    return root;
}

/*
 * vatricia_root_delete()
 * Delete the root of a tree.  The tree itself must be empty for this to
 * succeed.
 */
void
vatricia_root_delete (dbm_memory_t *dbmp UNUSED, vat_root_t *root)
{
    if (root) {
	assert(root->vr_root == VAT_PTR_NULL);
	vat_db_free(dbmp, root);
    }
}

/*
 * vatricia_node_in_tree
 * Return TRUE if a node is in the tree.  For now, this is only a syntactic
 * check.
 */
boolean
vatricia_node_in_tree (const vat_node_t *node)
{
    return (node->vn_bit != VAT_NOBIT) ||
	    (node->vn_right != VAT_PTR_NULL) ||
	    (node->vn_left != VAT_PTR_NULL);
}

/*
 * vatricia_node_init_length()
 * Passed a pointer to a vatricia node, initialize it.  Fortunately, this
 * is easy.
 */
void
vatricia_node_init_length (vat_node_t *node, u_int16_t key_bytes)
{
    if (key_bytes) {
	assert(key_bytes <= VAT_MAXKEY);
	node->vn_length = vatricia_length_to_bit(key_bytes);
    } else {
	node->vn_length = VAT_NOBIT;
    }
    node->vn_bit = VAT_NOBIT;
    node->vn_left = VAT_PTR_NULL;
    node->vn_right = VAT_PTR_NULL;
}

/*
 * vatricia_add
 * Add a node to a Vatricia tree.  Returns TRUE on success.
 */
boolean
vatricia_add (dbm_memory_t *dbmp, vat_root_t *root,
	      void *contents, vat_type_t type,
	      void *key_ptr, uint16_t key_bytes)
{
    vat_node_t *current;
    vat_offset_t *ptr;
    u_int16_t bit;
    u_int16_t diff_bit;
    const u_int8_t *key;

    if (key_bytes == 0)
	key_bytes = root->vr_key_bytes;

    vat_node_t *node = vat_db_calloc(dbmp, sizeof(*node));
    if (node == NULL)
	return FALSE;

    vatricia_node_init_length(node, key_bytes);

    vat_leaf_t *vlp = vat_db_calloc(dbmp, sizeof(*vlp));
    if (vlp == NULL) {
	vat_db_free(dbmp, node);
	return FALSE;
    }

    vat_ref_inc(&vlp->vl_refcount);
    vat_ref_inc(&node->vn_refcount);

    vlp->vl_contents = vat_off(dbmp, contents);
    vlp->vl_key = vat_off(dbmp, key_ptr ?: contents);
    vlp->vl_length = key_bytes;
    vlp->vl_type = type;

    node->vn_leaf = vat_off(dbmp, vlp);

    /*
     * If this is the first node in the tree, then it gets links to itself.
     * There is always exactly one node in the tree with VAT_NOBIT for the
     * bit number.  This node is always a leaf since this avoids ever testing
     * a bit with VAT_NOBIT, which leaves greater freedom in the choice of
     * bit formats.
     */
    if (root->vr_root == VAT_PTR_NULL) {
	node->vn_left = node->vn_right = vat_off(dbmp, node);
	root->vr_root = vat_off(dbmp, node);
	node->vn_bit = VAT_NOBIT;
	return TRUE;
    }

    /*
     * Start by waltzing down the tree to see if a duplicate (or a prefix
     * match) of the key is in the tree already.  If so, return FALSE.
     */
    key = vatricia_key(dbmp, root, node); /* Refer to key in database memory */
    assert(key == key_ptr);

    current = vatricia_search(dbmp, vat_ptr(dbmp, root->vr_root),
			      node->vn_length, key);

    /*
     * Find the first bit that differs from the node that we did find, to
     * the minimum length of the keys.  If the nodes match to this length
     * (i.e. one is a prefix of the other) we'll get a bit greater than or
     * equal to the minimum back, and we bail.
     */
    bit = (node->vn_length < current->vn_length)
	? node->vn_length : current->vn_length;

    diff_bit = vatricia_mismatch(key, vatricia_key(dbmp, root, current), bit);
    if (diff_bit >= bit) {
	vat_db_free(dbmp, vlp);
	vat_db_free(dbmp, node);
	return FALSE;
    }

    /*
     * Now waltz the tree again, looking for where the insertbit is in the
     * current branch.  Note that if there were parent pointers or a
     * convenient stack, we could back up.  Alas, we apply sweat...
     */
    bit = VAT_NOBIT;
    ptr = &root->vr_root;
    current = vat_ptr(dbmp, root->vr_root);

    while (bit < current->vn_bit && current->vn_bit < diff_bit) {
	bit = current->vn_bit;
	if (vat_key_test(key, bit)) {
	    ptr = &current->vn_right;
	    current = vat_ptr(dbmp, current->vn_right);
	} else {
	    ptr = &current->vn_left;
	    current = vat_ptr(dbmp, current->vn_left);
	}
    }

    /*
     * This is our insertion point.  Do the deed.
     */
    node->vn_bit = diff_bit;
    if (vat_key_test(key, diff_bit)) {
	node->vn_left = vat_off(dbmp, current);
	node->vn_right = vat_off(dbmp, node);
    } else {
	node->vn_right = vat_off(dbmp, current);
	node->vn_left = vat_off(dbmp, node);
    }

    *ptr = vat_off(dbmp, node);
    return TRUE;
}

/*
 * vatricia_delete()
 * Delete a node from a vatricia tree.
 */
boolean
vatricia_delete (dbm_memory_t *dbmp UNUSED, vat_root_t *root, vat_node_t *node)
{
    u_int16_t bit;
    const u_int8_t *key;
    vat_offset_t *downptr, *upptr, *parent;
    vat_node_t *current;
    
    /*
     * Is there even a tree?  Is the node in a tree?
     */
    assert(node->vn_left != VAT_PTR_NULL && node->vn_right != VAT_PTR_NULL);
    current = vat_ptr(dbmp, root->vr_root);
    if (!current) {
	return FALSE;
    }

    /*
     * Waltz down the tree, finding our node.  There should be two pointers
     * to the node: one internal, pointing to the node from above it, and
     * one at a leaf, pointing up to it from beneath.
     *
     * We want to set downptr to point to the internal pointer down to the
     * node.  The "upnode" is the node in the tree which has an "up" link
     * pointing to the node.  We want to set upptr to point to the pointer
     * to upnode [yes, that's right].
     */
    downptr = upptr = NULL;
    parent = &root->vr_root;
    bit = VAT_NOBIT;
    key = vatricia_key(dbmp, root, node);

    while (bit < current->vn_bit) {
	bit = current->vn_bit;
	if (current == node) {
	    downptr = parent;
	}
	upptr = parent;
	if (bit < node->vn_length && vat_key_test(key, bit)) {
	    parent = &current->vn_right;
	} else {
	    parent = &current->vn_left;
	}
	current = vat_ptr(dbmp, *parent);
    }

    /*
     * If the guy we found, `current', is not our node then it isn't
     * in the tree.
     */
    if (current != node) {
	return FALSE;
    }

    /*
     * If there's no upptr we're the only thing in the tree.
     * Otherwise we'll need to work a bit.
     */
    if (upptr == NULL) {
	assert(node->vn_bit == VAT_NOBIT);
	root->vr_root = VAT_PTR_NULL;
    } else {
	/*
	 * One pointer in the node upptr points at points at us,
	 * the other pointer points elsewhere.  Remove the node
	 * upptr points at from the tree in its internal node form
	 * by promoting the pointer which doesn't point at us.
	 * It is possible that this node is also `node', the node
	 * we're trying to remove, in which case we're all done.  If
	 * not, however, we'll catch that below.
	 */
	current = vat_ptr(dbmp, *upptr);
	if (parent == &current->vn_left) {
	    *upptr = current->vn_right;
	} else {
	    *upptr = current->vn_left;
	}
	if (!downptr) {
	    /*
	     * We were the no-bit node.  We make our parent the
	     * no-bit node.
	     */
	    assert(node->vn_bit == VAT_NOBIT);
	    current->vn_left = current->vn_right = vat_off(dbmp, current);
	    current->vn_bit = VAT_NOBIT;
	} else if (current != node) {
	    /*
	     * We were not our own `up node', which means we need to
	     * remove ourselves from the tree as in internal node.  Replace
	     * us with `current', which we freed above.
	     */
	    current->vn_left = node->vn_left;
	    current->vn_right = node->vn_right;
	    current->vn_bit = node->vn_bit;
	    *downptr = vat_off(dbmp, current);
	}
    }

    /*
     * Clean out the node.
     */
    node->vn_left = node->vn_right = VAT_PTR_NULL;
    node->vn_bit = VAT_NOBIT;

    vat_leaf_t *vlp = vatricia_leaf(dbmp, node);
    if (vat_ref_dec(&vlp->vl_refcount) == 0)
	vat_db_free(dbmp, vlp);

    if (vat_ref_dec(&node->vn_refcount) == 0)
	vat_db_free(dbmp, node);

    return TRUE;
}


/*
 * vatricia_find_next()
 * Given a node, find the lexical next node in the tree.  If the
 * node pointer is NULL the leftmost node in the tree is returned.
 * Returns NULL if the tree is empty or it falls off the right.  Asserts
 * if the node isn't in the tree.
 */
 
vat_node_t *
vatricia_find_next (dbm_memory_t *dbmp, vat_root_t *root, vat_node_t *node)
{
    u_int16_t bit;
    const u_int8_t *key;
    vat_node_t *current, *lastleft;

    /*
     * If there's nothing in the tree we're done.
     */
    current = vat_ptr(dbmp, root->vr_root);
    if (current == NULL) {
	assert(node == NULL);
	return NULL;
    }

    /*
     * If he didn't specify a node, return the leftmost guy.
     */
    if (node == NULL) {
	return vatricia_find_leftmost(dbmp, VAT_NOBIT, current);
    }

    /*
     * Search down the tree for the node.  Track where we last went
     * left, so we can go right from there.
     */
    lastleft = NULL;
    key = vatricia_key(dbmp, root, node);
    bit = VAT_NOBIT;
    while (bit < current->vn_bit) {
	bit = current->vn_bit;
	if (bit < node->vn_length && vat_key_test(key, bit)) {
	    current = vat_ptr(dbmp, current->vn_right);
	} else {
	    lastleft = current;
	    current = vat_ptr(dbmp, current->vn_left);
	}
    }
    assert(current == node);

    /*
     * If we found a left turn go right from there.  Otherwise barf.
     */
    if (lastleft) {
	return vatricia_find_leftmost(dbmp, lastleft->vn_bit,
				      vat_ptr(dbmp, lastleft->vn_right));
    }
    return NULL;
}

/* 
 * This is just a hack to let callers use const trees and 
 * receive back a const node..  The called functions are not const --
 * they can't be since they need to return non-const nodes to the 
 * caller -- even though they do not modify the contents of the 
 * tree.  However, if you're exposing the vatricias to other modules
 * that should be looked at, but not modified, these can help.. 
 */
 
const vat_node_t *
vatricia_cons_find_next (dbm_memory_t *dbmp UNUSED, const vat_root_t *root,
			 const vat_node_t *node)
{
    vat_root_t *r = const_drop(root);
    vat_node_t *n = const_drop(node);

    /* does not change or modify tree or node */
    return vatricia_find_next(dbmp, r, n); 
}

const vat_node_t *
vatricia_cons_find_prev (dbm_memory_t *dbmp UNUSED, const vat_root_t *root,
			 const vat_node_t *node)
{
    vat_root_t *r = const_drop(root);
    vat_node_t *n = const_drop(node);

    /* does not change or modify tree or node */    
    return vatricia_find_prev(dbmp, r, n); 
}

const vat_node_t *
vatricia_cons_get (dbm_memory_t *dbmp UNUSED, const vat_root_t *root,
		   const u_int16_t key_bytes, const void *key)
{
    vat_root_t *r = const_drop(root);

    /* does not change or modify tree or node */
    return vatricia_get(dbmp, r, key_bytes, key); 
}

const vat_node_t *
vatricia_cons_subtree_match (dbm_memory_t *dbmp UNUSED, const vat_root_t *root,
			     const u_int16_t prefix_len, const void *prefix)
{
    vat_root_t *r = const_drop(root);

    /* does not change or modify tree or node */    
    return vatricia_subtree_match(dbmp, r, prefix_len, prefix); 
}

const vat_node_t *
vatricia_cons_subtree_next (dbm_memory_t *dbmp UNUSED, const vat_root_t *root,
			    const vat_node_t *node, const u_int16_t prefix_len)
{
    vat_root_t *r = const_drop(root);
    vat_node_t *n = const_drop(node);

    /* does not change or modify tree or node */    
    return vatricia_subtree_next(dbmp, r, n, prefix_len); 
}


/*
 * vatricia_find_prev()
 * Given a node, find the lexical previous node in the tree.  If the
 * node pointer is NULL the rightmost node in the tree is returned.
 * Returns NULL if the tree is empty or it falls off the left.  Asserts
 * if the node isn't in the tree.
 */
vat_node_t *
vatricia_find_prev (dbm_memory_t *dbmp, vat_root_t *root,
		    vat_node_t *node)
{
    u_int16_t bit;
    const u_int8_t *key;
    vat_node_t *current, *lastright;

    /*
     * If there's nothing in the tree we're done.
     */
    current = vat_ptr(dbmp, root->vr_root);
    if (current == NULL) {
	assert(node == NULL);
	return NULL;
    }

    /*
     * If he didn't specify a node, return the rightmost guy.
     */
    if (node == NULL) {
	return vatricia_find_rightmost(dbmp, VAT_NOBIT, current);
    }

    /*
     * Search down the tree for the node.  Track where we last went
     * right, so we can go right from there.
     */
    lastright = NULL;
    key = vatricia_key(dbmp, root, node);
    bit = VAT_NOBIT;
    while (bit < current->vn_bit) {
	bit = current->vn_bit;
	if (bit < node->vn_length && vat_key_test(key, bit)) {
	    lastright = current;
	    current = vat_ptr(dbmp, current->vn_right);
	} else {
	    current = vat_ptr(dbmp, current->vn_left);
	}
    }
    assert(current == node);

    /*
     * If we found a right turn go right from there.  Otherwise barf.
     */
    if (lastright) {
	return vatricia_find_rightmost(dbmp, lastright->vn_bit,
				       vat_ptr(dbmp, lastright->vn_left));
    }
    return NULL;
}


/*
 * vatricia_subtree_match()
 * We're passed in a prefix length, in bits, and a pointer to that
 * many bits of prefix.  Return the leftmost guy for which this
 * is a prefix of the node's key.
 */
vat_node_t *
vatricia_subtree_match (dbm_memory_t *dbmp, vat_root_t *root,
			u_int16_t plen, const void *v_prefix)
{
    u_int16_t diff_bit, p_bit;
    vat_node_t *current;
    const u_int8_t *prefix = v_prefix;

    /*
     * If there's nothing in the tree, return NULL.
     */
    assert(plen && plen <= (VAT_MAXKEY * 8));

    if (root->vr_root == VAT_PTR_NULL) {
	return NULL;
    }

    /*
     * Okay, express the prefix length as a vatricia bit number
     * and search for someone.
     */
    p_bit = vat_plen_to_bit(plen);
    current = vatricia_search(dbmp, vat_ptr(dbmp, root->vr_root),
			      p_bit, prefix);

    /*
     * If the guy we found is shorter than the prefix length, we're
     * doomed (we could walk forward, but since we're guaranteed that
     * we'll never test a bit not in a key on the way there, we're sure
     * not to find any other matches).
     */
    if (p_bit > current->vn_length) {
	return NULL;
    }

    /*
     * Compare the key of the guy we found to our prefix.  If they
     * match to the prefix length return him, otherwise there is no match.
     */
    diff_bit = vatricia_mismatch(prefix,
				 vatricia_key(dbmp, root, current), p_bit);
    if (diff_bit < p_bit) {
	return NULL;
    }
    return current;
}


/*
 * vatricia_subtree_next()
 * Given a node in a subtree, and the prefix length in bits of the prefix
 * common to nodes in the subtree, return the lexical next node in the
 * subtree.  assert()'s if the node isn't in the tree.
 */
vat_node_t *
vatricia_subtree_next (dbm_memory_t *dbmp, vat_root_t *root,
		       vat_node_t *node, u_int16_t plen)
{
    const u_int8_t *prefix;
    u_int16_t bit, p_bit;
    vat_node_t *current, *lastleft;

    /*
     * Make sure this is reasonable.
     */
    current = vat_ptr(dbmp, root->vr_root);
    assert(plen && current);
    p_bit = vat_plen_to_bit(plen);
    assert(node->vn_length >= p_bit);

    prefix = vatricia_key(dbmp, root, node);
    bit = VAT_NOBIT;
    lastleft = NULL;
    while (bit < current->vn_bit) {
	bit = current->vn_bit;
	if (bit < node->vn_length && vat_key_test(prefix, bit)) {
	    current = vat_ptr(dbmp, current->vn_right);
	} else {
	    lastleft = current;
	    current = vat_ptr(dbmp, current->vn_left);
	}
    }

    /*
     * If we didn't go left, or the left turn was at a bit in the prefix,
     * we've fallen off the end of the subtree.  Otherwise step right
     * and return the leftmost guy over there.
     */
    assert(current == node);
    if (lastleft == NULL || lastleft->vn_bit < p_bit) {
	return NULL;
    }
    return vatricia_find_leftmost(dbmp, lastleft->vn_bit,
				  vat_ptr(dbmp, lastleft->vn_right));
}


/*
 * vatricia_get()
 * Given a key and its length, find a node which matches.
 */
vat_node_t *
vatricia_get (dbm_memory_t *dbmp UNUSED, vat_root_t *root,
	      u_int16_t key_bytes, const void *key)
{
    return vatricia_get_inline(dbmp, root, key_bytes, key);
}

/*
 * vatricia_getnext()
 * Find the next matching guy in the tree.  This is a classic getnext,
 * except that if we're told to we will return an exact match if we find
 * one.
 */
/**
 * Some more documentation for this function.
 *  Let's see what Doxygen does with it.
 */
vat_node_t *
vatricia_getnext (dbm_memory_t *dbmp UNUSED, vat_root_t *root,
		  u_int16_t klen, const void *v_key, boolean eq)
{
    u_int16_t bit, bit_len, diff_bit;
    vat_node_t *current, *lastleft, *lastright;
    const u_int8_t *key = v_key;

    assert(klen);

    /*
     * If nothing in tree, nothing to find.
     */
    current = vat_ptr(dbmp, root->vr_root);
    if (current == NULL) {
	return NULL;
    }

    /*
     * Search the tree looking for this prefix.  Note the last spot
     * at which we go left.
     */
    bit_len = vatricia_length_to_bit(klen);
    bit = VAT_NOBIT;
    lastright = lastleft = NULL;
    while (bit < current->vn_bit) {
	bit = current->vn_bit;
	if (bit < bit_len && vat_key_test(key, bit)) {
	    lastright = current;
	    current = vat_ptr(dbmp, current->vn_right);
	} else {
	    lastleft = current;
	    current = vat_ptr(dbmp, current->vn_left);
	}
    }

    /*
     * So far so good.  Determine where the first mismatch between
     * the guy we found and the key occurs.
     */
    bit = (current->vn_length > bit_len) ? bit_len : current->vn_length;
    diff_bit = vatricia_mismatch(key,
				 vatricia_key(dbmp, root, current), bit);

    /*
     * Three cases here.  Do them one by one.
     */
    if (diff_bit >= bit) {
	/*
	 * They match to at least the length of the shortest.  If the
	 * key is shorter, or if the key is equal and we've been asked
	 * to return that, we're golden.
	 */
	if (bit_len < current->vn_length
	    || (eq && bit_len == current->vn_length)) {
	    return current;
	}

	/*
	 * If none of the above, go right from `lastleft'.
	 */
    } else if (vat_key_test(key, diff_bit)) {
	/*
	 * The key is bigger than the guy we found.  We need to find
	 * somewhere that tested a bit less than diff_bit where we
	 * went left, and go right there instead.  `lastleft' will
	 * be the spot if it tests a bit less than diff_bit, otherwise
	 * we need to search again.
	 */
	if (lastleft && lastleft->vn_bit > diff_bit) {
	    bit = VAT_NOBIT;
	    current = vat_ptr(dbmp, root->vr_root);
	    lastleft = NULL;
	    while (bit < current->vn_bit && current->vn_bit < diff_bit) {
		bit = current->vn_bit;
		if (vat_key_test(key, bit)) {
		    current = vat_ptr(dbmp, current->vn_right);
		} else {
		    lastleft = current;
		    current = vat_ptr(dbmp, current->vn_left);
		}
	    }
	}
    } else {
	/*
	 * The key is smaller than the guy we found, so the guy we
	 * found is actually a candidate.  It may be the case, however,
	 * that this guy isn't the first larger node in the tree.  We
	 * know this is true if the last right turn tested a bit greater
	 * than the difference bit, in which case we search again from the
	 * top.
	 */
	if (lastright && lastright->vn_bit >= diff_bit) {
	    return vatricia_search(dbmp, vat_ptr(dbmp, root->vr_root),
				   diff_bit, key);
	}
	return current;
    }

    /*
     * The first two cases come here.  In either case, if we've got
     * a `lastleft' take a right turn there, otherwise return nothing.
     */
    if (lastleft) {
	return vatricia_find_leftmost(dbmp, lastleft->vn_bit,
				      vat_ptr(dbmp, lastleft->vn_right));
    }
    return NULL;
}

int
vatricia_compare_nodes (dbm_memory_t *dbmp UNUSED, vat_root_t *root,
			vat_node_t *node1, vat_node_t *node2)
{
    u_int16_t bit;
    u_int16_t diff_bit;
    const u_int8_t *key_1, *key_2;
    
    bit = (node1->vn_length < node2->vn_length) ? node1->vn_length : node2->vn_length;
    key_1 = vatricia_key(dbmp, root, node1);
    key_2 = vatricia_key(dbmp, root, node2);
    
    diff_bit = vatricia_mismatch(key_1, key_2, bit);
    
    if (diff_bit >= bit)
	return 0;
    
    if (vat_key_test(key_1, diff_bit)) {
	return 1;
    }
    
    return -1;
}

vat_header_t *
vat_check_header (dbm_memory_t *dbmp)
{
    vat_header_t *vhp = dbm_header(dbmp);

    if (vhp->vh_version == 0) {
	vhp->vh_version = htonl(VAT_HEADER_VERSION);
	vhp->vh_magic = htonl(VAT_HEADER_MAGIC);

    } else {
	if (vhp->vh_version != htonl(VAT_HEADER_VERSION))
	    assert(!"bad version number");
	if (vhp->vh_magic != htonl(VAT_HEADER_MAGIC))
	    assert(!"bad magic number");
    }

    return vhp;
}

/**
 * Open a vatricia tree database
 */
vat_handle_t *
vat_open (const char *filename, size_t size, unsigned flags)
{
    if (flags & VATF_CREATE)
	unlink(filename);
    unsigned long addr = 0x200000000000;

    unsigned dbm_flags = DBMF_CREATE | DBMF_WRITE| DBMF_FIXED;
    dbm_memory_t *dbmp = dbm_open(filename, (caddr_t) addr,
				  sizeof(vat_header_t), size, &dbm_flags);
    if (dbmp == 0)
	return NULL;

    dbm_malloc_init(dbmp);
    vat_check_header(dbmp);

    vat_header_t *vhp = vat_check_header(dbmp);
    vat_handle_t *handle = vat_user_calloc(sizeof(*handle));
    if (handle == NULL) {
	dbm_close(dbmp);
	return NULL;
    }

    handle->vhn_dbmp = dbmp;
    handle->vhn_header = vhp;

    int len = strlen(filename) + 1;
    char *cp = vat_user_alloc(len); /* Needs to be in user space */
    if (cp) {
	memcpy(cp, filename, len);
	handle->vhn_filename = cp;
    }

    return handle;
}

/**
 * Close a vatricia tree database
 */
void
vat_close (vat_handle_t *handle)
{
    if (handle) {
	if (handle->vhn_filename)
	    vat_user_free(handle->vhn_filename); /* User space */
	vat_user_free(handle);
    }
}

vat_tree_t *
vat_tree_new (vat_handle_t *handle, vat_generation_t base UNUSED,
	      size_t key_offset)
{
    dbm_memory_t *dbmp = handle->vhn_dbmp;
    vat_root_t *root = vat_ptr(dbmp, handle->vhn_header->vh_root);

    if (root == NULL) {
	/*
	 * There's no root, so we malloc and init it ourselves.
	 */
	root = vat_db_calloc(dbmp, sizeof(*root));
	if (root) {
	    handle->vhn_header->vh_root = vat_off(dbmp, root);
	    vatricia_root_init(dbmp, root, VAT_MAXKEY, key_offset);
	    root->vr_generation = VAT_GENERATION_NULL;
	}
    }

    vat_tree_t *vtp = vat_user_calloc(sizeof(*vtp));
    if (vtp == NULL)
	return NULL;

    vtp->vt_handle = handle;
    vtp->vt_generation = VAT_GENERATION_NULL;
    vtp->vt_root = root;

    return vtp;
}

