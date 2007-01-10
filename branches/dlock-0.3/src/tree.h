/*
    tree.h - lock tree utility functions
    Copyright (C) 2006,2007  Frederik Deweerdt <frederik.deweerdt@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef _TREE_H_
#define _TREE_H_

#include "dlock_core.h"

struct dlock_node {
	struct dlock_node *parent;
	struct dlock_node **children;
	dlock_lock_t *lock;
	int nb_children;
	unsigned long long lock_time;
	unsigned long long unlock_time;
};

#define CUR_CHILD(t) (t->current_node->children[t->current_node->nb_children - 1])
#define FIRST_NODE(t) (t->root->children[0])

static int get_tree_depth(struct dlock_node *n, int current_depth)
{
	int i, max;
	max = current_depth;

	for (i=0; i < n->nb_children; i++) {
		int d = get_tree_depth(n->children[i], current_depth + 1);
		if (d > max)
			max = d;
	}
	return max;
}

static void free_nodes(struct dlock_node *n)
{
	int i;

	for (i=0; i < n->nb_children; i++) {
		free_nodes(n->children[i]);
	}
	free(n->children);
	free(n);
}

#define BIG_PRIME (4294967291u)
/* Returns a checksum used to uniquely identify a mutex/action sequence
   this helps distinguishing between a A->B->B->A and a B->A->B->A
   sequence */
static unsigned long cksum(struct dlock_node *n, int salt)
{
	int i;
	unsigned long cksm = 0;

	for (i=0; i < n->nb_children; i++) {
		cksm = (cksm << 4)^(cksm >> 28)^((unsigned long)n->children[i]->lock);
		cksm = (cksm << 4)^(cksm >> 28)^(cksum(n->children[i], cksm));
	}
	return cksm % BIG_PRIME;
}


#endif /* _TREE_H_ */

