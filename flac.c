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

#include "flac.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <FLAC/metadata.h>

#define TAG_TITLE 0
#define TAG_ARTIST 1
#define TAG_ALBUM 2
#define TAG_DATE 3
#define TAG_TRACKNUMBER 4
#define TAG_TRACKTOTAL 5

/* takes the path of a flac file and return the file descriptor of the
 * re-encoded mp3 file */
int flac_to_mp3(const char *path)
{
    pid_t main_pid;
    FILE *temp_stream;
    int temp_fd;

    /* create temporary file */
    temp_stream = tmpfile();
    if (temp_stream == NULL)
        return -1;
    temp_fd = fileno(temp_stream);

    /* fork and recode */
    main_pid = fork();
    if (main_pid == -1) {
        fprintf(stderr, "cannot fork");
        return -1;
    }
    /* this is the child */
    else if (main_pid == 0) {
        pid_t pipe_pid;
        int pipefd[2];

        /* create pipe */
        pipe(pipefd);
        /* fork */
        pipe_pid = fork();
        if (main_pid == -1) {
            fprintf(stderr, "cannot fork");
            exit(EXIT_FAILURE);
        }
        /* decoder process */
        else if (pipe_pid == 0) {
            /* write stdout to pipe */
            dup2(pipefd[1], STDOUT_FILENO);
            /* run flac */
            execlp("flac", "flac", "-d", "-s", "-c", path, NULL);
        }
        /* encoder process */ 
        else {
            tag_t tags[6];
            char *track_num;
            /* read tags from file */
            get_all_tags(path, tags);
            /* 4 digits + slash + null byte + 2 just in case*/
            track_num = calloc(8, sizeof(char));
            sprintf(track_num, "%s/%s",
                    tags[TAG_TRACKNUMBER],
                    tags[TAG_TRACKTOTAL]);
            /* read from pipe, write to temp file */
            dup2(pipefd[0], STDIN_FILENO);
            dup2(temp_fd, STDOUT_FILENO);
            execlp("lame", "lame", "--silent", "-h", "-V", "0",
                    "--tt", tags[TAG_TITLE],
                    "--ta", tags[TAG_ARTIST],
                    "--tl", tags[TAG_ALBUM],
                    "--ty", tags[TAG_DATE],
                    "--tn", track_num,
                    "-", "-", NULL);
        }
    }
    /* this is the parent */
    else {
        wait(NULL);
    }
    return temp_fd;
}

/* returned pointed must be freed */
static char *find_tag_in_comment(const char *tag_name,
        FLAC__StreamMetadata_VorbisComment *vorbis_comment)
{
    int i;
    FLAC__StreamMetadata_VorbisComment_Entry *comments;
    char *value = NULL;
    size_t to_equals = strlen(tag_name);

    comments = vorbis_comment->comments;

    /* loop through comments */
    for (i = 0; i < vorbis_comment->num_comments; i++) {
        if (strncmp((char *)comments[i].entry, tag_name, to_equals) == 0) {
            /* move past `=' */
            char *ptr = (char *)comments[i].entry + to_equals + 1;
            value = calloc(strlen(ptr) + 1, sizeof(char));
            strcpy(value, ptr);
            break;
        }
    }
    return value;
}

/* read tags from file */
void get_all_tags(const char *path, tag_t tags[])
{
    FLAC__StreamMetadata *metaflac_tags;
    FLAC__StreamMetadata_VorbisComment vorbis_comment;

    /* grab metadata object */
    FLAC__metadata_get_tags(path, &metaflac_tags);
    vorbis_comment = metaflac_tags->data.vorbis_comment;

    /* fill each member of flac_tags struct */
    tags[TAG_TITLE]       = find_tag_in_comment("TITLE",       &vorbis_comment);
    tags[TAG_ARTIST]      = find_tag_in_comment("ARTIST",      &vorbis_comment);
    tags[TAG_ALBUM]       = find_tag_in_comment("ALBUM",       &vorbis_comment);
    tags[TAG_DATE]        = find_tag_in_comment("DATE",        &vorbis_comment);
    tags[TAG_TRACKNUMBER] = find_tag_in_comment("TRACKNUMBER", &vorbis_comment);
    tags[TAG_TRACKTOTAL]  = find_tag_in_comment("TRACKTOTAL",  &vorbis_comment);

    /* delete metadata object */
    FLAC__metadata_object_delete(metaflac_tags);
}

/* free an array of tags */
void free_tags(tag_t tags[])
{
    int i;
    for (i = 0; i < 6; i++) {
        free(tags[i]);
    }
}
