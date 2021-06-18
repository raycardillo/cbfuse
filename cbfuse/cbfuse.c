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

#include <cbfuse.h>
#include "util.h"
#include "common.h"
#include "sync_get.h"
#include "sync_store.h"
#include "sync_remove.h"
#include "stats.h"
#include "dentries.h"
#include "data.h"

// We're using high-level FUSE ops which are synchronous
// and from those we're making synchronous calls to Couchbase.
static lcb_INSTANCE *_lcb_instance = NULL;

/////

static void open_callback(__unused lcb_INSTANCE *instance, lcb_STATUS rc)
{
    fprintf(stderr, "open bucket: %s\n", lcb_strerror_short(rc));
}

/////

// Initialize filesystem
static void *cbfuse_init(__unused struct fuse_conn_info *conn)
{
    fprintf(stderr, "cbfuse_init\n");

    conn->async_read = false;
    conn->want |= FUSE_CAP_BIG_WRITES;

    return NULL;
}

// Get file attributes
static int cbfuse_getattr(const char *path, struct stat *stbuf)
{
    fprintf(stderr, "cbfuse_getattr path:%s\n", path);

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

    rc = sync_get(_lcb_instance, cmd, &result);

    // first check the sync command result code
    IfLCBFailGotoDone(rc, -EIO);

    // now check the actual result status
    IfLCBFailGotoDoneWithRef(result->status, -ENOENT, path);

    // sanity check that we received the expected structure
    IfTrueGotoDoneWithRef((result->nvalue != CBFUSE_STAT_STRUCT_SIZE), -EBADF, path);

    // get the fuse context (for uid and gid)
    struct fuse_context *fc = fuse_get_context();

    // copy the stat binary into the stat buffer
    cbfuse_stat *stres = (cbfuse_stat*)result->value;
    stbuf->st_uid = fc->uid;
    stbuf->st_gid = fc->gid;
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
    sync_get_destroy(result);
    return fresult;
}

// File open operation
static int cbfuse_open(const char *path, struct fuse_file_info *fi)
{
    fprintf(stderr, "cbfuse_open path:%s flags:0x%04x\n", path, fi->flags);
    
    int fresult = 0;

    size_t npath = strlen(path);
    IfTrueGotoDoneWithRef((npath > MAX_PATH_LEN), ENAMETOOLONG, path);

    cbfuse_stat stat = {0};
    fresult = get_stat(_lcb_instance, path, &stat, NULL);
    IfFRErrorGotoDoneWithRef(path);

done:
    return fresult;
}

// Create and open a file
static int cbfuse_create(const char *path, mode_t mode, __unused struct fuse_file_info *fi)
{
    fprintf(stderr, "cbfuse_create path:%s mode:0x%02X\n", path, mode);

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

    size_t npath = strlen(path);
    IfTrueGotoDoneWithRef((npath > MAX_PATH_LEN), ENAMETOOLONG, path);

    fresult = insert_stat(_lcb_instance, path, mode);
    IfFRErrorGotoDoneWithRef(path);

    fresult = add_child_to_dentry(_lcb_instance, dname, bname);
    IfFRErrorGotoDoneWithRef(path);

done:
    free(dirstr);
    free(basestr);
    return fresult;
}

// Remove a file
static int cbfuse_unlink(const char *path)
{
    fprintf(stderr, "cbfuse_unlink path:%s\n", path);

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

    // TODO: These operations can be in a transaction or at least scheduled as a batch

    // Only check the stat operation - others can fail silently and may be useful for error recovery
    fresult = remove_data(_lcb_instance, path);
    fresult = remove_child_from_dentry(_lcb_instance, dname, bname);
    fresult = remove_stat(_lcb_instance, path);
    IfFRErrorGotoDoneWithRef(path);

done:
    free(dirstr);
    free(basestr);
    return fresult;
}

// Read directory
static int cbfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, __unused struct fuse_file_info *fi)
{
    fprintf(stderr, "cbfuse_readdir path:%s\n", path);

    cJSON *dentry_json = NULL;
    int fresult = get_dentry_json(_lcb_instance, path, &dentry_json);
    IfFRErrorGotoDoneWithRef(path);

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

// Read data from an open file
static int cbfuse_read(const char *path, char *buf, size_t size, off_t offset, __unused struct fuse_file_info *fi)
{
    fprintf(stderr, "cbfuse_read path:%s size:%lu offset:%llu\n", path, size, offset);

    int fresult = read_data(_lcb_instance, path, buf, size, offset);
    IfFRErrorGotoDoneWithRef(path);

done:
    return fresult;
}

// Write data to an open file
static int cbfuse_write(const char *path, const char *buf, size_t size, off_t offset, __unused struct fuse_file_info *fi)
{
    fprintf(stderr, "cbfuse_write path:%s size:%lu offset:%llu\n", path, size, offset);

    int fresult = write_data(_lcb_instance, path, buf, size, offset);
    IfFRErrorGotoDoneWithRef(path);

done:
    return fresult;
}

// Change the permission bits of a file
static int cbfuse_chmod(const char *path, mode_t mode)
{
    fprintf(stderr, "cbfuse_chmod path:%s mode:0x%04X\n", path, mode);

    int fresult = update_stat_mode(_lcb_instance, path, mode);
    IfFRErrorGotoDoneWithRef(path);

done:
    return fresult;
}

// Change the size of a file
static int cbfuse_truncate(const char *path, off_t offset)
{
    fprintf(stderr, "cbfuse_truncate path:%s offset:%llu\n", path, offset);

    int fresult = truncate_data(_lcb_instance, path, offset);
    IfFRErrorGotoDoneWithRef(path);

done:
    return fresult;
}

// Change the access and modification times of a file with nanosecond resolution
static int cbfuse_utimens(const char *path, const struct timespec tv[2])
{
    fprintf(stderr, "cbfuse_utimens path:%s", path);

    int fresult = update_stat_utimens(_lcb_instance, path, tv);
    IfFRErrorGotoDoneWithRef(path);

done:
    return fresult;
}

static int cbfuse_mkdir(const char * path, mode_t mode)
{
    fprintf(stderr, "cbfuse_mkdir path:%s mode:0x%02X\n", path, mode);

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
        (mode|S_IFDIR),
        -EINVAL,
        path
    );

    // TODO: These operations can be in a transaction or at least scheduled as a batch

    fresult = insert_stat(_lcb_instance, path, mode);
    IfFRErrorGotoDoneWithRef(path);

    fresult = add_new_dentry(_lcb_instance, path, path, dname);
    IfFRErrorGotoDoneWithRef(path);

    fresult = add_child_to_dentry(_lcb_instance, dname, bname);
    IfFRErrorGotoDoneWithRef(path);

done:
    free(dirstr);
    free(basestr);
    return fresult;
}

/////

static int insert_root(lcb_INSTANCE *instance) {
    // TODO: Consider refactoring to C++ to take advantage of transaction context with multiple ops

    // add root stat as directory with 0x755 permissions
    int fresult = insert_stat(instance, ROOT_DIR_STRING, (S_IFDIR | 0755));
    IfFRErrorGotoDoneWithRef(ROOT_DIR_STRING);

    fresult = add_new_dentry(instance, ROOT_DIR_STRING, ROOT_DIR_STRING, NULL);
    IfFRErrorGotoDoneWithRef(ROOT_DIR_STRING);

done:
    return fresult;
}

///// INITIALIZATION AND BOOTSTRAPPING

static struct fuse_operations cb_filesystem_operations = {
    .init     = cbfuse_init,
    .getattr  = cbfuse_getattr,
    .open     = cbfuse_open,
    .create   = cbfuse_create,
    .unlink   = cbfuse_unlink,
    .read     = cbfuse_read,
    .readdir  = cbfuse_readdir,
    .write    = cbfuse_write,
    .chmod    = cbfuse_chmod,
    .truncate = cbfuse_truncate,
    .utimens  = cbfuse_utimens,
    .mkdir    = cbfuse_mkdir
};

struct cbfuse_config {
    char *cb_connect;
    char *cb_username;
    char *cb_password;
};

enum {
     KEY_HELP,
     KEY_VERSION
};

#define CBFUSE_OPT(t, p, v) { t, offsetof(struct cbfuse_config, p), v }

static struct fuse_opt cbfuse_opts[] = {
    CBFUSE_OPT("cb_connect=%s",     cb_connect, 0),
    CBFUSE_OPT("--cb_connect=%s",   cb_connect, 0),
    CBFUSE_OPT("cb_username=%s",    cb_username, 0),
    CBFUSE_OPT("--cb_username=%s",  cb_username, 0),
    CBFUSE_OPT("cb_password=%s",    cb_password, 0),
    CBFUSE_OPT("--cb_password=%s",  cb_password, 0),

    FUSE_OPT_KEY("-V",              KEY_VERSION),
    FUSE_OPT_KEY("--version",       KEY_VERSION),
    FUSE_OPT_KEY("-h",              KEY_HELP),
    FUSE_OPT_KEY("--help",          KEY_HELP),
    FUSE_OPT_END
};

static void usage(const char *name) {
    fprintf(stderr,
        "usage: %s mountpoint [options]\n"
        "\n"
        "general options:\n"
        "  -o opt,[opt...]  mount options\n"
        "  -h   --help      print help\n"
        "  -V   --version   print version\n"
        "\n"
        "couchbase connection options:\n"
        "  -o cb_connect=COUCHBASE_CONNECT_STRING\n"
        "  -o cb_username=COUCHBASE_SASL_USERNAME\n"
        "  -o cb_password=COUCHBASE_SASL_PASSWORD\n"
        "  --cb_connect=COUCHBASE_CONNECT_STRING\n"
        "  --cb_username=COUCHBASE_SASL_USERNAME\n"
        "  --cb_password=COUCHBASE_SASL_PASSWORD\n"
        "\n"
        "example:\n"
        "  %s ~/mountdir --cb_connect=couchbase://127.0.0.1/cbfuse --cb_username=rcardillo --cb_password=rcardillo\n"
        , name, name
    );
}

static int cbfuse_opt_proc(void *data, __unused const char *arg, int key, struct fuse_args *outargs)
{
    __unused struct cbfuse_config *config = data;

    switch (key) {
    case KEY_HELP:
        usage(basename(outargs->argv[0]));
        // fuse_opt_add_arg(outargs, "--help");
        // fuse_main(outargs->argc, outargs->argv, &cb_filesystem_operations, NULL);
        exit(EXIT_FAILURE);

    case KEY_VERSION:
        fprintf(stderr, "%s version: %d.%d.%d\n", basename(outargs->argv[0]), cbfuse_VERSION_MAJOR, cbfuse_VERSION_MINOR, cbfuse_VERSION_PATCH);
        fuse_opt_add_arg(outargs, "--version");
        fuse_main(outargs->argc, outargs->argv, &cb_filesystem_operations, NULL);
        exit(EXIT_SUCCESS);
    }

    return 1;
}

int main(int argc, char **argv)
{    
    if ((getuid() == 0) || (geteuid() == 0)) {
        fprintf(stderr, "Running as root is not allowed because it may open security holes.\n");
        return 1;
    }

    ///// PARSE ARGUMENTS

    struct fuse_args fargs = FUSE_ARGS_INIT(argc, argv);
    struct cbfuse_config config = {0};

    int fresult = fuse_opt_parse(&fargs, &config, cbfuse_opts, cbfuse_opt_proc);
    IfFRFailGotoDoneWithRef("Could not parse options");

    // set FUSE single-threaded mode
    fresult = fuse_opt_add_arg(&fargs, "-s");
    IfFRFailGotoDoneWithRef("Could not add FUSE single-threaded mode option.");

    // set FUSE foreground mode
    fresult = fuse_opt_add_arg(&fargs, "-f");
    IfFRFailGotoDoneWithRef("Could not add FUSE foreground mode option.");

    // make sure a reasonable connection string has been provided
    if (config.cb_connect == NULL || strlen(config.cb_connect) < 5) {
        fprintf(stderr, "Couchbase connection string must be provided.\n\n");
        usage(basename(argv[0]));
        exit(EXIT_FAILURE);
    }

    ///// CONNECT TO COUCHBASE

    lcb_CREATEOPTS *create_options = NULL;
    lcb_createopts_create(&create_options, LCB_TYPE_CLUSTER);
    lcb_createopts_connstr(create_options, config.cb_connect, strlen(config.cb_connect));
    if (config.cb_username != NULL || config.cb_password != NULL) {
        lcb_createopts_credentials(
            create_options,
            config.cb_username, strlen(config.cb_username),
            config.cb_password, strlen(config.cb_password)
        );
    }

    lcb_STATUS rc;
    rc = lcb_create(&_lcb_instance, create_options);
    lcb_createopts_destroy(create_options);
    IfLCBFailGotoDoneWithMsg(rc, EXIT_FAILURE, "Couldn't create a couchbase instance.");
    IfNULLGotoDoneWithRef(_lcb_instance, EXIT_FAILURE, "Couldn't create a couchbase instance.");

    rc = lcb_connect(_lcb_instance);
    IfLCBFailGotoDoneWithMsg(rc, EXIT_FAILURE, "Couldn't create couchbase connect handle.");

    rc = lcb_wait(_lcb_instance, LCB_WAIT_DEFAULT);
    IfLCBFailGotoDoneWithMsg(rc, EXIT_FAILURE, "Couldn't connect to couchbase.");

    rc = lcb_get_bootstrap_status(_lcb_instance);
    IfLCBFailGotoDoneWithMsg(rc, EXIT_FAILURE, "Couldn't bootstrap couchbase connection (make sure server is running).");

    // install callbacks for the initialized instance
    lcb_set_open_callback(_lcb_instance, open_callback);
    sync_get_init(_lcb_instance);
    sync_store_init(_lcb_instance);
    sync_remove_init(_lcb_instance);

    const char *bucket = "cbfuse";
    rc = lcb_open(_lcb_instance, bucket, strlen(bucket));
    IfLCBFailGotoDoneWithMsg(rc, EXIT_FAILURE, "Couldn't create a couchbase open bucket request.");

    rc = lcb_wait(_lcb_instance, LCB_WAIT_DEFAULT);
    IfLCBFailGotoDoneWithMsg(rc, EXIT_FAILURE, "Couldn't open couchbase bucket.");

    ///// VERIFY OR INSTALL ROOT DIR

    struct stat root_stat;
    int get_root_rc = cbfuse_getattr(ROOT_DIR_STRING, &root_stat);
    if (get_root_rc == 0) {
        if (!S_ISDIR(root_stat.st_mode)) {
            // we received something but it's not a directory
            fprintf(stderr, "Unexpected root directory detected. st_mode=0x%02x\n", root_stat.st_mode);
            fresult = EXIT_FAILURE;
            goto done;
        }
    } else if (get_root_rc == -ENOENT) {
        if (insert_root(_lcb_instance) != 0) {
            fprintf(stderr, "Unexpected error when trying to create root directory.\n");
            fresult = EXIT_FAILURE;
            goto done;
        }
    } else {
        fprintf(stderr, "Unexpected error when trying to find root directory.\n");
        fresult = EXIT_FAILURE;
        goto done;
    }

    ///// MOUNT THE FUSE FILESYSTEM AND START THE EVENT LOOP

    fresult = fuse_main(fargs.argc, fargs.argv, &cb_filesystem_operations, NULL);
    IfFRErrorGotoDoneWithRef("FUSE error encountered.");

done:
    fuse_opt_free_args(&fargs);
	free(config.cb_connect);
	free(config.cb_username);
	free(config.cb_password);

    if (_lcb_instance) {
        lcb_destroy(_lcb_instance);
    }

	return fresult;
}
