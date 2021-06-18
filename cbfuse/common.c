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

const size_t  MAX_KEY_LEN                   = 250;
const size_t  MAX_DOC_LEN                   = 20 * 1024 * 1024;

const size_t  MAX_PATH_LEN                  = MAX_KEY_LEN;
const size_t  MAX_FILE_BLOCKS               = 256;
const size_t  MAX_FILE_LEN                  = MAX_FILE_BLOCKS * MAX_DOC_LEN;

const char   *DEFAULT_SCOPE_STRING          = NULL;
const size_t  DEFAULT_SCOPE_STRLEN          = 0;

const char    ROOT_DIR_STRING[]             = "/";
const size_t  ROOT_DIR_STRLEN               = sizeof(ROOT_DIR_STRING)-1;

const char    STATS_COLLECTION_STRING[]     = "stats";
const size_t  STATS_COLLECTION_STRLEN       = sizeof(STATS_COLLECTION_STRING)-1;

const char    DENTRIES_COLLECTION_STRING[]  = "dentries";
const size_t  DENTRIES_COLLECTION_STRLEN    = sizeof(DENTRIES_COLLECTION_STRING)-1;

const char    BLOCKS_COLLECTION_STRING[]    = "blocks";
const size_t  BLOCKS_COLLECTION_STRLEN      = sizeof(BLOCKS_COLLECTION_STRING)-1;

const char    DENTRY_DIR_PATH[]             = "d";  // current directory path key
const char    DENTRY_PAR_PATH[]             = "p";  // parent directory path key
const char    DENTRY_CHILDREN[]             = "c";  // current directory child path keys
