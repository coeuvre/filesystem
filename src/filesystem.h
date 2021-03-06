#ifndef FILESYSTEM_H
#define FILESYSTEM_H


#ifdef __WIN32__
#include <stdlib.h>
#define NAME_MAX 256
#else
#include <linux/limits.h>
#endif

enum FS_FILE_TYPE {
    FS_DIR,
    FS_FILE,
};

struct fs_file {
    int             isinzip;

    union {
        struct zip_file *zip_file;
        FILE            *file;
    };
};

#if 0
struct fs_delegate {
    int (*process)(const char *pathname);
    int (*cd)(const char *pathname);
    int (*cwd)(char *buf, size_t count);
    int (*open)(const char *filename);
    int (*close)(int fd);
    ssize_t (*write)(int fd, const void *buf, size_t count);
    ssize_t (*read)(int fd, void *buf, size_t count);
};
#endif

int fs_init(int argc, char **argv);

void fs_term(void);

/* default root is `current working directory` */
//void fs_set_root(const char *root);

int fs_cd(const char *pathname);

int fs_cwd(char *buf, size_t count);

int fs_file_walker(int (*func)(int type, const char *filename, void *arg),
                   void *arg);

int fs_file_open(struct fs_file *f, const char *filename, const char *mode);

int fs_file_close(struct fs_file *f);

ssize_t fs_file_write(struct fs_file *f, const void *buf, size_t count);

ssize_t fs_file_read(const struct fs_file *f, void *buf, size_t count);

size_t fs_file_size(const char *filename);


#endif /* FILESYSTEM_H */
