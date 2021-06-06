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

#include "attribs.h"
#include "util.h"
#include "common.h"
#include "sync_get.h"
#include "sync_store.h"

const size_t CBFUSE_STAT_STRUCT_SIZE = sizeof(cbfuse_stat);

int insert_attrib(lcb_INSTANCE *instance, const char *pkey, mode_t mode)
{
    int fresult = 0;
    cbfuse_stat root_stat = {0};

    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        fresult = -EIO;
        goto done;
    }

    root_stat.st_mode = mode;
    root_stat.st_atime = ts.tv_sec;
    root_stat.st_atimensec = ts.tv_nsec;
    root_stat.st_mtime = ts.tv_sec;
    root_stat.st_mtimensec = ts.tv_nsec;
    root_stat.st_ctime = ts.tv_sec;
    root_stat.st_ctimensec = ts.tv_nsec;

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
        ATTRIBS_COLLECTION_STRING, ATTRIBS_COLLECTION_STRLEN);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_key(cmd, pkey, strlen(pkey));
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_value(cmd, (char*)&root_stat, CBFUSE_STAT_STRUCT_SIZE);
    IfLCBFailGotoDone(rc, -EIO);

    sync_store_result *result = NULL;
    rc = sync_store(instance, cmd, &result);
    IfLCBFailGotoDone(rc, -EIO);

done:
    return fresult;
}