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

#ifndef CBFUSE_DENTRIES_HEADER_SEEN
#define CBFUSE_DENTRIES_HEADER_SEEN

#include <libcouchbase/couchbase.h>
#include <cjson/cJSON.h>

int get_dentry_json(lcb_INSTANCE *instance, const char *pkey, cJSON **dentry_json);

// directory entries are mostly used by readdir
// represented as JSON because it's mostly dynamic character data
// note that the path-key may not be the full path
// {
//   "cr": "current-dir-path-key",
//   "pr": "parent-dir-path-key",
//   "ch": [
//     "some-child-dir-path-key",
//     "other-child-dir-path-key"
//   ]
// }
char *create_dentry(const char *current_pkey, const char *parent_pkey, const char *child_pkeys[], int child_nkeys);

int add_child_to_dentry(lcb_INSTANCE *instance, const char *pkey, const char *child_pkey);

int insert_root_dentry(lcb_INSTANCE *instance);


#endif /* !CBFUSE_DENTRIES_HEADER_SEEN */
