#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <linux/limits.h>

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

    fs_file_walker(print_file_name, NULL);
    fprintf(stdout, "\n");
}

int main(int argc, char *argv[])
{
    char buf[PATH_MAX];

    atexit((void (*)(void)) sf_memcheck);

    fs_init(argc, argv);

    pwd();

    while ((fprintf(stdout, "CD: "), fgets(buf, PATH_MAX, stdin))) {
        int len = strlen(buf);
        /* trim the last `\n` */
        buf[--len] = '\0';
        fs_cd(buf);

        pwd();
    }

    fs_term();

    return 0;
}
