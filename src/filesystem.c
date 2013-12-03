#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <linux/limits.h>

#include <zip.h>

#include <sf/utils.h>
#include <sf/log.h>
#include <sf/list.h>

#include "filesystem.h"

#if defined(__WIN32__)
    static char seperator = '\\';
#else
    static char seperator = '/';
#endif


struct file {
    struct zip      *parent;

    int isinzip;
    union {
        struct zip_file *zfile;
        FILE            *file;
    };
};

struct directory {
    char name[NAME_MAX];

    int iszip;
    int isopened;
    union {
        struct zip *zip;
        DIR        *dir;
    };

    int nopens;
};

static struct filesystem {
    int isinited;
    sf_list_t         directories;
} fs;


static int get_directory_pathname(struct directory *d, char *buf, size_t count)
{
    struct directory *ptr;
    sf_list_iter_t iter;
    int nwritten = 0;

    buf[0] = '/';
    *++buf = '\0';

    if (sf_list_begin(&fs.directories, &iter)) do {
        int n;
        int len;

        ptr = sf_list_iter_elt(&iter);
        len = strlen(ptr->name);
        n = snprintf(buf, count, "%s%c", ptr->name, seperator);
        ++len;
        nwritten += n;
        if (n != len) {
            break;
        }

        buf += n;
        count -= n;
    } while (ptr != d && sf_list_iter_next(&iter));

    return nwritten;

}

static int directory_init(struct directory *d, const char *pathname)
{
    struct stat statbuf;

    if (stat(pathname, &statbuf) < 0) {
        sf_log(SF_LOG_ERR, "failed to stat file %s", pathname);
        return SF_ERR;
    }

    strncpy(d->name, pathname, NAME_MAX);

    if (S_ISDIR(statbuf.st_mode)) {
        d->isopened = 0;
        d->iszip  = 0;
    } else {
        int err;
        if ((d->zip = zip_open(pathname, 0, &err)) == NULL) {
            char buf[1024];
            zip_error_to_str(buf, 1024, err, errno);
            sf_log(SF_LOG_ERR, "failed to open %s: %s\n",
                   pathname, buf);
            return SF_ERR;
        }
        d->iszip = 1;
        d->isopened = 1;
    }

    d->nopens = 0;

    return SF_OK;
}

static int directory_open(struct directory *d)
{
    if (d->isopened) {
        return SF_OK;
    }

    if (d->iszip) {
        assert(0);
    } else {
        char buf[PATH_MAX];

        get_directory_pathname(d, buf, PATH_MAX);
        if ((d->dir = opendir(buf)) == NULL) {
            sf_log(SF_LOG_ERR, "failed to open directory %s", d->name);
            return SF_ERR;
        }
    }
    d->isopened = 1;
    return SF_OK;
}

static void directory_close(void *elt)
{
    struct directory *d = elt;

    if (d->isopened) {
        if (d->iszip) {
            zip_close(d->zip);
        } else {
            closedir(d->dir);
        }
    }
}


int fs_init(int argc, char **argv)
{
    char buf[PATH_MAX];
    int  len;
    char *ptr;
    sf_list_def_t def;

    if (fs.isinited) {
        sf_log(SF_LOG_WARN, "fs_init: filesystem already initialized.");
        return SF_OK;
    }

    sf_memzero(&def, sizeof(def));
    def.size = sizeof(struct directory);
    def.free = directory_close;
    sf_list_init(&fs.directories, &def);

    if (argv[0][0] == '/') {
        strncpy(buf, argv[0], PATH_MAX);
    } else {
        len = PATH_MAX;
        getcwd(buf, len);
        len = strlen(buf);
        buf[len++] = seperator;
        buf[len] = '\0';
        strncat(buf, argv[0], PATH_MAX - len);
    }
    ptr = strrchr(buf, seperator);
    assert(ptr != NULL);
    *ptr = '\0';

    if (fs_cd(buf) != SF_OK) {
        sf_list_destroy(&fs.directories);
        return SF_ERR;
    }

    fs.isinited = 1;
    return SF_OK;
}

void fs_term(void)
{
    sf_list_destroy(&fs.directories);
}

int fs_cwd(char *buf, size_t count)
{
    return get_directory_pathname(sf_list_tail(&fs.directories), buf, count);
}

static int fs_cd_file(const char *filename)
{
    struct directory d;

    if (strcmp(filename, "..") == 0) {
        if (sf_list_cnt(&fs.directories)) {
            struct directory *d = sf_list_tail(&fs.directories);
            if (!d->iszip) {
                chdir("..");
            }
            sf_list_pop(&fs.directories);
        } else {
            chdir("..");
        }
    } else {
        if (sf_list_cnt(&fs.directories)) {
            struct directory *d = sf_list_tail(&fs.directories);
            if (d->iszip) {
                struct directory dir;
                strncpy(dir.name, filename, NAME_MAX);
                dir.iszip = 1;
                dir.isopened = 1;
                dir.zip = d->zip;
                sf_list_push(&fs.directories, &dir);
                return SF_OK;
            }
        }
        if (directory_init(&d, filename) != SF_OK) {
            return SF_ERR;
        }
        sf_list_push(&fs.directories, &d);
        chdir(filename);
    }

    return SF_OK;
}

int fs_cd(const char *pathname)
{
    if (pathname[0] == '/') {
    /* absolute path */
        sf_list_clear(&fs.directories);
        chdir("/");
        if (pathname[1] != '\0') {
            fs_cd(pathname + 1);
        }
    } else {
    /* relative path */
        char buf[PATH_MAX];
        char *ptr;
        char delim[2];

        strncpy(buf, pathname, PATH_MAX);

        delim[0] = seperator;
        delim[1] = '\0';

        ptr = strtok(buf, delim);
        while (ptr) {
            if (strcmp(ptr, ".") != 0) {
                if (fs_cd_file(ptr) != SF_OK) {
                    break;
                }
            }
            ptr = strtok(NULL, delim);
        }
    }
    return SF_OK;
}

static int fs_file_walker_dir(struct directory *d,
                              int (*func)(int type, const char *filename,
                                          void *arg),
                              void *arg)
{
    int ret = SF_OK;
    struct dirent *dirp;

    while ((dirp = readdir(d->dir)) != NULL) {
        int type;
        struct stat statbuf;

        if (strcmp(dirp->d_name, ".") == 0
            || strcmp(dirp->d_name, "..") == 0) {
            continue;
        }

        stat(dirp->d_name, &statbuf);
        if (S_ISDIR(statbuf.st_mode)) {
            type = FS_DIR;
        } else {
            type = FS_FILE;
        }

        if ((ret = func(type, dirp->d_name, arg)) != SF_OK) {
            break;
        }
    }

    rewinddir(d->dir);

    return ret;
}

static int get_zip_directory_pathname(char *buf, size_t count)
{
    sf_list_iter_t iter;
    struct directory *zip_root;
    struct directory *ptr;
    int iszip = 0;

    if (sf_list_begin(&fs.directories, &iter)) do {
        zip_root = sf_list_iter_elt(&iter);
        if (zip_root->iszip) {
            iszip = 1;
            break;
        }
    } while (sf_list_iter_next(&iter));

    buf[0] = '\0';
    if (iszip) {
        while (sf_list_iter_next(&iter)) {
            int len;
            ptr = sf_list_iter_elt(&iter);

            len = strlen(ptr->name) + 1;
            if (snprintf(buf, count, "%s%c", ptr->name, seperator) != len) {
                return SF_ERR;
            }
            buf += len;
            count -= len;
        }
    }

    return SF_OK;
}

static int fs_file_walker_zip(struct directory *d,
                              int (*func)(int type, const char *filename,
                                          void *arg),
                              void *arg)
{
    char buf[PATH_MAX];
    int len;
    int ret = SF_OK;
    zip_int64_t nentries = zip_get_num_entries(d->zip, 0);
    zip_int64_t i;

    get_zip_directory_pathname(buf, PATH_MAX);
    len = strlen(buf);

    for (i = 0; i < nentries; ++i) {
        const char *filename = zip_get_name(d->zip, i, 0);
        if (len == 0) {
            if (strchr(filename, seperator) == 0) {
                if ((ret = func(FS_FILE, filename, arg)) != SF_OK) {
                    break;
                }
            }
        } else {
            int isindir = 1;
            int i;

            for (i = 0; i < len; ++i) {
                if (buf[i] != filename[i]) {
                    isindir = 0;
                    break;
                }
            }
            if (isindir) {
                if ((ret = func(FS_FILE, filename + len, arg)) != SF_OK) {
                    break;
                }
            }
        }
    }
    return ret;
}

int fs_file_walker(int (*func)(int type, const char *filename, void *arg),
                   void *arg)
{
    int ret = SF_OK;
    struct directory *d;

    d = sf_list_tail(&fs.directories);
    if (!d->isopened) {
        if ((ret = directory_open(d)) != SF_OK) {
            return ret;
        }
    }

    if (!d->iszip) {
        return fs_file_walker_dir(d, func, arg);
    } else {
        return fs_file_walker_zip(d, func, arg);
    }
}
