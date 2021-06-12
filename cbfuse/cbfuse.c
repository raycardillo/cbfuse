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
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <libgen.h>

#include <libcouchbase/couchbase.h>
#include <cjson/cJSON.h>
#include <xxhash.h>
#include <fuse.h>

#include "util.h"
#include "common.h"
#include "sync_get.h"
#include "sync_store.h"
#include "stats.h"
#include "dentries.h"
#include "data.h"

// IMPORTANT:
// When running, use the '-s' option to force single-threaded operation for now
//
// NOTE: Single threaded for now but to avoid using a Mutex, we can have a per thread initializer.
// TODO: Implement a mutex or thread local storage to support multi-threaded operations.
_Thread_local lcb_INSTANCE *_thread_instance;

/////

static void die(__unused lcb_INSTANCE *instance, const char *msg, lcb_STATUS err)
{
    fprintf(stderr, "%s. Received code 0x%X (%s)\n", msg, err, lcb_strerror_short(err));
    exit(EXIT_FAILURE);
}

/////

static void open_callback(__unused lcb_INSTANCE *instance, lcb_STATUS rc)
{
    fprintf(stderr, "open bucket: %s\n", lcb_strerror_short(rc));
}

/////

static void *cb_init(__unused struct fuse_conn_info *conn)
{
    conn->async_read = false;
    conn->want = FUSE_CAP_BIG_WRITES;
    return NULL;
}

static int cb_getattr(const char *path, struct stat *stbuf)
{
    fprintf(stderr, "cb_getattr path:%s\n", path);

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

    size_t npath = strlen(path);
    IfTrueGotoDoneWithRef((npath > MAX_PATH_LEN), ENAMETOOLONG, path);

    rc = lcb_cmdget_key(cmd, path, npath);
    IfLCBFailGotoDone(rc, -EIO);

    rc = sync_get(_thread_instance, cmd, &result);

    // first check the sync command result code
    IfLCBFailGotoDone(rc, -EIO);

    // now check the actual result status
    IfLCBFailGotoDoneWithRef(result->status, -ENOENT, path);

    // sanity check that we received the expected structure
    IfTrueGotoDoneWithRef((result->nvalue != CBFUSE_STAT_STRUCT_SIZE), -EBADF, path);

    // copy the stat binary into the stat buffer
    cbfuse_stat *stres = (cbfuse_stat*)result->value;
    stbuf->st_uid = geteuid();
    stbuf->st_gid = getegid();
    stbuf->st_mode = stres->st_mode;
    stbuf->st_atime = stres->st_atime;
    stbuf->st_atimensec = stres->st_atimensec;
    stbuf->st_mtime = stres->st_mtime;
    stbuf->st_mtimensec = stres->st_mtimensec;
    stbuf->st_ctime = stres->st_ctime;
    stbuf->st_ctimensec = stres->st_ctimensec;
    stbuf->st_size = stres->st_size;

    fprintf(stderr, "%s:%s:%d %s size:%lld\n", __FILENAME__, __func__, __LINE__, path, stbuf->st_size);

done:
    // free the result
    sync_get_destroy(result);

    return fresult;
}

static int cb_open(const char *path, struct fuse_file_info *fi)
{
    fprintf(stderr, "cb_open path:%s flags:0x%04x\n", path, fi->flags);
    
    int fresult = 0;

    size_t npath = strlen(path);
    IfTrueGotoDoneWithRef((npath > MAX_PATH_LEN), ENAMETOOLONG, path);

    cbfuse_stat stat = {0};
    fresult = get_stat(_thread_instance, path, &stat, NULL);
    IfFRFailGotoDoneWithRef(path);

done:
    return fresult;
}

static int cb_create(const char *path, mode_t mode, __unused struct fuse_file_info *fi)
{
    fprintf(stderr, "cb_create path:%s mode:0x%02x\n", path, mode);

    int fresult = 0;

    size_t npath = strlen(path);
    IfTrueGotoDoneWithRef((npath > MAX_PATH_LEN), ENAMETOOLONG, path);

    char *dirstr = strdup(path);
    char *basestr = strdup(path);
    char *dname = dirname(dirstr);
    char *bname = basename(basestr);

    IfTrueGotoDoneWithRef(
        (dname == NULL || bname == NULL),
        -ENOENT,
        path
    );

    IfFalseGotoDoneWithRef(
        S_ISREG(mode),
        -EINVAL,
        path
    );

    fresult = insert_stat(_thread_instance, path, mode);
    IfFRFailGotoDoneWithRef(path);

    fresult = add_child_to_dentry(_thread_instance, dname, bname);
    IfFRFailGotoDoneWithRef(path);

done:
    return fresult;
}

static int cb_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, __unused struct fuse_file_info *fi)
{
    fprintf(stderr, "cb_readdir path:%s\n", path);

    cJSON *dentry_json = NULL;
    int fresult = get_dentry_json(_thread_instance, path, &dentry_json);
    IfFRFailGotoDoneWithRef(path);

    int child_offset = 0;
    cJSON *child;
    cJSON *children = cJSON_GetObjectItemCaseSensitive(dentry_json, DENTRY_CHILDREN);
    IfFalseGotoDoneWithRef(cJSON_IsArray(children), 0, path);

    cJSON_ArrayForEach(child, children) {
        // skip to the offset (underlying implementation is a linked list)
        if (child_offset++ >= offset) {
            // fill with the next offset or zero if no more
            IfFalseGotoDoneWithRef(cJSON_IsString(child), 0, path);
            fresult = filler(buf, cJSON_GetStringValue(child), NULL, child_offset);
        }
    }

done:
    cJSON_Delete(dentry_json);
    return fresult;
}

static int cb_read(const char *path, char *buf, size_t size, off_t offset, __unused struct fuse_file_info *fi)
{
    fprintf(stderr, "cb_read path:%s size:%lu offset:%llu\n", path, size, offset);

    int fresult = read_data(_thread_instance, path, buf, size, offset);
    IfFRFailGotoDoneWithRef(path);

done:
    return fresult;
}

static int cb_write(const char *path, const char *buf, size_t size, off_t offset, __unused struct fuse_file_info *fi)
{
    fprintf(stderr, "cb_write path:%s size:%lu offset:%llu\n", path, size, offset);

    int fresult = write_data(_thread_instance, path, buf, size, offset);
    IfFRFailGotoDoneWithRef(path);

done:
    return fresult;
}

/////

static int insert_root(lcb_INSTANCE *instance) {
    // TODO: Consider refactoring to C++ to take advantage of transaction context with multiple ops

    // add root stat as directory with 0x755 permissions
    int fresult = insert_stat(instance, ROOT_DIR_STRING, (S_IFDIR | 0755));
    IfFRFailGotoDoneWithRef(ROOT_DIR_STRING);

    fresult = insert_root_dentry(instance);
    IfFRFailGotoDoneWithRef(ROOT_DIR_STRING);

done:
    return fresult;
}

/////

static struct fuse_operations cb_filesystem_operations = {
    .init    = cb_init,
    .getattr = cb_getattr,
    .open    = cb_open,
    .create  = cb_create,
    .read    = cb_read,
    .readdir = cb_readdir,
    .write   = cb_write,
};

__unused
static void usage(const char *name)
{
	fprintf(stderr,
    "Usage: %s couchbase://host/bucket mountpoint [options]\n"
    "\n"
    "    -h   --help            print help\n"
    "    -V   --version         print version\n"
    "    -f                     foreground operation\n"
    "    -s                     disable multi-threaded operation\n"
    "    -d, --debug            print some debugging information (implies -f)\n"
    "    -v, --verbose          print ssh replies and messages\n"
    "    -o opt,[opt...]        mount options\n"
    // "    -o dir_cache=BOOL      enable caching of directory contents (names,\n"
    // "                           attributes, symlink targets) {yes,no} (default: yes)\n"
    // "    -o dcache_max_size=N   sets the maximum size of the directory cache (default: 10000)\n"
    // "    -o dcache_timeout=N    sets timeout for directory cache in seconds (default: 20)\n"
    // "    -o dcache_{stat,link,dir}_timeout=N\n"
    // "                           sets separate timeout for {attributes, symlinks, names}\n"
    // "    -o dcache_clean_interval=N\n"
    // "                           sets the interval for automatic cleaning of the\n"
    // "                           cache (default: 60)\n"
    // "    -o dcache_min_clean_interval=N\n"
    // "                           sets the interval for forced cleaning of the\n"
    // "                           cache if full (default: 5)\n"
    "    -o username=UNAME      read password from stdin (only for pam_mount!)\n"
    "    -o password=PWORD      read password from stdin (only for pam_mount!)\n"
    "    -o password_stdin      read password from stdin (only for pam_mount!)\n"
    "\n"
    "FUSE Options:\n",
    name);
}

int main(int argc, char **argv)
{
    if ((getuid() == 0) || (geteuid() == 0)) {
        fprintf(stderr, "Running as root is not allowed because it may open security holes.\n");
        return 1;
    }

    ///// CONNECT TO COUCHBASE

    // TODO: more advanced arg processing (see SSHFS)
    // TODO: remove temporary dev hard-coded connect values

    // if (argc < 2) {
    //     fprintf(stderr, "Usage: %s couchbase://$HOSTS/$BUCKET?$OPTIONS [ password [ username ] ]\n", argv[0]);
    //     exit(EXIT_FAILURE);
    // }
    char *connect = "couchbase://127.0.0.1/cbfuse";
    char *username = "raycardillo";
    char *password = "raycardillo";

    lcb_CREATEOPTS *create_options = NULL;
    lcb_createopts_create(&create_options, LCB_TYPE_CLUSTER);
    lcb_createopts_connstr(create_options, connect, strlen(connect));
    if (1) { // TODO: if credentials provided
        lcb_createopts_credentials(create_options, username, strlen(username), password, strlen(password));
    }

    lcb_STATUS rc;
    rc = lcb_create(&_thread_instance, create_options);
    lcb_createopts_destroy(create_options);
    rc = lcb_connect(_thread_instance);
    if (rc != LCB_SUCCESS) {
        die(NULL, "Couldn't create couchbase connect handle", rc);
    }

    rc = lcb_wait(_thread_instance, LCB_WAIT_DEFAULT);

    rc = lcb_get_bootstrap_status(_thread_instance);
    if (rc != LCB_SUCCESS) {
        die(_thread_instance, "Couldn't bootstrap connection", rc);
    }

    // install callbacks for the initialized instance
    lcb_set_open_callback(_thread_instance, open_callback);
    sync_get_init(_thread_instance);
    sync_store_init(_thread_instance);

    const char *bucket = "cbfuse";
    rc = lcb_open(_thread_instance, bucket, strlen(bucket)),

    rc = lcb_wait(_thread_instance, LCB_WAIT_DEFAULT);

    if (rc != LCB_SUCCESS) {
        die(_thread_instance, "Couldn't open bucket", rc);
    }

    ///// VERIFY OR INSTALL ROOT DIR

    struct stat root_stat;
    int get_root_rc = cb_getattr(ROOT_DIR_STRING, &root_stat);
    if (get_root_rc == 0) {
        if (!S_ISDIR(root_stat.st_mode)) {
            // we received something but it's not a directory
            fprintf(stderr, "Unexpected root directory detected. st_mode=0x%02x\n", root_stat.st_mode);
            exit(EXIT_FAILURE);
        }
    } else if (get_root_rc == -ENOENT) {
        if (insert_root(_thread_instance) != 0) {
            fprintf(stderr, "Unexpected error when trying to create root directory.\n");
            exit(EXIT_FAILURE);
        }
    } else {
        fprintf(stderr, "Unexpected error when trying to find root directory.\n");
        exit(EXIT_FAILURE);
    }

    return fuse_main(argc, argv, &cb_filesystem_operations, NULL);

    // TODO: register sig handler and call destroy and clean-up
    /* Now that we're all done, close down the connection handle */
    // lcb_destroy(_thread_instance);
    // return 0;
}
