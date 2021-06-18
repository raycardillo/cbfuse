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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libcouchbase/couchbase.h>

#include "util.h"
#include "sync_get.h"

static void sync_get_callback(__unused lcb_INSTANCE *instance, __unused int cbtype, const lcb_RESPGET *resp)
{
    sync_get_result *result;
    lcb_respget_cookie(resp, (void**)&result);
    if (result == NULL) {
        return;
    }

    lcb_STATUS status = lcb_respget_status(resp);
    result->status = status;
    if (status == LCB_SUCCESS) {
        lcb_respget_cas(resp, &result->cas);
        lcb_respget_flags(resp, &result->flags);
        
        const char *key, *value;
        size_t nkey, nvalue;
        lcb_respget_key(resp, &key, &nkey);
        lcb_respget_value(resp, &value, &nvalue);

        // make a copy of the allocated data
        result->key = strdup(key);
        result->nkey = nkey;
        result->value = memdup(value, nvalue);
        result->nvalue = nvalue;
    }
}

void sync_get_init(lcb_INSTANCE *instance)
{
    lcb_install_callback(instance, LCB_CALLBACK_GET, (lcb_RESPCALLBACK)sync_get_callback);
}

lcb_STATUS sync_get(lcb_INSTANCE *instance, lcb_CMDGET *cmd, sync_get_result **result)
{
    lcb_STATUS rc;
    *result = calloc(1, sizeof(sync_get_result));
    rc = lcb_get(instance, *result, cmd);
    if (rc != LCB_SUCCESS) {
        fprintf(stderr, "  sync_get:lcb_get: %s\n", lcb_strerror_short(rc));
        return rc;
    }

    rc = lcb_cmdget_destroy(cmd);
    rc = lcb_wait(instance, LCB_WAIT_DEFAULT);

    return rc;
}

void sync_get_destroy(sync_get_result *result)
{
    if (result != NULL) {
        free((void*)result->key);
        free((void*)result->value);
        free(result);
    }
}