/* flac-fs - convert flac files to mp3s on the fly
 *
 * Copyright (C) 2014 Wesley Merkel <ooesili@gmail.com>
 *
 * This file is part of flac-fs
 *
 * flac-fs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * flac-fs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with flac-fs.  If not, see <http://www.gnu.org/licenses/>.
 */

#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif

#include <stdlib.h>
#include "flac.h"
#include <err.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

static const char *flacfs_root;
static const size_t flacfs_root_len = 19;

static int file_exists(const char* path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* make sure the free the returned pointer */
static char *replace_ext(const char *path, const char* ext) {
    const char *ptr;
    char *newstr;
    size_t dot_len;

    /* find the last dot and move past it */
    ptr = strrchr(path, '.') + 1;
    if (ptr == NULL) { return NULL; }
    dot_len = ptr - path;
    /* create enough room for new string */
    newstr = calloc(dot_len + strlen(ext) + 1, sizeof(char));
    /* copy entire old string */
    strncpy(newstr, path, dot_len);
    /* replace extension */
    strcpy(newstr + dot_len, ext);

    return newstr;
}

static int ends_with(const char *path, const char *end)
{
    const char *ptr = path;
    size_t len = strlen(path);
    int code = 0;
    if (len > 5) {
        ptr += len - strlen(end);
        if (strcmp(ptr, end) == 0) { code = 1; }
    }
    return code;
}

/* make sure the free the returned pointer */
static char *prepend_root(const char *path)
{
    char *full_path;

    /* prepend flacfs_root to path */
    full_path = calloc(strlen(path) + flacfs_root_len + 1, sizeof(char));
    strcpy(full_path, flacfs_root);
    strcat(full_path, path);

    return full_path;
}

static char *unhide_flac(const char *path)
{
    char *flac_path = NULL;
    /* see if files is mp3 and doesn't exist */
    if (ends_with(path, ".mp3") && !file_exists(path)) {
        flac_path = replace_ext(path, "flac");
        /* return null if the flac file doesn't exist, otherwise,
         * leave flac_path intact */
        if (!file_exists(flac_path)) {
            free(flac_path);
            flac_path = NULL;
        }
    }
    return flac_path;
}

static int flacfs_getattr(const char *path, struct stat *stbuf)
{
    int res;
    char *full_path, *flac_path;

    full_path = prepend_root(path);
    /* see if there is a hidden flac file */
    flac_path = unhide_flac(full_path);
    if (flac_path != NULL) {
        free(full_path);
        full_path = flac_path;
    }

    res = lstat(full_path, stbuf);
    if (res == -1)
        return -errno;

    /* modify permissions */
    if      (S_ISDIR(stbuf->st_mode)) { stbuf->st_mode &= 0040755; }
    else if (S_ISREG(stbuf->st_mode)) { stbuf->st_mode &= 0100644; }
    else                              { stbuf->st_mode &= 0170000; }

    free(full_path);
    return 0;
}

static int flacfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
               off_t offset, struct fuse_file_info *fi)
{
    DIR *dp;
    struct dirent *de;
    char *full_path;

    full_path = prepend_root(path);

    dp = opendir(full_path);
    if (dp == NULL)
        return -errno;

    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        char *name;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        name = de->d_name;
        if (S_ISREG(st.st_mode) && ends_with(name, ".flac")) {
            name = replace_ext(name, "mp3");
        }
        if (filler(buf, name, &st, 0))
            break;
    }

    closedir(dp);
    free(full_path);

    return 0;
}

static int flacfs_open(const char *path, struct fuse_file_info *fi)
{
    int res, code = 0;
    char *full_path, *flac_path;

    full_path = prepend_root(path);
    /* see if there is a hidden flac file */
    flac_path = unhide_flac(full_path);
    if (flac_path != NULL) {
        int fd;
        /* replace full_path */
        free(full_path);
        full_path = flac_path;
        /* get the fild descriptor of the recoded file */
        fd = flac_to_mp3(full_path);
        if (fd == -1)
            code = -errno;
        else
            fi->fh = fd;
        /* clean up paths */
        free(full_path);
    }
    /* not a flac file, open as mormal */
    else {
        res = open(full_path, fi->flags);
        if (res == -1)
            code = -errno;
        else
            fi->fh = res;
    }

    return code;
}

int flacfs_release (const char *path, struct fuse_file_info *fi)
{
    return close(fi->fh);
}


static int flacfs_read(const char *path, char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi)
{
    int res, code;

    res = pread(fi->fh, buf, size, offset);

    if (res == -1)
        code = -errno;
    else
        code = res;

    return code;
}

static struct fuse_operations flacfs_oper = {
    .getattr	= flacfs_getattr,
    .readdir	= flacfs_readdir,
    .open	= flacfs_open,
    .read	= flacfs_read,
    .release	= flacfs_release,
};

int main(int argc, char *argv[])
{
    struct stat st;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <source> <dest> [options]\n", argv[0]);
        exit(1);
    }
    /* see if file exists and is a directory */
    if (stat(argv[1], &st) != 0 || !S_ISDIR(st.st_mode))
        errx(2, "`%s' is not a directory", argv[1]);

    /* start fuse */
    flacfs_root = argv[1];
    umask(0);
    return fuse_main(argc-1, argv+1, &flacfs_oper, NULL);
}
