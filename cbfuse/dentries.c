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

#include "dentries.h"
#include "util.h"
#include "common.h"
#include "sync_get.h"
#include "sync_store.h"

int get_dentry_json(lcb_INSTANCE *instance, const char *pkey, cJSON **dentry_json)
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
        DENTRIES_COLLECTION_STRING, DENTRIES_COLLECTION_STRLEN);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdget_key(cmd, pkey, strlen(pkey));
    IfLCBFailGotoDone(rc, -EIO);

    rc = sync_get(instance, cmd, &result);

    // first check the sync command result code
    IfLCBFailGotoDone(rc, -EIO);

    // now check the actual result status
    IfLCBFailGotoDoneWithRef(result->status, -ENOENT, pkey);

    *dentry_json = cJSON_ParseWithLength(result->value, result->nvalue);
    IfNULLGotoDoneWithRef(dentry_json, -EIO, pkey);

done:
    // free the result
    sync_get_destroy(result);

    return fresult;
}

// directory entries are mostly used by readdir
// represented as JSON because it's mostly dynamic character data
// note that the path-key may not be the full path
// {
//   "cr": "current-dir-path-key",
//   "pr": "parent-dir-path-key",
//   "ch": [
//     "some-child-dir-path-key",
//     "other-child-dir-path-key"
//   ]
// }
static char *create_dentry(const char *current_pkey, const char *parent_pkey, const char *child_pkeys[], int child_nkeys)
{
    char *dentry_string = NULL;
    cJSON *dentry_json = cJSON_CreateObject();
    if (dentry_json == NULL) {
        goto done;
    }

    if (!cJSON_AddItemToObject(dentry_json, DENTRY_CURRENT, cJSON_CreateStringReference(current_pkey))) {
        goto done;
    }

    if (!cJSON_AddItemToObject(dentry_json, DENTRY_PARENT, cJSON_CreateStringReference(parent_pkey))) {
        goto done;
    }

    cJSON *children_json = cJSON_CreateArray();
    for (int i=0; i < child_nkeys; i++) {
        if (!cJSON_AddItemToArray(children_json, cJSON_CreateStringReference(child_pkeys[i]))) {
            goto done;
        }
    }
    if (!cJSON_AddItemToObject(dentry_json, DENTRY_CHILDREN, children_json)) {
        goto done;
    }

    dentry_string = cJSON_PrintUnformatted(dentry_json);
    if (dentry_string == NULL) {
        fprintf(stderr, "%s:%s:%d Failed to create JSON.\n", __FILENAME__, __func__, __LINE__);
    }

done:
    cJSON_Delete(dentry_json);
    return dentry_string;
}

int add_child_to_dentry(lcb_INSTANCE *instance, const char *pkey, const char *child_pkey)
{
    cJSON *dentry_json = NULL;
    int fresult = get_dentry_json(instance, pkey, &dentry_json);
    if (fresult != 0) {
        return fresult;
    }

    cJSON *children_json = cJSON_GetObjectItemCaseSensitive(dentry_json, DENTRY_CHILDREN);
    IfNULLGotoDoneWithRef(dentry_json, -EIO, pkey);

    IfFalseGotoDoneWithRef(cJSON_IsArray(children_json), -EIO, pkey);

    IfFalseGotoDoneWithRef(
        cJSON_AddItemToArray(children_json, cJSON_CreateString(child_pkey)),
        -EIO,
        pkey
    );

    char *dentry_string = cJSON_PrintUnformatted(dentry_json);
    IfNULLGotoDoneWithRef(dentry_string, -EIO, pkey);

    lcb_STATUS rc;
    lcb_CMDSTORE *cmd;

    // replace the previous dentry
    rc = lcb_cmdstore_create(&cmd, LCB_STORE_REPLACE);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_collection(
        cmd,
        DEFAULT_SCOPE_STRING, DEFAULT_SCOPE_STRLEN,
        DENTRIES_COLLECTION_STRING, DENTRIES_COLLECTION_STRLEN);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_key(cmd, pkey, strlen(pkey));
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_value(cmd, dentry_string, strlen(dentry_string));
    IfLCBFailGotoDone(rc, -EIO);

    sync_store_result *result = NULL;
    rc = sync_store(instance, cmd, &result);

    // first check the sync command result code
    IfLCBFailGotoDone(rc, -EIO);

    // now check the actual result status
    IfLCBFailGotoDoneWithRef(result->status, -ENOENT, pkey);

done:
    return fresult;
}

int insert_root_dentry(lcb_INSTANCE *instance)
{
    int fresult = 0;

    const char *dentry = create_dentry(
        ROOT_DIR_STRING,
        ROOT_DIR_STRING,
        NULL,
        0
    );
    if (dentry == NULL) {
        fprintf(stderr, "%s:%s:%d Failed to create ROOT dentry JSON.\n", __FILENAME__, __func__, __LINE__);
        goto done;
    }

    lcb_STATUS rc;
    lcb_CMDSTORE *cmd;

    // insert only if key doesn't already exist
    rc = lcb_cmdstore_create(&cmd, LCB_STORE_INSERT);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_collection(
        cmd,
        DEFAULT_SCOPE_STRING, DEFAULT_SCOPE_STRLEN,
        DENTRIES_COLLECTION_STRING, DENTRIES_COLLECTION_STRLEN);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_key(cmd, ROOT_DIR_STRING, ROOT_DIR_STRLEN);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdstore_value(cmd, dentry, strlen(dentry));
    IfLCBFailGotoDone(rc, -EIO);

    sync_store_result *result = NULL;
    rc = sync_store(instance, cmd, &result);

    // first check the sync command result code
    IfLCBFailGotoDone(rc, -EIO);

    // now check the actual result status
    IfLCBFailGotoDoneWithRef(result->status, -ENOENT, ROOT_DIR_STRING);

done:
    return fresult;
}
