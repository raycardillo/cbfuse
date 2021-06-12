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

#ifndef CBFUSE_BLOCKS_HEADER_SEEN
#define CBFUSE_BLOCKS_HEADER_SEEN

#include <libcouchbase/couchbase.h>

int read_data(lcb_INSTANCE *instance, const char *pkey, const char *buf, size_t nbuf, off_t offset);
int write_data(lcb_INSTANCE *instance, const char *pkey, const char *buf, size_t nbuf, off_t offset);

#endif /* !CBFUSE_BLOCKS_HEADER_SEEN */
