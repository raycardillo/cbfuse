/*
 * Copyright (c) 2021 Raymond Cardillo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CBFUSE_ATTRIBS_HEADER_SEEN
#define CBFUSE_ATTRIBS_HEADER_SEEN

#include <libcouchbase/couchbase.h>

// a lightweight stat object
typedef struct cbfuse_stat {
    mode_t          st_mode;        /* [XSI] Mode of file (see below) */
	time_t          st_atime;       /* [XSI] Time of last access */
	long            st_atimensec;   /* nsec of last access */
	time_t          st_mtime;       /* [XSI] Last data modification time */
	long            st_mtimensec;   /* last data modification nsec */
	time_t          st_ctime;       /* [XSI] Time of last status change */
	long            st_ctimensec;   /* nsec of last status change */
	off_t           st_size;        /* [XSI] file size, in bytes */
} cbfuse_stat;

extern const size_t CBFUSE_STAT_STRUCT_SIZE;

int insert_attrib(lcb_INSTANCE *instance, const char *pkey, mode_t mode);

#endif /* !CBFUSE_ATTRIBS_HEADER_SEEN */
