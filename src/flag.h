/* flag.h */

#ifndef _FLAG_H
#define _FLAG_H

#include "freeserf.h"
#include "list.h"
#include "map.h"

#define FLAG_INDEX(ptr)  ((int)((ptr) - globals.flgs))

#define FLAG_PLAYER(flag)  ((int)(((flag)->path_con >> 6) & 3))


typedef struct flag flag_t;

struct flag {
	map_pos_t pos; /* ADDITION */
	int search_num;
	dir_t search_dir;
	int path_con;
	int endpoint;
	int transporter;
	int length[6];
	int res_waiting[8];
	int res_dest[8];
	union {
		building_t *b[6];
		flag_t *f[6];
		void *v[6];
	} other_endpoint;
	int other_end_dir[6];
	int bld_flags;
	int stock1_prio;
	int bld2_flags;
	int stock2_prio;
};

typedef int flag_search_func(flag_t *flag, void *data);

typedef struct {
	list_t queue;
	int id;
} flag_search_t;

flag_t *get_flag(int index); /* in freeserf.c */

void flag_search_init(flag_search_t *search);
void flag_search_add_source(flag_search_t *search, flag_t *flag);
int flag_search_execute(flag_search_t *search, flag_search_func *callback, int transporter, void *data);
int flag_search_single(flag_t *src, flag_search_func *callback, int transporter, void *data);

void flag_prioritize_pickup(flag_t *flag, dir_t dir, const int flag_prio[]);
void flag_cancel_transported_stock(flag_t *flag, int res);

#endif /* ! _FLAG_H */
