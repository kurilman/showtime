/*
 *  GL Widgets, GLW_CONTAINER -widgets
 *  Copyright (C) 2007 Andreas �man
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

#include "glw.h"
#include "glw_container.h"

static void 
glw_container_xy_layout(glw_t *w, glw_rctx_t *rc)
{
  glw_t *c;
  float d = -1.0f, e1, tw = 0;
  int xy;
  glw_rctx_t rc0;

  glw_flush_render_list(w);

  if(w->glw_alpha < 0.01)
    return;

  rc0 = *rc;

  TAILQ_FOREACH(c, &w->glw_childs, glw_parent_link)
    tw += c->glw_conf_weight;

  switch(w->glw_class) {
  case GLW_CONTAINER_X:
    xy = 1;
    break;
  case GLW_CONTAINER_Y:
    xy = 0;
    break;
  default:
    return;
  }


  TAILQ_FOREACH(c, &w->glw_childs, glw_parent_link) {

    e1 = c->glw_conf_weight / tw;
    c->glw_norm_weight = e1;

    if(c->glw_class != GLW_DUMMY) {

      if(xy) {
	c->glw_parent_pos.x = d + e1 + w->glw_displacement.x;
	c->glw_parent_pos.y = 0.0    + w->glw_displacement.y;
	c->glw_parent_pos.z = 0.0    + w->glw_displacement.z;

	c->glw_parent_scale.x = e1;
	c->glw_parent_scale.y = 1.0f;
	c->glw_parent_scale.z = e1;
      
	rc0.rc_size_x = rc->rc_size_x * e1;

      } else {

	c->glw_parent_pos.x = 0         + w->glw_displacement.x;
	c->glw_parent_pos.y = -(d + e1) + w->glw_displacement.y;
	c->glw_parent_pos.z = 0.0       + w->glw_displacement.z;

	c->glw_parent_scale.x = 1.0f;
	c->glw_parent_scale.y = e1;
	c->glw_parent_scale.z = e1;

	rc0.rc_size_y = rc->rc_size_y * e1;
      }

      glw_layout0(c, &rc0);
 
      if(c->glw_conf_weight > 0.01)
	glw_link_render_list(w, c);
    }
    d += 2 * e1;
  }
}



static void
glw_container_z_layout(glw_t *w, glw_rctx_t *rc)
{
  glw_t *c;
  glw_rctx_t rc0;

  if(w->glw_alpha < 0.01)
    return;

  rc0 = *rc;

  glw_flush_render_list(w);

  TAILQ_FOREACH(c, &w->glw_childs, glw_parent_link) {
    
    c->glw_parent_pos.x = w->glw_displacement.x;
    c->glw_parent_pos.y = w->glw_displacement.y;
    c->glw_parent_pos.z = w->glw_displacement.z;

    c->glw_parent_scale.x = 1.0f;
    c->glw_parent_scale.y = 1.0f;
    c->glw_parent_scale.z = 1.0f;

    glw_layout0(c, &rc0);

    glw_link_render_list(w, c);
   }
}




static void
glw_container_render(glw_t *w, glw_rctx_t *rc)
{
  glw_t *c;
  float alpha = rc->rc_alpha * w->glw_alpha;
  glw_rctx_t rc0 = *rc;

  if(alpha < 0.01)
    return;

  rc0.rc_alpha = alpha;
  
  if(glw_is_focusable(w))
    glw_store_matrix(w, rc);

  TAILQ_FOREACH(c, &w->glw_render_list, glw_render_link)
    glw_render_TS(c, &rc0, rc);
}


static int
glw_container_callback(glw_t *w, void *opaque, glw_signal_t signal,
		       void *extra)
{
  glw_t *c;

  switch(signal) {
  case GLW_SIGNAL_RENDER:
    glw_container_render(w, extra);
    break;

  case GLW_SIGNAL_EVENT:
    TAILQ_FOREACH(c, &w->glw_childs, glw_parent_link)
      if(glw_signal0(c, GLW_SIGNAL_EVENT, extra))
	return 1;
    break;

  default:
    break;
  }
  return 0;
}

static int
glw_container_xy_callback(glw_t *w, void *opaque, glw_signal_t signal,
			  void *extra)
{
  switch(signal) {
  default:
    return glw_container_callback(w, opaque, signal, extra);
  case GLW_SIGNAL_LAYOUT:
    glw_container_xy_layout(w, extra);
    break;
  }
  return 0;
}

static int
glw_container_z_callback(glw_t *w, void *opaque, glw_signal_t signal,
			 void *extra)
{
  switch(signal) {
  default:
    return glw_container_callback(w, opaque, signal, extra);
  case GLW_SIGNAL_LAYOUT:
    glw_container_z_layout(w, extra);
    break;
  }
  return 0;
}




void 
glw_container_ctor(glw_t *w, int init, va_list ap)
{
  if(init) {
    switch(w->glw_class) {
    default:
      glw_signal_handler_int(w, glw_container_xy_callback);
      break;
      
    case GLW_CONTAINER_Z:
      glw_signal_handler_int(w, glw_container_z_callback);
      break;
    }
  }
}