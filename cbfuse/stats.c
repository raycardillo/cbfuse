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

#define UTIME_NOW       -1
#define UTIME_OMIT      -2

const size_t CBFUSE_STAT_STRUCT_SIZE = sizeof(cbfuse_stat);

int get_stat(lcb_INSTANCE *instance, const char *pkey, cbfuse_stat *stat, uint64_t *cas)
{
    int fresult = 0;
    sync_get_result *result = NULL;

    lcb_STATUS rc;
    lcb_CMDGET *cmd;

    rc = lcb_cmdget_create(&cmd);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdget_collection(
        cmd,
        DEFAULT_SCOPE_STRING, DEFAULT_SCOPE_STRLEN,
        STATS_COLLECTION_STRING, STATS_COLLECTION_STRLEN);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdget_key(cmd, pkey, strlen(pkey));
    IfLCBFailGotoDone(rc, -EIO);

    rc = sync_get(instance, cmd, &result);

    // first check the sync command result code
    IfLCBFailGotoDone(rc, -EIO);

    // now check the actual result status
    IfLCBFailGotoDoneWithRef(result->status, -ENOENT, pkey);

    // sanity check that we received the expected structure
    IfTrueGotoDoneWithRef((result->nvalue != CBFUSE_STAT_STRUCT_SIZE), -EBADF, pkey);

    memcpy(stat, result->value, CBFUSE_STAT_STRUCT_SIZE);

    if (cas != NULL) {
        *cas = result->cas;
    }

    fprintf(stderr, "%s:%s:%d %s size:%lld\n", __FILENAME__, __func__, __LINE__, pkey, stat->st_size);

done:
    sync_get_destroy(result);
    return fresult;
}

int insert_stat(lcb_INSTANCE *instance, const char *pkey, mode_t mode)
{
    int fresult = 0;
    cbfuse_stat root_stat = {0};
    sync_store_result *result = NULL;

    // get the current time to update file times
    struct timespec ts;
    IfFalseGotoDoneWithRef(
        (clock_gettime(CLOCK_REALTIME, &ts) == 0),
        -EIO,
        "clock_gettime"
    );

    // create the stat struct to insert
    root_stat.st_mode = mode;
    root_stat.st_atime = ts.tv_sec;
    root_stat.st_atimensec = ts.tv_nsec;
    root_stat.st_mtime = ts.tv_sec;
    root_stat.st_mtimensec = ts.tv_nsec;
    root_stat.st_ctime = ts.tv_sec;
    root_stat.st_ctimensec = ts.tv_nsec;

    // now write the stat data to Couchbase

    lcb_STATUS rc;
    lcb_CMDSTORE *cmd;

    // insert only if key doesn't already exist
    rc = lcb_cmdstore_create(&cmd, LCB_STORE_INSERT);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_datatype(cmd, LCB_VALUE_RAW);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_collection(
        cmd,
        DEFAULT_SCOPE_STRING, DEFAULT_SCOPE_STRLEN,
        STATS_COLLECTION_STRING, STATS_COLLECTION_STRLEN);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_key(cmd, pkey, strlen(pkey));
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_value(cmd, (char*)&root_stat, CBFUSE_STAT_STRUCT_SIZE);
    IfLCBFailGotoDone(rc, -EIO);

    rc = sync_store(instance, cmd, &result);

    // first check the sync command result code
    IfLCBFailGotoDone(rc, -EIO);

    // now check the actual result status
    IfLCBFailGotoDoneWithRef(result->status, -ENOENT, pkey);

done:
    sync_store_destroy(result);
    return fresult;
}

int remove_stat(lcb_INSTANCE *instance, const char *pkey)
{
    int fresult = 0;
    sync_remove_result *result = NULL;

    lcb_STATUS rc;
    lcb_CMDREMOVE *cmd;

    // insert only if key doesn't already exist
    rc = lcb_cmdremove_create(&cmd);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdremove_collection(
        cmd,
        DEFAULT_SCOPE_STRING, DEFAULT_SCOPE_STRLEN,
        STATS_COLLECTION_STRING, STATS_COLLECTION_STRLEN);
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

int update_stat_atime(lcb_INSTANCE *instance, const char *pkey)
{
    int fresult = 0;
    cbfuse_stat stat = {0};
    sync_store_result *result = NULL;

    // get the current stat
    uint64_t cas = 0;
    fresult = get_stat(instance, pkey, &stat, &cas);
    IfFRErrorGotoDoneWithRef(pkey);

    // get the current time to update modified time
    struct timespec ts;
    IfFalseGotoDoneWithRef(
        (clock_gettime(CLOCK_REALTIME, &ts) == 0),
        -EIO,
        "clock_gettime"
    );

    // update the stat struct
    stat.st_atime = ts.tv_sec;
    stat.st_atimensec = ts.tv_nsec;

    // now write the stat back to Couchbase

    lcb_STATUS rc;
    lcb_CMDSTORE *cmd;

    // update the stat entry with the new version
    rc = lcb_cmdstore_create(&cmd, LCB_STORE_REPLACE);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_collection(
        cmd,
        DEFAULT_SCOPE_STRING, DEFAULT_SCOPE_STRLEN,
        STATS_COLLECTION_STRING, STATS_COLLECTION_STRLEN);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_datatype(cmd, LCB_VALUE_RAW);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_cas(cmd, cas);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_key(cmd, pkey, strlen(pkey));
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_value(cmd, (char*)&stat, CBFUSE_STAT_STRUCT_SIZE);
    IfLCBFailGotoDone(rc, -EIO);

    rc = sync_store(instance, cmd, &result);

    // first check the sync command result code
    IfLCBFailGotoDone(rc, -EIO);

    // now check the actual result status
    IfLCBFailGotoDoneWithRef(result->status, -ENOENT, pkey);

    // TODO: retry if fail due to CAS (could also considering locking semantics with CAS to prevent this

done:
    sync_store_destroy(result);
    return fresult;
}

int update_stat_utimens(lcb_INSTANCE *instance, const char *pkey, const struct timespec tv[2])
{
    int fresult = 0;
    cbfuse_stat stat = {0};
    sync_store_result *result = NULL;

    // get the current stat
    uint64_t cas = 0;
    fresult = get_stat(instance, pkey, &stat, &cas);
    IfFRErrorGotoDoneWithRef(pkey);
    
    // get the current time to update file times
    struct timespec ts_now;
    if (tv == NULL || tv[0].tv_nsec == UTIME_NOW || tv[1].tv_nsec == UTIME_NOW) {
        IfFalseGotoDoneWithRef(
            (clock_gettime(CLOCK_REALTIME, &ts_now) == 0),
            -EIO,
            "clock_gettime"
        );
    }

    // update the stat struct - the rules are a little complicated
    // for details see: UTIMENSAT(2)
    if (tv == NULL) {
        stat.st_atime = ts_now.tv_sec;
        stat.st_atimensec = ts_now.tv_nsec;
        stat.st_mtime = ts_now.tv_sec;
        stat.st_mtimensec = ts_now.tv_nsec;
    } else {
        if (tv[0].tv_nsec == UTIME_NOW) {
            stat.st_atime = ts_now.tv_sec;
            stat.st_atimensec = ts_now.tv_nsec;
        } else if (tv[0].tv_nsec != UTIME_OMIT) {
            stat.st_atime = tv[0].tv_sec;
            stat.st_atimensec = tv[0].tv_nsec;
        }
        if (tv[1].tv_nsec == UTIME_NOW) {
            stat.st_mtime = ts_now.tv_sec;
            stat.st_mtimensec = ts_now.tv_nsec;
        } else if (tv[1].tv_nsec != UTIME_OMIT) {
            stat.st_mtime = tv[1].tv_sec;
            stat.st_mtimensec = tv[1].tv_nsec;
        }
    }

    // now write the stat back to Couchbase

    lcb_STATUS rc;
    lcb_CMDSTORE *cmd;

    // update the stat entry with the new version
    rc = lcb_cmdstore_create(&cmd, LCB_STORE_REPLACE);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_collection(
        cmd,
        DEFAULT_SCOPE_STRING, DEFAULT_SCOPE_STRLEN,
        STATS_COLLECTION_STRING, STATS_COLLECTION_STRLEN);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_datatype(cmd, LCB_VALUE_RAW);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_cas(cmd, cas);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_key(cmd, pkey, strlen(pkey));
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_value(cmd, (char*)&stat, CBFUSE_STAT_STRUCT_SIZE);
    IfLCBFailGotoDone(rc, -EIO);

    rc = sync_store(instance, cmd, &result);

    // first check the sync command result code
    IfLCBFailGotoDone(rc, -EIO);

    // now check the actual result status
    IfLCBFailGotoDoneWithRef(result->status, -ENOENT, pkey);

    // TODO: retry if fail due to CAS (could also considering locking semantics with CAS to prevent this

done:
    sync_store_destroy(result);
    return fresult;
}

int update_stat_size(lcb_INSTANCE *instance, const char *pkey, size_t size)
{
    int fresult = 0;
    cbfuse_stat stat = {0};
    sync_store_result *result = NULL;

    // get the current stat
    uint64_t cas = 0;
    fresult = get_stat(instance, pkey, &stat, &cas);
    IfFRErrorGotoDoneWithRef(pkey);

    // get the current time to update modified time
    struct timespec ts;
    IfFalseGotoDoneWithRef(
        (clock_gettime(CLOCK_REALTIME, &ts) == 0),
        -EIO,
        "clock_gettime"
    );

    // update the stat struct
    stat.st_mtime = ts.tv_sec;
    stat.st_mtimensec = ts.tv_nsec;
    stat.st_size = size;

    // now write the stat back to Couchbase

    lcb_STATUS rc;
    lcb_CMDSTORE *cmd;

    // update the stat entry with the new version
    rc = lcb_cmdstore_create(&cmd, LCB_STORE_REPLACE);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_collection(
        cmd,
        DEFAULT_SCOPE_STRING, DEFAULT_SCOPE_STRLEN,
        STATS_COLLECTION_STRING, STATS_COLLECTION_STRLEN);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_datatype(cmd, LCB_VALUE_RAW);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_cas(cmd, cas);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_key(cmd, pkey, strlen(pkey));
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_value(cmd, (char*)&stat, CBFUSE_STAT_STRUCT_SIZE);
    IfLCBFailGotoDone(rc, -EIO);

    rc = sync_store(instance, cmd, &result);

    // first check the sync command result code
    IfLCBFailGotoDone(rc, -EIO);

    // now check the actual result status
    IfLCBFailGotoDoneWithRef(result->status, -ENOENT, pkey);

    // TODO: retry if fail due to CAS (could also considering locking semantics with CAS to prevent this

done:
    sync_store_destroy(result);
    return fresult;
}

int update_stat_mode(lcb_INSTANCE *instance, const char *pkey, mode_t mode)
{
    int fresult = 0;
    cbfuse_stat stat = {0};
    sync_store_result *result = NULL;

    // get the current stat
    uint64_t cas = 0;
    fresult = get_stat(instance, pkey, &stat, &cas);
    IfFRErrorGotoDoneWithRef(pkey);

    // update the stat struct
    stat.st_mode = mode;

    // now write the stat back to Couchbase

    lcb_STATUS rc;
    lcb_CMDSTORE *cmd;

    // update the stat entry with the new version
    rc = lcb_cmdstore_create(&cmd, LCB_STORE_REPLACE);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_collection(
        cmd,
        DEFAULT_SCOPE_STRING, DEFAULT_SCOPE_STRLEN,
        STATS_COLLECTION_STRING, STATS_COLLECTION_STRLEN);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_datatype(cmd, LCB_VALUE_RAW);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_cas(cmd, cas);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_key(cmd, pkey, strlen(pkey));
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_value(cmd, (char*)&stat, CBFUSE_STAT_STRUCT_SIZE);
    IfLCBFailGotoDone(rc, -EIO);

    rc = sync_store(instance, cmd, &result);

    // first check the sync command result code
    IfLCBFailGotoDone(rc, -EIO);

    // now check the actual result status
    IfLCBFailGotoDoneWithRef(result->status, -ENOENT, pkey);

    // TODO: retry if fail due to CAS (could also considering locking semantics with CAS to prevent this

done:
    sync_store_destroy(result);
    return fresult;
}
