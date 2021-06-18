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

int add_new_dentry(lcb_INSTANCE *instance, const char *dir_pkey, const char *dir_path, const char *parent_path);
int get_dentry_json(lcb_INSTANCE *instance, const char *pkey, cJSON **dentry_json);
int add_child_to_dentry(lcb_INSTANCE *instance, const char *dir_pkey, const char *child_name);
int remove_child_from_dentry(lcb_INSTANCE *instance, const char *dir_pkey, const char *child_name);

#endif /* !CBFUSE_DENTRIES_HEADER_SEEN */
