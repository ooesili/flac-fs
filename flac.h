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

typedef char *tag_t;

int flac_to_mp3(const char *path);

void get_all_tags(const char *path, tag_t tags[]);

void free_tags(tag_t tags[]);
