/*
 *  Property trees
 *  Copyright (C) 2008 Andreas Öman
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PROP_H__
#define PROP_H__

#include <stdlib.h>

#include <libavutil/pixfmt.h>
#include <arch/threads.h>
#include <queue.h>
#include <htsmsg/htsmsg.h>

struct prop;
struct prop_sub;

#define PROP_ADD_SELECTED 0x1

typedef enum {
  PROP_SET_VOID,
  PROP_SET_STRING,
  PROP_SET_INT,
  PROP_SET_FLOAT,
  PROP_SET_DIR,
  PROP_SET_PIXMAP,

  PROP_ADD_CHILD,
  PROP_ADD_CHILD_BEFORE,
  PROP_DEL_CHILD,
  PROP_SELECT_CHILD,
  PROP_REQ_NEW_CHILD,
  PROP_REQ_DELETE,
  PROP_DESTROYED,
} prop_event_t;

typedef void (prop_callback_t)(struct prop_sub *sub, prop_event_t event, ...);


TAILQ_HEAD(prop_queue, prop);
LIST_HEAD(prop_list, prop);
LIST_HEAD(prop_sub_list, prop_sub);



/**
 *
 */
TAILQ_HEAD(prop_notify_queue, prop_notify);

typedef struct prop_courier {

  struct prop_notify_queue pc_queue;

  hts_mutex_t *pc_entry_mutex;
  hts_cond_t pc_cond;
  hts_thread_t pc_thread;

  int pc_run;

} prop_courier_t;



/**
 * Property types
 */
typedef enum {
  PROP_VOID,
  PROP_DIR,
  PROP_STRING,
  PROP_FLOAT,
  PROP_INT,
  PROP_PIXMAP,
  PROP_ZOMBIE, /* Destroyed can never be changed again */
} prop_type_t;


/**
 *
 */
typedef struct prop_pixmap {
  int pp_refcount;

  int pp_width;
  int pp_height;
  int pp_linesize;

  enum PixelFormat pp_pixfmt;

  uint8_t pp_pixels[0];
} prop_pixmap_t;


/**
 *
 */
typedef struct prop {

  /**
   * Refcount. Not protected by mutex. Modification needs to be issued
   * using atomic ops.
   */
  int hp_refcount;

  /**
   * The name never changes after creation. Not protected by mutex
   */
  char *hp_name;

  /**
   * Parent linkage. Protected by mutex
   */
  struct prop *hp_parent;
  TAILQ_ENTRY(prop) hp_parent_link;


  /**
   * Originating property. Used when reflecting properties
   * in the tree (aka symlinks). Protected by mutex
   */
  struct prop *hp_originator;
  LIST_ENTRY(prop) hp_originator_link;

  /**
   * Properties receiving our values. Protected by mutex
   */
  struct prop_list hp_targets;


  /**
   * Subscriptions. Protected by mutex
   */
  struct prop_sub_list hp_value_subscriptions;
  struct prop_sub_list hp_canonical_subscriptions;

  /**
   * Payload
   * Protected by mutex
   */
  prop_type_t hp_type;
  union {
    float f;
    int i;
    char *str;
    struct {
      struct prop_queue childs;
      struct prop *selected;
    } c;
    prop_pixmap_t *pixmap;
  } u;

#define hp_string   u.str
#define hp_float    u.f
#define hp_int      u.i
#define hp_childs   u.c.childs
#define hp_selected u.c.selected
#define hp_pixmap   u.pixmap

} prop_t;



/**
 *
 */
typedef struct prop_sub {

  /**
   * Refcount. Not protected by mutex. Modification needs to be issued
   * using atomic ops.
   */
  int hps_refcount;

  /**
   * Callback. May never be changed. Not protected by mutex
   */
  prop_callback_t *hps_callback;

  /**
   * Flags as passed to prop_subscribe()
   */
  int hps_flags;

  /**
   * Pointer to courier, May never be changed. Not protected by mutex
   */
  prop_courier_t *hps_courier;

  /**
   *
   */
  void *hps_opaque;

  /**
   * Linkage to property. Protected by mutex
   */
  LIST_ENTRY(prop_sub) hps_value_prop_link;
  prop_t *hps_value_prop;

  /**
   * Linkage to property. Protected by mutex
   */
  LIST_ENTRY(prop_sub) hps_canonical_prop_link;
  prop_t *hps_canonical_prop;

} prop_sub_t;


/**
 *
 */

prop_t *prop_get_global(void);

void prop_init(void);

prop_sub_t *prop_subscribe(const char **name,
			   prop_callback_t *cb, void *opaque,
			   prop_courier_t *pc, int flags,
			   prop_t *p1, prop_t *p2)
     __attribute__ ((malloc));

#define PROP_SUB_DIRECT_UPDATE 0x1
#define PROP_SUB_NO_INITIAL_UPDATE 0x2
#define PROP_SUB_TRACK_DESTROY 0x4
#define PROP_SUB_AUTO_UNSUBSCRIBE 0x8

void prop_unsubscribe(prop_sub_t *s);

prop_t *prop_create_ex(prop_t *parent, const char *name,
		       prop_sub_t *skipme)
     __attribute__ ((malloc));

#define prop_create(parent, name) prop_create_ex(parent, name, NULL)

void prop_destroy(prop_t *p);

void prop_set_string_ex(prop_t *p, prop_sub_t *skipme, const char *str);

void prop_set_stringf_ex(prop_t *p, prop_sub_t *skipme, const char *fmt, ...);

void prop_set_float_ex(prop_t *p, prop_sub_t *skipme, float v);

void prop_set_int_ex(prop_t *p, prop_sub_t *skipme, int v);

void prop_set_void_ex(prop_t *p, prop_sub_t *skipme);

void prop_set_pixmap_ex(prop_t *p, prop_sub_t *skipme, prop_pixmap_t *pp);

#define prop_set_string(p, str) prop_set_string_ex(p, NULL, str)

#define prop_set_stringf(p, fmt...) prop_set_stringf_ex(p, NULL, fmt)

#define prop_set_float(p, v) prop_set_float_ex(p, NULL, v)

#define prop_set_int(p, v) prop_set_int_ex(p, NULL, v)

#define prop_set_void(p) prop_set_void_ex(p, NULL)

#define prop_set_pixmap(p, pp) prop_set_pixmap_ex(p, NULL, pp)

int prop_get_string(prop_t *p, char *buf, size_t bufsize)
     __attribute__ ((warn_unused_result));

void prop_ref_dec(prop_t *p);

void prop_ref_inc(prop_t *p);

void prop_pixmap_ref_dec(prop_pixmap_t *pp);

void prop_pixmap_ref_inc(prop_pixmap_t *pp);

int prop_set_parent_ex(prop_t *p, prop_t *parent, prop_t *before, 
		       prop_sub_t *skipme)
     __attribute__ ((warn_unused_result));
     
#define prop_set_parent(p, parent) prop_set_parent_ex(p, parent, NULL, NULL)

void prop_link_ex(prop_t *src, prop_t *dst, prop_sub_t *skipme);

#define prop_link(src, dst) prop_link_ex(src, dst, NULL)

void prop_unlink_ex(prop_t *p, prop_sub_t *skipme);

#define prop_unlink(p) prop_unlink_ex(p, NULL)

void prop_select_ex(prop_t *p, int advisory, prop_sub_t *skipme);

#define prop_select(p, advisory) prop_select_ex(p, advisory, NULL)

prop_t **prop_get_ancestors(prop_t *p);

void prop_ancestors_unref(prop_t **r);

prop_t *prop_get_by_subscription(prop_sub_t *s);

prop_t *prop_get_by_subscription_canonical(prop_sub_t *s);

prop_t *prop_get_by_name(const char **name, int follow_symlinks,
			 prop_t *p1, prop_t *p2);

void prop_request_new_child(prop_t *p);

void prop_request_new_child_by_subscription(prop_sub_t *s);

void prop_request_delete_child_by_subscription(prop_sub_t *s);

prop_courier_t *prop_courier_create(hts_mutex_t *entrymutex);

void prop_courier_destroy(prop_courier_t *pc);

prop_t *prop_get_by_names(prop_t *parent, ...) 
     __attribute__((__sentinel__(0)));

htsmsg_t *prop_tree_to_htsmsg(prop_t *p);

prop_pixmap_t *prop_pixmap_create(int width, int height, int linesize,
				  enum PixelFormat pixfmt, uint8_t *pixels)
     __attribute__ ((malloc));

/* DEBUGish */
const char *propname(prop_t *p);

void prop_print_tree(prop_t *p);

#endif /* PROP_H__ */