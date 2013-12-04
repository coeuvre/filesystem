#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>

#include <sf/utils.h>

#include "filesystem.h"


static int print_file_name(int type, const char *filename, void *arg)
{
    if (type == FS_FILE) {
        fprintf(stdout, "(%s) ", filename);
    } else {
        fprintf(stdout, "[%s] ", filename);
    }

    return SF_OK;
}

static void pwd(void)
{
    char buf[PATH_MAX];

    fs_cwd(buf, PATH_MAX);
    fprintf(stdout, "fs_cwd: %s\n", buf);

    getcwd(buf, PATH_MAX);
    fprintf(stdout, "getcwd: %s\n", buf);
}

static void cat(const char *filename)
{
    char *buf;
    size_t len;
    struct fs_file f;

    if (fs_file_open(&f, filename, "r") != SF_OK) {
        fprintf(stdout, "failed to open %s\n", filename);
        return;
    }

    len = fs_file_size(filename);
    buf = sf_alloc(len + 1);
    fs_file_read(&f, buf, len);
    buf[len] = '\0';
    fprintf(stdout, "%s", buf);
    sf_free(buf);
    fs_file_close(&f);
}

static void touch(const char *filename)
{
    time_t rawtime;
    struct tm *timeinfo;
    char buf[1024];
    struct fs_file f;

    if (fs_file_open(&f, filename, "w+") != SF_OK) {
        fprintf(stdout, "failed to open %s\n", filename);
        return;
    }

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    snprintf(buf, 1024, "%s", asctime(timeinfo));
    fs_file_write(&f, buf, strlen(buf));

    fs_file_close(&f);
}

int main(int argc, char *argv[])
{
    char buf[PATH_MAX];

    atexit((void (*)(void)) sf_memcheck);

    fs_init(argc, argv);

    pwd();

    while ((fprintf(stdout, "$ "), fgets(buf, PATH_MAX, stdin))) {
        int len = strlen(buf);
        /* trim the last `\n` */
        buf[--len] = '\0';

        if (strncmp("cd ", buf, 3) == 0) {
            fs_cd(buf + 3);
        } else if (strncmp("pwd", buf, 3) == 0) {
            pwd();
        } else if (strncmp("ls", buf, 2) == 0) {
            fs_file_walker(print_file_name, NULL);
            fprintf(stdout, "\n");
         } else if (strncmp("cat ", buf, 4) == 0) {
             cat(buf + 4);
         } else if (strncmp("touch ", buf, 6) == 0) {
             touch(buf + 6);
         } else {
            fprintf(stdout, "Unkown command: %s\n", buf);
        }
    }

    fs_term();

    return 0;
}
