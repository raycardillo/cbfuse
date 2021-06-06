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

#ifndef CBFUSE_COMMON_HEADER_SEEN
#define CBFUSE_COMMON_HEADER_SEEN

#include <stdlib.h>

extern const char   *DEFAULT_SCOPE_STRING;
extern const size_t  DEFAULT_SCOPE_STRLEN;

extern const char    ROOT_DIR_STRING[];
extern const size_t  ROOT_DIR_STRLEN;

extern const char    ATTRIBS_COLLECTION_STRING[];
extern const size_t  ATTRIBS_COLLECTION_STRLEN;

extern const char    DENTRIES_COLLECTION_STRING[];
extern const size_t  DENTRIES_COLLECTION_STRLEN;

extern const char    DENTRY_CURRENT[];
extern const char    DENTRY_PARENT[];
extern const char    DENTRY_CHILDREN[];

#endif /* !CBFUSE_COMMON_HEADER_SEEN */
