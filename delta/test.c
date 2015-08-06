#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#if 0
#include <sys/atomic.h>
#endif
#include <sys/queue.h>

/*
 * Tail queue declarations.
 */
#define	ATAILQ_HEAD(name, type)						\
struct name {								\
	struct type *tqh_first;	/* first element */			\
	struct type **tqh_last;	/* addr of last next element */		\
	TRACEBUF							\
}

#define	ATAILQ_HEAD_INITIALIZER(head)					\
	{ NULL, &(head).tqh_first, TRACEBUF_INITIALIZER }

#define	ATAILQ_ENTRY(type)						\
struct {								\
	struct type *tqe_next;	/* next element */			\
	struct type **tqe_prev;	/* address of previous next element */	\
	TRACEBUF							\
}

#define	ATAILQ_CONCAT(head1, head2, field) do {				\
	if (!ATAILQ_EMPTY(head2)) {					\
		*(head1)->tqh_last = (head2)->tqh_first;		\
		(head2)->tqh_first->field.tqe_prev = (head1)->tqh_last;	\
		(head1)->tqh_last = (head2)->tqh_last;			\
		ATAILQ_INIT((head2));					\
	}								\
} while (0)

#define	ATAILQ_EMPTY(head)	((head)->tqh_first == NULL)

#define	ATAILQ_FIRST(head)	((head)->tqh_first)

#define	ATAILQ_FOREACH(var, head, field)					\
	for ((var) = ATAILQ_FIRST((head));				\
	    (var);							\
	    (var) = ATAILQ_NEXT((var), field))

#define	ATAILQ_FOREACH_FROM(var, head, field)				\
	for ((var) = ((var) ? (var) : ATAILQ_FIRST((head)));		\
	    (var);							\
	    (var) = ATAILQ_NEXT((var), field))

#define	ATAILQ_FOREACH_SAFE(var, head, field, tvar)			\
	for ((var) = ATAILQ_FIRST((head));				\
	    (var) && ((tvar) = ATAILQ_NEXT((var), field), 1);		\
	    (var) = (tvar))

#define	ATAILQ_FOREACH_FROM_SAFE(var, head, field, tvar)			\
	for ((var) = ((var) ? (var) : ATAILQ_FIRST((head)));		\
	    (var) && ((tvar) = ATAILQ_NEXT((var), field), 1);		\
	    (var) = (tvar))

#define	ATAILQ_FOREACH_REVERSE(var, head, headname, field)		\
	for ((var) = ATAILQ_LAST((head), headname);			\
	    (var);							\
	    (var) = ATAILQ_PREV((var), headname, field))

#define	ATAILQ_FOREACH_REVERSE_FROM(var, head, headname, field)		\
	for ((var) = ((var) ? (var) : ATAILQ_LAST((head), headname));	\
	    (var);							\
	    (var) = ATAILQ_PREV((var), headname, field))

#define	ATAILQ_FOREACH_REVERSE_SAFE(var, head, headname, field, tvar)	\
	for ((var) = ATAILQ_LAST((head), headname);			\
	    (var) && ((tvar) = ATAILQ_PREV((var), headname, field), 1);	\
	    (var) = (tvar))

#define	ATAILQ_FOREACH_REVERSE_FROM_SAFE(var, head, headname, field, tvar) \
	for ((var) = ((var) ? (var) : ATAILQ_LAST((head), headname));	\
	    (var) && ((tvar) = ATAILQ_PREV((var), headname, field), 1);	\
	    (var) = (tvar))

#define	ATAILQ_INIT(head) do {						\
	ATAILQ_FIRST((head)) = NULL;					\
	(head)->tqh_last = &ATAILQ_FIRST((head));			\
} while (0)

#define	ATAILQ_INSERT_AFTER(head, listelm, elm, field) do {		\
	if ((ATAILQ_NEXT((elm), field) = ATAILQ_NEXT((listelm), field)) != NULL)\
		ATAILQ_NEXT((elm), field)->field.tqe_prev = 		\
		    &ATAILQ_NEXT((elm), field);				\
	else {								\
		(head)->tqh_last = &ATAILQ_NEXT((elm), field);		\
	}								\
	ATAILQ_NEXT((listelm), field) = (elm);				\
	(elm)->field.tqe_prev = &ATAILQ_NEXT((listelm), field);		\
} while (0)

#define	ATAILQ_INSERT_BEFORE(listelm, elm, field) do {			\
	(elm)->field.tqe_prev = (listelm)->field.tqe_prev;		\
	ATAILQ_NEXT((elm), field) = (listelm);				\
	*(listelm)->field.tqe_prev = (elm);				\
	(listelm)->field.tqe_prev = &ATAILQ_NEXT((elm), field);		\
} while (0)

#define	ATAILQ_INSERT_HEAD(head, elm, field) do {			\
	if ((ATAILQ_NEXT((elm), field) = ATAILQ_FIRST((head))) != NULL)	\
		ATAILQ_FIRST((head))->field.tqe_prev =			\
		    &ATAILQ_NEXT((elm), field);				\
	else								\
		(head)->tqh_last = &ATAILQ_NEXT((elm), field);		\
	ATAILQ_FIRST((head)) = (elm);					\
	(elm)->field.tqe_prev = &ATAILQ_FIRST((head));			\
} while (0)

#define	ATAILQ_INSERT_TAIL(head, elm, field) do {			\
	ATAILQ_NEXT((elm), field) = NULL;				\
	(elm)->field.tqe_prev = (head)->tqh_last;			\
	*(head)->tqh_last = (elm);					\
	(head)->tqh_last = &ATAILQ_NEXT((elm), field);			\
} while (0)

#define	ATAILQ_LAST(head, headname)					\
	(*(((struct headname *)((head)->tqh_last))->tqh_last))

#define	ATAILQ_NEXT(elm, field) ((elm)->field.tqe_next)

#define	ATAILQ_PREV(elm, headname, field)				\
	(*(((struct headname *)((elm)->field.tqe_prev))->tqh_last))

#define	ATAILQ_REMOVE(head, elm, field) do {				\
	if ((ATAILQ_NEXT((elm), field)) != NULL)				\
		ATAILQ_NEXT((elm), field)->field.tqe_prev = 		\
		    (elm)->field.tqe_prev;				\
	else {								\
		(head)->tqh_last = (elm)->field.tqe_prev;		\
	}								\
	*(elm)->field.tqe_prev = ATAILQ_NEXT((elm), field);		\
	TRASHIT(*oldnext);						\
	TRASHIT(*oldprev);						\
} while (0)

#define ATAILQ_SWAP(head1, head2, type, field) do {			\
	struct type *swap_first = (head1)->tqh_first;			\
	struct type **swap_last = (head1)->tqh_last;			\
	(head1)->tqh_first = (head2)->tqh_first;			\
	(head1)->tqh_last = (head2)->tqh_last;				\
	(head2)->tqh_first = swap_first;				\
	(head2)->tqh_last = swap_last;					\
	if ((swap_first = (head1)->tqh_first) != NULL)			\
		swap_first->field.tqe_prev = &(head1)->tqh_first;	\
	else								\
		(head1)->tqh_last = &(head1)->tqh_first;		\
	if ((swap_first = (head2)->tqh_first) != NULL)			\
		swap_first->field.tqe_prev = &(head2)->tqh_first;	\
	else								\
		(head2)->tqh_last = &(head2)->tqh_first;		\
} while (0)

typedef uint64_t vat_offset_t;	/* Offsets within the database */

static inline int
test_cas (vat_offset_t *dest, vat_offset_t oldval, vat_offset_t newval)
{
    return __sync_bool_compare_and_swap(dest, oldval, newval);
}

int
main (int argc, char **argv)
{
    int i;
    vat_offset_t t;

    t = 0;

    for (i = 0; i < 100; i++) {
	for (;;) {
	    vat_offset_t o;
	    o = t;

	    vat_offset_t n;
	    n = i;

	    if (test_cas(&t, o, n))
		break;
	}
    }

    return 0;
}
