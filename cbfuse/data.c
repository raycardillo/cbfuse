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

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "stats.h"
#include "util.h"
#include "common.h"
#include "sync_get.h"
#include "sync_store.h"
#include "sync_remove.h"

static int get_block(lcb_INSTANCE *instance, const char *pkey, __unused uint8_t block, sync_get_result **result)
{
    int fresult = 0;

    lcb_STATUS rc;
    lcb_CMDGET *cmd;

    rc = lcb_cmdget_create(&cmd);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdget_collection(
        cmd,
        DEFAULT_SCOPE_STRING, DEFAULT_SCOPE_STRLEN,
        BLOCKS_COLLECTION_STRING, BLOCKS_COLLECTION_STRLEN);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdget_key(cmd, pkey, strlen(pkey));
    IfLCBFailGotoDone(rc, -EIO);

    rc = sync_get(instance, cmd, result);

    // first check the sync command result code
    IfLCBFailGotoDone(rc, -EIO);

    // check the actual result status
    IfLCBFailGotoDoneWithRef((*result)->status, -ENOENT, pkey);

    // TODO: Consider techniques to reduce extra copying (e.g., CPP ref counting)

done:
    return fresult;
}

static int update_block(lcb_INSTANCE *instance, const char *pkey, __unused uint8_t block, const char *buf, size_t nbuf, off_t offset, size_t *new_block_size)
{
    int fresult = 0;
    sync_get_result *get_result = NULL;
    sync_store_result *store_result = NULL;

    // calculate the length of the date update operation
    size_t nupdate = offset + nbuf;
    IfTrueGotoDoneWithRef((nupdate > MAX_FILE_LEN), -EFBIG, pkey);

    // get the current data for the block or allocate a new data block
    fresult = get_block(instance, pkey, 1, &get_result);
    if (fresult == -ENOENT) {
        // if the block wasn't found we need to create a new one
        fresult = 0;
        get_result->value = malloc(nupdate);
        IfNULLGotoDoneWithRef(get_result->value, -ENOMEM, pkey);
        get_result->nvalue = nupdate;
        *new_block_size = nupdate;
    } else {
        IfFRFailGotoDoneWithRef(pkey);
    }

    // if we need more room then grow the buffer
    if (nupdate > get_result->nvalue) {
        void *new_data = memdupm(get_result->value, get_result->nvalue, nupdate);
        IfNULLGotoDoneWithRef(get_result->value, -ENOMEM, pkey);
        free((void*)(get_result->value));
        get_result->value = new_data;
        get_result->nvalue = nupdate;
        *new_block_size = nupdate;
    }

    // modify the data as instructed
    memcpy((void*)((get_result->value)+offset), buf, nbuf);

    // now write the data back to Couchbase

    lcb_STATUS rc;
    lcb_CMDSTORE *cmd;

    // insert or update block data
    rc = lcb_cmdstore_create(&cmd, LCB_STORE_UPSERT);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_collection(
        cmd,
        DEFAULT_SCOPE_STRING, DEFAULT_SCOPE_STRLEN,
        BLOCKS_COLLECTION_STRING, BLOCKS_COLLECTION_STRLEN);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_datatype(cmd, LCB_VALUE_RAW);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_key(cmd, pkey, strlen(pkey));
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_value(cmd, get_result->value, get_result->nvalue);
    IfLCBFailGotoDone(rc, -EIO);

    rc = sync_store(instance, cmd, &store_result);

    // first check the sync command result code
    IfLCBFailGotoDone(rc, -EIO);

    // now check the actual result status
    IfLCBFailGotoDoneWithRef(store_result->status, -ENOENT, pkey);

    // TODO: retry if fail due to CAS (could also considering locking semantics with CAS to prevent this

done:
    sync_get_destroy(get_result);
    sync_store_destroy(store_result);
    return fresult;
}

/////

int read_data(lcb_INSTANCE *instance, const char *pkey, const char *buf, size_t nbuf, off_t offset)
{
    int fresult = 0;
    sync_get_result *get_result = NULL;

    // TODO: Refactor to support multiple data blocks

    // get the current data for the block and then deal with offset and max
    fresult = get_block(instance, pkey, 1, &get_result);
    IfFRFailGotoDoneWithRef(pkey);

    // the result size is the max read size
    size_t max_size = get_result->nvalue;

    // Check if trying to read past the max size.
    IfTrueGotoDoneWithRef((offset >= (off_t)max_size), 0, pkey);

    // Also make sure the read is trimmed to the max size.
    if ((offset + nbuf) > max_size) {
        nbuf = max_size - offset;
    }

    // Copy requested data into the buffer.
    memcpy((void*)buf, (void*)((get_result->value)+offset), nbuf);

    fresult = update_stat_atime(instance, pkey);
    IfFRFailGotoDoneWithRef(pkey);

    // Update the read result to indicate how many bytes were read
    fresult = nbuf;

done:
    sync_get_destroy(get_result);
    return fresult;
}

int write_data(lcb_INSTANCE *instance, const char *pkey, const char *buf, size_t nbuf, off_t offset)
{
    int fresult = 0;

    // TODO: Refactor to support multiple data blocks
    IfTrueGotoDoneWithRef((offset + nbuf > MAX_FILE_LEN), -EFBIG, pkey);

    size_t new_block_size = 0;
    fresult = update_block(instance, pkey, 1, buf, nbuf, offset, &new_block_size);
    IfFRFailGotoDoneWithRef(pkey);

    if (new_block_size != 0) {
        fresult = update_stat_size(instance, pkey, new_block_size);
        IfFRFailGotoDoneWithRef(pkey);
    }

    // Update the write result to indicate how many bytes were written
    fresult = nbuf;

done:
    //fprintf(stderr, ">> write_data done: pkey:%s fr:%d\n", pkey, fresult);
    return fresult;
}

int remove_data(lcb_INSTANCE *instance, const char *pkey)
{
    int fresult = 0;
    sync_remove_result *result = NULL;

    // TODO: Refactor to support multiple data blocks

    lcb_STATUS rc;
    lcb_CMDREMOVE *cmd;

    // insert only if key doesn't already exist
    rc = lcb_cmdremove_create(&cmd);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdremove_collection(
        cmd,
        DEFAULT_SCOPE_STRING, DEFAULT_SCOPE_STRLEN,
        BLOCKS_COLLECTION_STRING, BLOCKS_COLLECTION_STRLEN);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdremove_key(cmd, pkey, strlen(pkey));
    IfLCBFailGotoDone(rc, -EIO);

    rc = sync_remove(instance, cmd, &result);

    // first check the sync command result code
    IfLCBFailGotoDone(rc, -EIO);

    // now check the actual result status
    IfLCBFailGotoDoneWithRef(result->status, -ENOENT, pkey);

done:
    sync_remove_destroy(result);
    return fresult;
}