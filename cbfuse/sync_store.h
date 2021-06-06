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

#ifndef CBFUSE_SYNC_STORE_HEADER_SEEN
#define CBFUSE_SYNC_STORE_HEADER_SEEN

#include <libcouchbase/couchbase.h>

typedef struct
sync_store_result {
    lcb_STATUS status;
    const char *key, *value;
    size_t nkey, nvalue;
    uint64_t cas;
    uint32_t flags;
} sync_store_result;          // contains the results of the operation

/**
 * Initializes the synchronous helper by installing the required callback.
 *
 * @param instance  the library instance to use
 */
void sync_store_init(lcb_INSTANCE *instance);

/**
 * Perform a synchronous store operation and return the result.
 * 
 * For convenience, the command will be destraoyed after it is used.
 * 
 * @param instance  library instance to use
 * @param cmd       specific store command to call
 * @param result    results from the store operation 
 * @return status code of the synchronous operation
 */
lcb_STATUS sync_store(lcb_INSTANCE *instance, lcb_CMDSTORE *cmd, sync_store_result **result);

/**
 * Frees the memory that was used to provide results.
 *
 * @param result    result memory to destroy
 */
void sync_store_destroy(sync_store_result *result);

#endif /* !CBFUSE_SYNC_STORE_HEADER_SEEN */
