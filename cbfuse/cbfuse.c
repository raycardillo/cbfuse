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

#include "sync_get.h"
#include "sync_store.h"
#include "attribs.h"
#include "dentries.h"
#include "util.h"
#include "common.h"

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
    return NULL;
}

static int cb_getattr(const char *path, struct stat *stbuf)
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
        ATTRIBS_COLLECTION_STRING, ATTRIBS_COLLECTION_STRLEN);
    IfLCBFailGotoDone(rc, -EIO);

    rc = lcb_cmdget_key(cmd, path, strlen(path));
    IfLCBFailGotoDone(rc, -EIO);

    rc = sync_get(_thread_instance, cmd, &result);
    IfLCBFailGotoDone(rc, -EIO);

    // first check the command result code
    IfLCBFailGotoDone(rc, -EIO);

    // now check the actual result status
    IfLCBFailGotoDoneWithRef(result->status, -ENOENT, path);

    // sanity check that we received the expected structure
    if (result->nvalue != CBFUSE_STAT_STRUCT_SIZE) {
        fprintf(stderr, "cb_getattr:nvalue %lu != %lu\n", result->nvalue, CBFUSE_STAT_STRUCT_SIZE);
        fresult = -EBADF;
        goto done;
    }

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

done:
    // free the result
    sync_get_destroy(result);

    return fresult;
}

static int cb_open(const char *path, struct fuse_file_info *fi)
{
    fprintf(stderr, "cb_open:path: %s %0x\n", path, fi->flags);

    char *dirstr = strdup(path);
    //char *basestr = strdup(path);
    char *dname = dirname(dirstr);
    //char *bname = basename(basestr);

    // TODO: reorg all returns and be sure to free memory

    if (strcmp(dname, ROOT_DIR_STRING) != 0) {
        // We only recognize one file.
        return -ENOENT;
    }

    if ((fi->flags & O_ACCMODE) != O_RDONLY) {
        // Only reading allowed.
        return -EACCES;
    }

    lcb_STATUS rc;
    lcb_CMDGET *cmd;
    const char *scope = NULL;
    size_t scope_len = 0;
    const char *collection = "inodes";
    size_t collection_len = strlen(collection);
    sync_get_result *result = NULL;
    rc = lcb_cmdget_create(&cmd);
    if (rc != LCB_SUCCESS) {
        //fprintf(stderr, "lcb_cmdget_create: %s\n", lcb_strerror_short(rc));
        return rc;
    }
    rc = lcb_cmdget_collection(cmd, scope, scope_len, collection, collection_len);
    if (rc != LCB_SUCCESS) {
        //fprintf(stderr, "lcb_cmdget_collection: %s\n", lcb_strerror_short(rc));
        return rc;
    }
    rc = lcb_cmdget_key(cmd, "1234", strlen("1234"));
    if (rc != LCB_SUCCESS) {
        //fprintf(stderr, "lcb_cmdget_key: %s\n", lcb_strerror_short(rc));
        return rc;
    }

    rc = sync_get(_thread_instance, cmd, &result);

    fi->fh = (uint64_t)result;

    return 0;
}

static int cb_create(const char *path, mode_t mode, __unused struct fuse_file_info *fi)
{
    fprintf(stderr, "cb_create:path: %s %0x\n", path, mode);

    int fresult = 0;

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

    fresult = insert_attrib(_thread_instance, path, (S_IFREG | 0755));
    IfFRFailGotoDoneWithRef(path);

    fresult = add_child_to_dentry(_thread_instance, dname, bname);
    IfFRFailGotoDoneWithRef(path);

done:
    return fresult;
}

static int cb_readdir(const char *path, void *buf, fuse_fill_dir_t filler, __unused off_t offset, __unused struct fuse_file_info *fi)
{
    fprintf(stderr, "cb_readdir:path: %s\n", path);

    if (strcmp(path, ROOT_DIR_STRING) != 0) {
        // We only recognize the root directory.
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0);      // Current directory (.)
    filler(buf, "..", NULL, 0);     // Parent directory (..)
    // filler(buf, file_path + 1, NULL, 0);

    return 0;
}

static int cb_read(const char *path, char *buf, size_t size, off_t offset, __unused struct fuse_file_info *fi)
{
    sync_get_result *result = (sync_get_result*)fi->fh;

    char *file_path = "replace_me";
    if (strcmp(path, file_path) != 0)
        return -ENOENT;
    
    size_t file_size = result->nvalue;
    if (offset >= (off_t)file_size) /* Trying to read past the end of file. */
        return 0;

    if (offset + size > file_size) /* Trim the read to the file size. */
        size = file_size - offset;

    memcpy(buf, result->value + offset, size); /* Provide the content. */

    return size;
}

/////

static int insert_root(lcb_INSTANCE *instance) {
    // TODO: Consider refactoring to C++ to take advantage of transaction context with multiple ops

    // add root attrib as directory with 0x755 permissions
    int fresult = insert_attrib(instance, ROOT_DIR_STRING, (S_IFDIR | 0755));
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
            fprintf(stderr, "Unexpected root directory detected. st_mode=0x%0x\n", root_stat.st_mode);
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
