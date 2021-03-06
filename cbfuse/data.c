/*
 * cbfuse implements a FUSE file-system using Couchbase as the data store.
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
#include <stdbool.h>

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
    const bool isNotTruncate = (buf != NULL && nbuf > 0); 

    // calculate the overall length of the update operation
    size_t nupdate = offset + nbuf;
    IfTrueGotoDoneWithRef((nupdate > MAX_FILE_LEN), -EFBIG, pkey);

    // get the current data for the block
    fresult = get_block(instance, pkey, 1, &get_result);
    if (fresult == -ENOENT) {
        // if a block wasn't found:
        // a) nothing to do if it's a truncate operation (no data provided)
        // b) otherwise allocate memory to store the new data

        fresult = 0;
        if (isNotTruncate) {
            get_result->value = malloc(nupdate);
            IfNULLGotoDoneWithRef(get_result->value, -ENOMEM, pkey);
            get_result->nvalue = nupdate;
            *new_block_size = nupdate;
        } else {
            goto done;
        }

    } else {
        IfFRErrorGotoDoneWithRef(pkey);
    }

    if (isNotTruncate) {
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
    } else {
        // no need to allocate and copy - just truncate the size of the data
        get_result->nvalue = offset;
    }

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
    if (store_result->status == LCB_SUCCESS) {
        fresult = 0;
    } else if (store_result->status == LCB_ERR_DOCUMENT_NOT_FOUND) {
        fresult = -ENOENT;
    } else {
        fresult = -EIO;
    }

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
    IfFRErrorGotoDoneWithRef(pkey);

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
    IfFRErrorGotoDoneWithRef(pkey);

    // Update the read result to indicate how many bytes were read
    fresult = nbuf;

done:
    if (fresult < 0) {
        // read must return 0 on EOF or -1 when an error happens
        fresult = -1;
    }
    sync_get_destroy(get_result);
    return fresult;
}

int write_data(lcb_INSTANCE *instance, const char *pkey, const char *buf, size_t nbuf, off_t offset)
{
    int fresult = 0;

    // TODO: Refactor to support multiple data blocks

    // TODO: Improve write performance
    // Current strategy is pretty bad because it's fetching the current block and then making changes and rewriting.
    // Even with FUSE_CAP_BIG_WRITES it ends up being too chatty because we're limited to the kernel read/write
    // buffer size (e.g., 64k on macOS).

    IfTrueGotoDoneWithRef((offset + nbuf > MAX_FILE_LEN), -EFBIG, pkey);

    size_t new_block_size = 0;
    fresult = update_block(instance, pkey, 1, buf, nbuf, offset, &new_block_size);
    IfFRErrorGotoDoneWithRef(pkey);

    if (new_block_size != 0) {
        fresult = update_stat_size(instance, pkey, new_block_size);
        IfFRErrorGotoDoneWithRef(pkey);
    }

    // Update the write result to indicate how many bytes were written
    fresult = nbuf;

done:
    if (fresult < 0) {
        // write must return -1 when an error happens
        fresult = -1;
    }
    return fresult;
}

int remove_data(lcb_INSTANCE *instance, const char *pkey)
{
    int fresult = 0;
    sync_remove_result *result = NULL;

    // TODO: Refactor to support multiple data blocks

    lcb_STATUS rc;
    lcb_CMDREMOVE *cmd;

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

int truncate_data(lcb_INSTANCE *instance, const char *pkey, off_t offset)
{
    int fresult = 0;

    // NOTE:
    // Strategy here is just to truncate existing data if smaller.
    // If larger, we just want to verify upper limit is available
    // because it doesn't make sense to re-write the blocks here.
    // New blocks will just be allocated on future writes, and the
    // writes are configured to support FUSE_CAP_BIG_WRITES writes.

    // TODO: Refactor to support multiple data blocks

    if (offset == 0) {
        // truncating to zero is equivalent to removing all data for the file
        fresult = remove_data(instance, pkey);
    } else {
        // otherwise update the block with no data (indicating a truncate)
        fresult = update_block(instance, pkey, 1, NULL, 0, offset, NULL);
    }
    IfFRErrorGotoDoneWithRef(pkey);

    // now update the file size
    fresult = update_stat_size(instance, pkey, offset);
    IfFRErrorGotoDoneWithRef(pkey);

done:
    //fprintf(stderr, ">> truncate_data done: pkey:%s fr:%d\n", pkey, fresult);
    return fresult;
}