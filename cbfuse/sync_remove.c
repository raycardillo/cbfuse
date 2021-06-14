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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libcouchbase/couchbase.h>

#include "sync_remove.h"

static void sync_remove_callback(__unused lcb_INSTANCE *instance, __unused int cbtype, const lcb_RESPREMOVE *resp)
{
    sync_remove_result *result;
    lcb_respremove_cookie(resp, (void**)&result);
    if (result == NULL) {
        return;
    }

    lcb_STATUS status = lcb_respremove_status(resp);
    result->status = status;
    if (status == LCB_SUCCESS) {
        // TBD - nothing extra needed currently
    }
}

void sync_remove_init(lcb_INSTANCE *instance)
{
    lcb_install_callback(instance, LCB_CALLBACK_REMOVE, (lcb_RESPCALLBACK)sync_remove_callback);
}

lcb_STATUS sync_remove(lcb_INSTANCE *instance, lcb_CMDREMOVE *cmd, sync_remove_result **result)
{
    lcb_STATUS rc;
    *result = calloc(1, sizeof(sync_remove_result));
    rc = lcb_remove(instance, *result, cmd);
    if (rc != LCB_SUCCESS) {
        fprintf(stderr, "  sync_remove:lcb_remove: %s\n", lcb_strerror_short(rc));
        return rc;
    }

    rc = lcb_cmdremove_destroy(cmd);
    rc = lcb_wait(instance, LCB_WAIT_DEFAULT);

    return rc;
}

void sync_remove_destroy(sync_remove_result *result)
{
    if (result != NULL) {
        free(result);
    }
}