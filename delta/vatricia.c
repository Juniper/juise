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
#include <string.h>

#include <libjuise/common/aux_types.h>
#include <libjuise/common/bits.h>
#include <libjuise/data/vatricia.h>

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
 * vatricia_root_alloc
 *
 * Built-in root allocator.
 */
static vatroot *
vatricia_root_alloc (void)
{
    return (malloc(sizeof(vatroot)));
}

/*
 * vatricia_root_free
 *
 * Built-in root deallocator.
 */
static void
vatricia_root_free (vatroot *root)
{
    assert(root);

    free(root);
}

/*
 * Vatricia tree users can specify their own root alloc & free
 * functions if they desire. These are used ONLY to allocate
 * and free vatroot structures. They are NOT used for vatnode
 * structures (the caller is responsible for vatnodes). If the
 * user doesn't specify his own functions then the built-in
 * functions using malloc/free are used.
 */
static struct vatricia_root_alloc_s {
    vatricia_root_alloc_fn vat_root_alloc;
    vatricia_root_free_fn vat_root_free;
} alloc_info = {
    vatricia_root_alloc,
    vatricia_root_free
};

void
vatricia_set_allocator (vatricia_root_alloc_fn my_alloc,
                        vatricia_root_free_fn my_free)
{
    assert(my_alloc);
    assert(my_free);
    
    alloc_info.vat_root_alloc = my_alloc;
    alloc_info.vat_root_free = my_free;
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
    return (result);
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
static inline vatnode *
vatricia_search (vatnode *node, u_int16_t keylen, const u_int8_t *key)
{
    u_int16_t bit = VAT_NOBIT;

    while (bit < node->bit) {
	bit = node->bit;
	if (bit < keylen && vat_key_test(key, bit)) {
	    node = node->right;
	} else {
	    node = node->left;
	}
    }
    return (node);
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
    return (bitlen);
}

/*
 * Given a bit number and a starting node, find the leftmost leaf
 * in the (sub)tree.
 */
static inline vatnode *
vatricia_find_leftmost (u_int16_t bit, vatnode *node)
{
    while (bit < node->bit) {
	bit = node->bit;
	node = node->left;
    }
    return (node);
}


/*
 * Given a bit number and a starting node, find the rightmost leaf
 * in the (sub)tree.
 */
static inline vatnode *
vatricia_find_rightmost (u_int16_t bit, vatnode *node)
{
    while (bit < node->bit) {
	bit = node->bit;
	node = node->right;
    }
    return (node);
}


/*
 * vatricia_root_init()
 * Initialize a vatricia root node.  Allocate one if not provided.
 */
vatroot *
vatricia_root_init (vatroot *root, boolean is_ptr, u_int16_t klen, u_int8_t off)
{
    assert(klen && klen <= VAT_MAXKEY);

    if (!root) {
	root = alloc_info.vat_root_alloc();
    }
    if (root) {
	root->root = NULL;
	root->key_bytes = klen;
	root->key_offset = off;
	root->key_is_ptr = (is_ptr ? 1 : 0);
    }
    return (root);
}

/*
 * vatricia_root_delete()
 * Delete the root of a tree.  The tree itself must be empty for this to
 * succeed.
 */
void
vatricia_root_delete (vatroot *root)
{
    if (root) {
	assert(root->root == NULL);
	alloc_info.vat_root_free(root);
    }
}

/*
 * vatricia_node_in_tree
 * Return TRUE if a node is in the tree.  For now, this is only a syntactic
 * check.
 */
boolean
vatricia_node_in_tree (const vatnode *node)
{
    return ((node->bit != VAT_NOBIT) ||
	    (node->right != NULL) ||
	    (node->left != NULL));
}

/*
 * vatricia_node_init_length()
 * Passed a pointer to a vatricia node, initialize it.  Fortunately, this
 * is easy.
 */
void
vatricia_node_init_length (vatnode *node, u_int16_t key_bytes)
{
    if (key_bytes) {
	assert(key_bytes <= VAT_MAXKEY);
	node->length = vatricia_length_to_bit(key_bytes);
    } else {
	node->length = VAT_NOBIT;
    }
    node->bit = VAT_NOBIT;
    node->left = NULL;
    node->right = NULL;
}

/*
 * vatricia_add
 * Add a node to a Vatricia tree.  Returns TRUE on success.
 */
boolean
vatricia_add (vatroot *root, vatnode *node)
{
    vatnode *current;
    vatnode **ptr;
    u_int16_t bit;
    u_int16_t diff_bit;
    const u_int8_t *key;

    /*
     * Make sure this node is not in a tree already.
     */
    assert((node->bit == VAT_NOBIT) &&
	   (node->right == NULL) &&
	   (node->left == NULL));
  
    if (node->length == VAT_NOBIT) {
	node->length = vatricia_length_to_bit(root->key_bytes);
    }

    /*
     * If this is the first node in the tree, then it gets links to itself.
     * There is always exactly one node in the tree with VAT_NOBIT for the
     * bit number.  This node is always a leaf since this avoids ever testing
     * a bit with VAT_NOBIT, which leaves greater freedom in the choice of
     * bit formats.
     */
    if (root->root == NULL) {
	root->root = node->left = node->right = node;
	node->bit = VAT_NOBIT;
	return(TRUE);
    }

    /*
     * Start by waltzing down the tree to see if a duplicate (or a prefix
     * match) of the key is in the tree already.  If so, return FALSE.
     */
    key = vatricia_key(root, node);
    current = vatricia_search(root->root, node->length, key);

    /*
     * Find the first bit that differs from the node that we did find, to
     * the minimum length of the keys.  If the nodes match to this length
     * (i.e. one is a prefix of the other) we'll get a bit greater than or
     * equal to the minimum back, and we bail.
     */
    bit = (node->length < current->length) ? node->length : current->length;
    diff_bit = vatricia_mismatch(key, vatricia_key(root, current), bit);
    if (diff_bit >= bit) {
	return(FALSE);
    }

    /*
     * Now waltz the tree again, looking for where the insertbit is in the
     * current branch.  Note that if there were parent pointers or a
     * convenient stack, we could back up.  Alas, we apply sweat...
     */
    bit = VAT_NOBIT;
    current = root->root;
    ptr = &root->root;
    while (bit < current->bit && current->bit < diff_bit) {
	bit = current->bit;
	if (vat_key_test(key, bit)) {
	    ptr = &current->right;
	    current = current->right;
	} else {
	    ptr = &current->left;
	    current = current->left;
	}
    }

    /*
     * This is our insertion point.  Do the deed.
     */
    node->bit = diff_bit;
    if (vat_key_test(key, diff_bit)) {
	node->left = current;
	node->right = node;
    } else {
	node->right = current;
	node->left = node;
    }
    *ptr = node;
    return(TRUE);
}

/*
 * vatricia_delete()
 * Delete a node from a vatricia tree.
 */
boolean
vatricia_delete (vatroot *root, vatnode *node)
{
    u_int16_t bit;
    const u_int8_t *key;
    vatnode **downptr, **upptr, **parent, *current;
    
    /*
     * Is there even a tree?  Is the node in a tree?
     */
    assert(node->left && node->right);
    current = root->root;
    if (!current) {
	return (FALSE);
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
    parent = &root->root;
    bit = VAT_NOBIT;
    key = vatricia_key(root, node);

    while (bit < current->bit) {
	bit = current->bit;
	if (current == node) {
	    downptr = parent;
	}
	upptr = parent;
	if (bit < node->length && vat_key_test(key, bit)) {
	    parent = &current->right;
	} else {
	    parent = &current->left;
	}
	current = *parent;
    }

    /*
     * If the guy we found, `current', is not our node then it isn't
     * in the tree.
     */
    if (current != node) {
	return (FALSE);
    }

    /*
     * If there's no upptr we're the only thing in the tree.
     * Otherwise we'll need to work a bit.
     */
    if (upptr == NULL) {
	assert(node->bit == VAT_NOBIT);
	root->root = NULL;
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
	current = *upptr;
	if (parent == &current->left) {
	    *upptr = current->right;
	} else {
	    *upptr = current->left;
	}
	if (!downptr) {
	    /*
	     * We were the no-bit node.  We make our parent the
	     * no-bit node.
	     */
	    assert(node->bit == VAT_NOBIT);
	    current->left = current->right = current;
	    current->bit = VAT_NOBIT;
	} else if (current != node) {
	    /*
	     * We were not our own `up node', which means we need to
	     * remove ourselves from the tree as in internal node.  Replace
	     * us with `current', which we freed above.
	     */
	    current->left = node->left;
	    current->right = node->right;
	    current->bit = node->bit;
	    *downptr = current;
	}
    }

    /*
     * Clean out the node.
     */
    node->left = node->right = NULL;
    node->bit = VAT_NOBIT;
    return(TRUE);
}


/*
 * vatricia_find_next()
 * Given a node, find the lexical next node in the tree.  If the
 * node pointer is NULL the leftmost node in the tree is returned.
 * Returns NULL if the tree is empty or it falls off the right.  Asserts
 * if the node isn't in the tree.
 */
 
vatnode *
vatricia_find_next (vatroot *root, vatnode *node)
{
    u_int16_t bit;
    const u_int8_t *key;
    vatnode *current, *lastleft;

    /*
     * If there's nothing in the tree we're done.
     */
    current = root->root;
    if (current == NULL) {
	assert(node == NULL);
	return (NULL);
    }

    /*
     * If he didn't specify a node, return the leftmost guy.
     */
    if (node == NULL) {
	return (vatricia_find_leftmost(VAT_NOBIT, current));
    }

    /*
     * Search down the tree for the node.  Track where we last went
     * left, so we can go right from there.
     */
    lastleft = NULL;
    key = vatricia_key(root, node);
    bit = VAT_NOBIT;
    while (bit < current->bit) {
	bit = current->bit;
	if (bit < node->length && vat_key_test(key, bit)) {
	    current = current->right;
	} else {
	    lastleft = current;
	    current = current->left;
	}
    }
    assert(current == node);

    /*
     * If we found a left turn go right from there.  Otherwise barf.
     */
    if (lastleft) {
	return (vatricia_find_leftmost(lastleft->bit, lastleft->right));
    }
    return (NULL);
}

/* 
 * This is just a hack to let callers use const trees and 
 * receive back a const node..  The called functions are not const --
 * they can't be since they need to return non-const nodes to the 
 * caller -- even though they do not modify the contents of the 
 * tree.  However, if you're exposing the vatricias to other modules
 * that should be looked at, but not modified, these can help.. 
 */
 
const vatnode *
vatricia_cons_find_next (const vatroot *root, const vatnode *node)
{
    vatroot *r = const_drop(root);
    vatnode *n = const_drop(node);

    /* does not change or modify tree or node */
    return vatricia_find_next(r, n); 
}

const vatnode *
vatricia_cons_find_prev (const vatroot *root, const vatnode *node)
{
    vatroot *r = const_drop(root);
    vatnode *n = const_drop(node);

    /* does not change or modify tree or node */    
    return vatricia_find_prev(r, n); 
}

const vatnode *
vatricia_cons_get (const vatroot *root, const u_int16_t key_bytes,
		   const void *key)
{
    vatroot *r = const_drop(root);

    /* does not change or modify tree or node */
    return vatricia_get(r, key_bytes, key); 
}

const vatnode *
vatricia_cons_subtree_match (const vatroot *root, const u_int16_t prefix_len,
			     const void *prefix)
{
    vatroot *r = const_drop(root);

    /* does not change or modify tree or node */    
    return vatricia_subtree_match(r, prefix_len, prefix); 
}

const vatnode *
vatricia_cons_subtree_next (const vatroot *root, const vatnode *node,
			    const u_int16_t prefix_len)
{
    vatroot *r = const_drop(root);
    vatnode *n = const_drop(node);

    /* does not change or modify tree or node */    
    return vatricia_subtree_next(r, n, prefix_len); 
}


/*
 * vatricia_find_prev()
 * Given a node, find the lexical previous node in the tree.  If the
 * node pointer is NULL the rightmost node in the tree is returned.
 * Returns NULL if the tree is empty or it falls off the left.  Asserts
 * if the node isn't in the tree.
 */
vatnode *
vatricia_find_prev (vatroot *root, vatnode *node)
{
    u_int16_t bit;
    const u_int8_t *key;
    vatnode *current, *lastright;

    /*
     * If there's nothing in the tree we're done.
     */
    current = root->root;
    if (current == NULL) {
	assert(node == NULL);
	return (NULL);
    }

    /*
     * If he didn't specify a node, return the rightmost guy.
     */
    if (node == NULL) {
	return (vatricia_find_rightmost(VAT_NOBIT, current));
    }

    /*
     * Search down the tree for the node.  Track where we last went
     * right, so we can go right from there.
     */
    lastright = NULL;
    key = vatricia_key(root, node);
    bit = VAT_NOBIT;
    while (bit < current->bit) {
	bit = current->bit;
	if (bit < node->length && vat_key_test(key, bit)) {
	    lastright = current;
	    current = current->right;
	} else {
	    current = current->left;
	}
    }
    assert(current == node);

    /*
     * If we found a right turn go right from there.  Otherwise barf.
     */
    if (lastright) {
	return (vatricia_find_rightmost(lastright->bit, lastright->left));
    }
    return (NULL);
}


/*
 * vatricia_subtree_match()
 * We're passed in a prefix length, in bits, and a pointer to that
 * many bits of prefix.  Return the leftmost guy for which this
 * is a prefix of the node's key.
 */
vatnode *
vatricia_subtree_match (vatroot *root, u_int16_t plen, const void *v_prefix)
{
    u_int16_t diff_bit, p_bit;
    vatnode *current;
    const u_int8_t *prefix = v_prefix;

    /*
     * If there's nothing in the tree, return NULL.
     */
    assert(plen && plen <= (VAT_MAXKEY * 8));

    if (root->root == NULL) {
	return (NULL);
    }

    /*
     * Okay, express the prefix length as a vatricia bit number
     * and search for someone.
     */
    p_bit = vat_plen_to_bit(plen);
    current = vatricia_search(root->root, p_bit, prefix);

    /*
     * If the guy we found is shorter than the prefix length, we're
     * doomed (we could walk forward, but since we're guaranteed that
     * we'll never test a bit not in a key on the way there, we're sure
     * not to find any other matches).
     */
    if (p_bit > current->length) {
	return (NULL);
    }

    /*
     * Compare the key of the guy we found to our prefix.  If they
     * match to the prefix length return him, otherwise there is no match.
     */
    diff_bit = vatricia_mismatch(prefix, vatricia_key(root, current), p_bit);
    if (diff_bit < p_bit) {
	return (NULL);
    }
    return (current);
}


/*
 * vatricia_subtree_next()
 * Given a node in a subtree, and the prefix length in bits of the prefix
 * common to nodes in the subtree, return the lexical next node in the
 * subtree.  assert()'s if the node isn't in the tree.
 */
vatnode *
vatricia_subtree_next (vatroot *root, vatnode *node, u_int16_t plen)
{
    const u_int8_t *prefix;
    u_int16_t bit, p_bit;
    vatnode *current, *lastleft;

    /*
     * Make sure this is reasonable.
     */
    current = root->root;
    assert(plen && current);
    p_bit = vat_plen_to_bit(plen);
    assert(node->length >= p_bit);

    prefix = vatricia_key(root, node);
    bit = VAT_NOBIT;
    lastleft = NULL;
    while (bit < current->bit) {
	bit = current->bit;
	if (bit < node->length && vat_key_test(prefix, bit)) {
	    current = current->right;
	} else {
	    lastleft = current;
	    current = current->left;
	}
    }

    /*
     * If we didn't go left, or the left turn was at a bit in the prefix,
     * we've fallen off the end of the subtree.  Otherwise step right
     * and return the leftmost guy over there.
     */
    assert(current == node);
    if (lastleft == NULL || lastleft->bit < p_bit) {
	return (NULL);
    }
    return (vatricia_find_leftmost(lastleft->bit, lastleft->right));
}


/*
 * vatricia_get()
 * Given a key and its length, find a node which matches.
 */
vatnode *
vatricia_get (vatroot *root, u_int16_t key_bytes, const void *key)
{
    return (vatricia_get_inline(root, key_bytes, key));
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
vatnode *
vatricia_getnext (vatroot *root, u_int16_t klen, const void *v_key, boolean eq)
{
    u_int16_t bit, bit_len, diff_bit;
    vatnode *current, *lastleft, *lastright;
    const u_int8_t *key = v_key;

    assert(klen);

    /*
     * If nothing in tree, nothing to find.
     */
    current = root->root;
    if (current == NULL) {
	return (NULL);
    }

    /*
     * Search the tree looking for this prefix.  Note the last spot
     * at which we go left.
     */
    bit_len = vatricia_length_to_bit(klen);
    bit = VAT_NOBIT;
    lastright = lastleft = NULL;
    while (bit < current->bit) {
	bit = current->bit;
	if (bit < bit_len && vat_key_test(key, bit)) {
	    lastright = current;
	    current = current->right;
	} else {
	    lastleft = current;
	    current = current->left;
	}
    }

    /*
     * So far so good.  Determine where the first mismatch between
     * the guy we found and the key occurs.
     */
    bit = (current->length > bit_len) ? bit_len : current->length;
    diff_bit = vatricia_mismatch(key, vatricia_key(root, current), bit);

    /*
     * Three cases here.  Do them one by one.
     */
    if (diff_bit >= bit) {
	/*
	 * They match to at least the length of the shortest.  If the
	 * key is shorter, or if the key is equal and we've been asked
	 * to return that, we're golden.
	 */
	if (bit_len < current->length || (eq && bit_len == current->length)) {
	    return (current);
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
	if (lastleft && lastleft->bit > diff_bit) {
	    bit = VAT_NOBIT;
	    current = root->root;
	    lastleft = NULL;
	    while (bit < current->bit && current->bit < diff_bit) {
		bit = current->bit;
		if (vat_key_test(key, bit)) {
		    current = current->right;
		} else {
		    lastleft = current;
		    current = current->left;
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
	if (lastright && lastright->bit >= diff_bit) {
	    return (vatricia_search(root->root, diff_bit, key));
	}
	return (current);
    }

    /*
     * The first two cases come here.  In either case, if we've got
     * a `lastleft' take a right turn there, otherwise return nothing.
     */
    if (lastleft) {
	return (vatricia_find_leftmost(lastleft->bit, lastleft->right));
    }
    return (NULL);
}

int
vatricia_compare_nodes (vatroot *root, vatnode* node1, vatnode* node2)
{
    u_int16_t bit;
    u_int16_t diff_bit;
    const u_int8_t *key_1, *key_2;
    
    bit = (node1->length < node2->length) ? node1->length : node2->length;
    key_1 = vatricia_key(root, node1);
    key_2 = vatricia_key(root, node2);
    
    diff_bit = vatricia_mismatch(key_1, key_2, bit);
    
    if (diff_bit >= bit)
	return 0;
    
    if (vat_key_test(key_1, diff_bit)) {
	return 1;
    }
    
    return -1;
}

#ifdef TESTING

#define MAXNODECOUNT 100000
#define OPREPORT     100000
#define BHA	     1000000000		/* One Sagan */

typedef struct testnode_ {
    int dummy1;				/* Test worst case */
    vatnode vatricia;			/* Our node */
    int key;				/* Our key */
    int dummy2;				/* More worst case */
} testnode;

static inline testnode *
vat_to_test (vatnode *node)
{
    static testnode foo;
    testnode *result;

    result = (testnode *) ((int) node -
			 ((int)&foo.vatricia - (int)&foo));
    return(result);
}

/*
 * vatricia_lookup_random
 * Lookup a random leaf in the tree.
 */

vatnode *
vatricia_lookup_random (vatroot *root)
{
    vatnode *current = root->first;
    u_short lasttest = VAT_NOBIT;
  
    if (!current) {
	return(NULL);
    }

    /*
     * Waltz down the tree.  Stop when the bits appear to go backwards.
     */
    do {
	lasttest = current->bit;
	if (grand(2)) {
	    current = current->right;
	} else {
	    current = current->left;
	}
    } while (current->bit > lasttest);

    return(current);
}

typedef int vatricia_callback (vatnode *node);

/*
 * vat_traversal_internal
 * Internal recursive (debugging only!) function to do a traversal.
 */

int vat_traversal_internal (vatnode *node, vatricia_callback *callback)
{
    int result;

    if (node->left->bit > node->bit) {
	result = vat_traversal_internal(node->left, callback);
    } else {
	result = (*callback)(node->left);
    }
    if (!result) {
	return(FALSE);
    }
    
    if (node->right->bit > node->bit) {
	result = vat_traversal_internal(node->right, callback);
    } else {
	result = (*callback)(node->right);
    }
    if (!result) {
	return(FALSE);
    }
}

/*
 * vatricia_traversal
 * Traverse the tree in key order, calling the callback function once per
 * key.  Abort the traversal function if the callback function returns
 * FALSE.  Returns FALSE if terminated.  The root gets called back _twice_.
 */

int
vatricia_traversal (vatroot *root, vatricia_callback *callback)
{
    if (!root->first) {
	return(TRUE);
    }
    return(vat_traversal_internal(root->first, callback));
}

static long test_count;

int
test_callback (vatnode *node)
{
    test_count++;
    return(TRUE);
}

#define SPECIAL 0xe6d364f1

main (int argc, char *argv[])
{
    vatroot *root;
    testnode *test;
    vatnode *node, *next, *prev;
    int i;
    long adds, dels, total, badds, bdels;
    int special_on, special_key;

    root = vatricia_init_root(sizeof(int));
    if (!root) {
	printf("root malloc failed\n");
	exit(1);
    }

    adds = 0;
    dels = 0;
    total = 0;
    badds = 0;
    bdels = 0;
    special_on = FALSE;
    special_key = SPECIAL;
    while (TRUE) {
	switch(grand(4)) {
	case 0:				/* Add a node */
	default:
	    if (total >= MAXNODECOUNT) {
		break;
	    }
	    test = malloc(sizeof(testnode));
	    if (!test) {
		printf("node malloc failed\n");
		exit(1);
	    }
	    vatricia_node_init(&test->vatricia);

	    /*
	     * Generate a key not in the tree.
	     */
	    do {
		test->key = grand_log2(32);
	    } while (vatricia_lookup_inline(root, (u_char *)&test->key));

	    if (test->key == special_key) {
		special_on = TRUE;
	    }

	    if (!vatricia_add(root, &test->vatricia)) {

		printf("node add failed\n");
		exit(1);
	    }

	    if (vatricia_lookup_inline(root, (u_char *)&test->key) !=
		&test->vatricia) {
		printf("lookup after add failed, key %x\n",
		       test->key);
		exit(1);
	    }
	    total++;
	    adds++;
	    if (adds == BHA) {
		badds++;
		adds = 0;
	    }

	    if ((adds % OPREPORT) == 0) {
		printf("%ub %u adds, %u total\n", badds, adds, total);
	    }

	    if (special_on) {
		if (vatricia_lookup_inline(root,
					   (u_char *)&special_key) == NULL)
		{
		    printf("special failure after add, key %x\n",
			   test->key);
		    exit(1);
		}
	    }
	    break;
	case 1:				/* Do a lookup */
	    node = vatricia_lookup_random(root);
	    if (!node) {
		assert(total == 0);
		break;
	    }
	    if (vatricia_lookup_inline(root, (u_char *)&node->key) !=
		node) {
		printf("lookup failed, key %x\n", vat_to_test(node)->key);
		exit(1);
	    }

	    i = grand_log2(32);		/* Test lookup_geq */
	    node = vatricia_lookup_geq(root, (u_char *)&i);
	    if (node) {
		if (i == vat_to_test(node)->key) {
		    break;
		} else if (ntohl(vat_to_test(node)->key) < ntohl(i)) {
		    printf("geq failure 1\n");
		    exit(1);
		}
		prev = vatricia_get_previous(root, node);
		if (!prev) {
		    break;
		}
		if (ntohl(vat_to_test(prev)->key) < ntohl(i)) {
		    break;
		}
		printf("geq failure 2\n");
		exit(1);
	    } else {
		node = vatricia_lookup_greatest(root);
		if (!node) {
		    break;
		}
		if (ntohl(vat_to_test(node)->key) >= ntohl(i)) {
		    printf("geq failure 3\n");
		    exit(1);
		}
	    }
	    break;
	    
	case 2:				/* Do a delete */
	    node = vatricia_lookup_random(root);
	    if (!node) {
		assert(total == 0);
		break;
	    }
	    if (!vatricia_delete(root, node)) {
		printf("delete failed, key %x\n", node->key);
		exit(1);
	    }

	    if (special_on) {
		if (vat_to_test(node)->key == special_key) {
		    special_on = FALSE;
		} else {
		    if (vatricia_lookup_inline(root,
					       (u_char *)&special_key) == NULL)
		    {
			printf("special failure after delete, key %x\n",
			       vat_to_test(node)->key);
			exit(1);
		    }
		}
	    }
	    free(vat_to_test(node));
	    dels++;
	    total--;
	    if (dels == BHA) {
		bdels++;
		dels = 0;
	    }
	    if ((dels % OPREPORT) == 0) {
		printf("%ub %u dels, %u total\n", bdels, dels, total);
	    }

	    break;
	case 3:				/* Tree traversal */
	    test_count = 0;
	    vatricia_traversal(root, test_callback);
	    if (test_count) {
		test_count--;		/* Don't count the root twice */
	    }
	    if (test_count != total) {
		printf("%u total, %u found\n", total, test_count);
		exit(1);
	    }

	    /*
	     * Test get next.
	     */
	    test_count = 0;
	    node = vatricia_lookup_least(root);
	    if (node) {
		test_count++;
		prev = vatricia_get_previous(root, node);
		if (prev) {
		    printf("node previous to first");
		    exit(1);
		}
		    
/*		printf("least %x, total %d\n", vat_to_test(node)->key,
		total); */
		for (next = vatricia_get_next(root, node);
		     next;
		     node = next, next = vatricia_get_next(root, next)) {
		    test_count++;
		    if (ntohl(vat_to_test(node)->key) >=
			ntohl(vat_to_test(next)->key)) {
			printf("out of order %x %x\n",
			       vat_to_test(node)->key,
			       vat_to_test(next)->key);
			exit(1);
		    }
		    prev = vatricia_get_previous(root, next);
		    if (prev != node) {
			printf("previous not same as get previous");
			exit(1);
		    }
/*		    printf("next %x\n", vat_to_test(next)->key); */
		}
	    }
	    if (test_count != total) {
		printf("%u total, %u found via get next\n", total, test_count);
		exit(1);
	    }
	    break;
	}

    }
    exit(0);
}
#endif
					
