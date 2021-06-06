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

#include "common.h"

const char   *DEFAULT_SCOPE_STRING           = NULL;
const size_t  DEFAULT_SCOPE_STRLEN           = 0;

const char    ROOT_DIR_STRING[]              = "/";
const size_t  ROOT_DIR_STRLEN                = sizeof(ROOT_DIR_STRING)-1;

const char    ATTRIBS_COLLECTION_STRING[]    = "attribs";
const size_t  ATTRIBS_COLLECTION_STRLEN      = sizeof(ATTRIBS_COLLECTION_STRING)-1;

const char    DENTRIES_COLLECTION_STRING[]   = "dentries";
const size_t  DENTRIES_COLLECTION_STRLEN     = sizeof(DENTRIES_COLLECTION_STRING)-1;

const char    DENTRY_CURRENT[]    = "cr";
const char    DENTRY_PARENT[]     = "pr";
const char    DENTRY_CHILDREN[]   = "ch";
