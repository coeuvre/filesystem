#ifndef FILESYSTEM_H
#define FILESYSTEM_H


enum FS_FILE_TYPE {
    FS_DIR,
    FS_FILE,
};

int fs_init(int argc, char **argv);

void fs_term(void);

/* default root is `current working directory` */
//void fs_set_root(const char *root);

int fs_cd(const char *pathname);

int fs_cwd(char *buf, size_t count);

int fs_file_walker(int (*func)(int type, const char *filename, void *arg),
                   void *arg);

int fs_open(const char *filename);

int fs_close(int fd);

ssize_t fs_write(int fd, const void *buf, size_t count);

ssize_t fs_read(int fd, void *buf, size_t count);



#endif /* FILESYSTEM_H */
