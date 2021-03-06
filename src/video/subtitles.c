/*
 *  Subtitling
 *  Copyright (C) 2007, 2010 Andreas Öman
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "showtime.h"
#include "subtitles.h"
#include "fileaccess/fileaccess.h"
#include "misc/gz.h"

#include "libavcodec/avcodec.h"
#include "media.h"


/**
 *
 */
static int
se_cmp(const subtitle_entry_t *a, const subtitle_entry_t *b)
{
  if(a->se_start > b->se_start)
    return 1;
  if(a->se_start < b->se_start)
    return -1;
  return 0;
}

typedef struct {
  const char *buf;
  size_t len;       // Remaining bytes in buf
  ssize_t ll;        // Length of current line
} linereader_t;


static void
linereader_init(linereader_t *lr, const char *buf, size_t len)
{
  lr->buf = buf;
  lr->len = len;
  lr->ll = -1;
}



/**
 * Find end of line
 */
static ssize_t
linereader_next(linereader_t *lr)
{
  ssize_t i;

  if(lr->ll != -1) {
    // Skip over previous line
    lr->buf += lr->ll;
    lr->len -= lr->ll;

    /* Skip over EOL */
    if(lr->len > 0 && lr->buf[0] == 13) {
      lr->len--;
      lr->buf++;
    }

    if(lr->len > 0 && lr->buf[0] == 10) {
      lr->len--;
      lr->buf++; 
    }

  }

  if(lr->len == 0) {
    /* At EOF */
    lr->ll = -1;
    return -1;
  }

  for(i = 0; i < lr->len; i++)
    if(lr->buf[i] == 10 || lr->buf[i] == 13)
      break;

  lr->ll = i;
  return i;
}


/**
 *
 */
static int
get_int(linereader_t *lr, int *vp)
{
  int i, r = 0;
  for(i = 0; i < lr->ll; i++) {
    if(lr->buf[i] < '0' || lr->buf[i] > '9')
      return -1;
    r = r * 10 + lr->buf[i] - '0';
  }
  *vp = r;
  return 0;
}


/**
 *
 */
static int64_t
get_srt_timestamp2(const char *buf)
{
  return 1000LL * (
    (buf[ 0] - '0') * 36000000LL + 
    (buf[ 1] - '0') *  3600000LL +
    (buf[ 3] - '0') *   600000LL +
    (buf[ 4] - '0') *    60000LL + 
    (buf[ 6] - '0') *    10000LL + 
    (buf[ 7] - '0') *     1000LL +
    (buf[ 9] - '0') *      100LL +
    (buf[10] - '0') *       10LL +
    (buf[11] - '0'));
}

/**
 *
 */
static int
get_srt_timestamp(linereader_t *lr, int64_t *start, int64_t *stop)
{
  if(lr->ll != 29 || memcmp(lr->buf + 12, " --> ", 5))
    return -1;

  *start = get_srt_timestamp2(lr->buf);
  *stop  = get_srt_timestamp2(lr->buf + 17);
  return 0;
}


/**
 *
 */
static int
is_srt(const char *buf, size_t len)
{
  linereader_t lr;

  int n;
  int64_t start, stop;

  linereader_init(&lr, buf, len);

  if(linereader_next(&lr) < 0)
    return 0;
  if(get_int(&lr, &n))
    return 0;
  if(linereader_next(&lr) < 0)
    return 0;
  if(get_srt_timestamp(&lr, &start, &stop))
    return 0;

  if(stop < start)
    return 0;

  return 1;

}


/**
 *
 */
static subtitles_t *
load_srt(const char *buf, size_t len)
{
  int n;
  size_t tlen;
  int64_t start, stop;
  linereader_t lr;
  subtitles_t *s = calloc(1, sizeof(subtitles_t));
  char *txt;

  RB_INIT(&s->s_entries);
  linereader_init(&lr, buf, len);
  
  while(1) {
    if(linereader_next(&lr) < 0)
      break;
    if(get_int(&lr, &n) < 0)
      break;
    if(linereader_next(&lr) < 0)
      break;
    if(get_srt_timestamp(&lr, &start, &stop) < 0)
      break;

    tlen = 0;
    txt = NULL;
    // Text lines
    while(lr.ll != -1) {
      if(linereader_next(&lr) < 1)
	break;

      txt = realloc(txt, tlen + lr.ll + 1);
      memcpy(txt + tlen, lr.buf, lr.ll);
      txt[tlen + lr.ll] = 10;

      tlen += lr.ll + 1;
    }

    if(txt != NULL) {
      subtitle_entry_t *se = malloc(sizeof(subtitle_entry_t));
      se->se_start = start;
      se->se_stop = stop;
      se->se_text = txt;
      txt[tlen - 1] = 0;

      if(RB_INSERT_SORTED(&s->s_entries, se, se_link, se_cmp)) {
	// Collision
	free(se);
	free(txt);
      }
    }
    if(lr.ll < 0)
      break;
  }
  return s;
}

#if 0
/**
 *
 */
static void
dump_subtitles(subtitles_t *s)
{
  subtitle_entry_t *se;

  RB_FOREACH(se, &s->s_entries, se_link) {
    printf("PAGE: %lld -> %lld\n--\n%s\n--\n", se->se_start, se->se_stop, se->se_text);
  }
}
#endif

/**
 *
 */
subtitles_t *
subtitles_create(const char *buf, size_t len)
{
  subtitles_t *s;
  if(is_srt(buf, len)) 
    s = load_srt(buf, len);
  else
    s = NULL;

  return s;
}


/**
 *
 */
static void
subtitle_entry_destroy(subtitle_entry_t *se)
{
  if(se->se_link.left != NULL)
    subtitle_entry_destroy(se->se_link.left);
  if(se->se_link.right != NULL)
    subtitle_entry_destroy(se->se_link.right);
  free(se->se_text);
  free(se);
}



/**
 *
 */
void
subtitles_destroy(subtitles_t *s)
{
  if(s->s_entries.root != NULL)
    subtitle_entry_destroy(s->s_entries.root);

  free(s);
}


/**
 *
 */
subtitle_entry_t *
subtitles_pick(subtitles_t *s, int64_t pts)
{
  subtitle_entry_t skel, *se = s->s_cur;

  if(se != NULL && se->se_start <= pts && se->se_stop > pts)
    return NULL; // Already sent
  
  if(se != NULL) {
    se = RB_NEXT(se, se_link);
    if(se != NULL && se->se_start <= pts && se->se_stop > pts)
      return s->s_cur = se;
  }
  
  skel.se_start = pts;
  se = RB_FIND_LE(&s->s_entries, &skel, se_link, se_cmp);
  if(se == NULL || se->se_stop <= pts) {
    s->s_cur = NULL;
    return NULL;
  }

  return s->s_cur = se;
}


/**
 *
 */
subtitles_t *
subtitles_load(const char *url)
{
  subtitles_t *sub;
  char errbuf[256];
  struct fa_stat fs;
  int datalen;
  char *data = fa_quickload(url, &fs, NULL, errbuf, sizeof(errbuf));

  if(data == NULL) {
    TRACE(TRACE_ERROR, "Subtitles", "Unable to load %s -- %s", 
	  url, errbuf);
    return NULL;
  }

  if(gz_check(data, fs.fs_size)) {
    // is .gz compressed, inflate it

    char *inflated;
    size_t inflatedlen;

    inflated = gz_inflate(data, fs.fs_size,
			  &inflatedlen, errbuf, sizeof(errbuf));

    free(data);
    if(inflated == NULL) {
      TRACE(TRACE_ERROR, "Subtitles", "Unable to decompress %s -- %s", 
	    url, errbuf);
      return NULL;
    }
    data = inflated;
    datalen = inflatedlen;
  } else {
    datalen = fs.fs_size;
  }

  sub = subtitles_create(data, datalen);

  free(data);

  if(sub == NULL)
    TRACE(TRACE_ERROR, "Subtitles", "Unable to load %s -- Unknown format", 
	  url);
  return sub;
}


/**
 *
 */
static int64_t
get_ssa_ts(const char *buf)
{
  if(strlen(buf) != 10)
    return AV_NOPTS_VALUE;

  return 1000LL * (
    (buf[ 0] - '0') *  3600000LL +
    (buf[ 2] - '0') *   600000LL +
    (buf[ 3] - '0') *    60000LL + 
    (buf[ 5] - '0') *    10000LL + 
    (buf[ 6] - '0') *     1000LL +
    (buf[ 8] - '0') *      100LL +
    (buf[ 9] - '0') *       10LL);

}


/**
 *
 */
media_buf_t *
subtitles_ssa_decode_line(uint8_t *src, size_t len)
{
  char *t[10], *s, *d;
  char *buf = alloca(len + 1);
  int i;
  int64_t start, end, duration;
  media_buf_t *mb;

  memcpy(buf, src, len);
  buf[len] = 0;

  if(strncmp(buf, "Dialogue:", strlen("Dialogue:")))
    return NULL;
  buf += strlen("Dialogue:");

  s = strchr(buf, '\n');
  if(s != NULL)
    *s = 0;
  
  t[0] = buf;
  for(i = 1; i < 10; i++) {
    s = strchr(t[i-1], ',');
    if(s == NULL)
      return NULL;
    *s++ = 0;
    t[i] = s;
  }

  start = get_ssa_ts(t[1]);
    end = get_ssa_ts(t[2]);
  
  if(start == AV_NOPTS_VALUE || end == AV_NOPTS_VALUE)
    return NULL;

  duration = end - start;

  d = s = t[9];
  while(*s) {
    if(s[0] == '\\' && (s[1] == 'N' || s[1] == 'n')) {
      *d++ = '\n';
      s += 2;
    } else {
      *d++ = *s++;
    }
  }

  mb = media_buf_alloc();
  mb->mb_data_type = MB_SUBTITLE;
    
  mb->mb_pts = start;
  mb->mb_duration = duration;
  mb->mb_data = strdup(t[9]);
  mb->mb_size = 0;
  return mb;
}


