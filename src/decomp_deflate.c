/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "log.h"
#include "frogfs/frogfs.h"
#include "frogfs/format.h"

#include "zlib.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>


#define BUFFER_LEN 16
#define STREAM(f) ((z_stream *)(f->decomp_priv))

static int open_deflate(frogfs_f_t *f, unsigned int flags)
{
    int ret;

    z_stream *stream = malloc(sizeof(z_stream));
    if (stream == NULL) {
        LOGE("malloc failed");
        return -1;
    }
    memset(stream, 0, sizeof(*stream));

    ret = inflateInit(stream);
    if (ret != Z_OK) {
        LOGE("error allocating deflate stream");
        return -1;
    }

    f->decomp_priv = stream;
    return 0;
}

static void close_deflate(frogfs_f_t *f)
{
    z_stream *stream = STREAM(f);
    inflateEnd(stream);
    free(stream);
    f->decomp_priv = NULL;
}

static ssize_t read_deflate(frogfs_f_t *f, void *buf, size_t len)
{
    const frogfs_file_comp_t *file = (const frogfs_file_comp_t *) f->file;
    size_t start_in, start_out;
    int ret;

    if (STREAM(f)->total_out == file->uncompressed_len) {
        return 0;
    }

    start_in = STREAM(f)->total_in;
    start_out = STREAM(f)->total_out;

    while (STREAM(f)->total_in < file->data_len &&
            STREAM(f)->total_out - start_out < len) {
        STREAM(f)->next_in = f->data_ptr;
        STREAM(f)->avail_in = file->uncompressed_len - \
                (f->data_ptr - f->data_start);
        STREAM(f)->next_out = buf;
        STREAM(f)->avail_out = len;

        ret = inflate(STREAM(f), Z_NO_FLUSH);
        if (ret < 0) {
            LOGE("inflate");
            return -1;
        }
        f->data_ptr += STREAM(f)->total_in - start_in;
        if (ret == Z_STREAM_END) {
            break;
        }
    }

    return STREAM(f)->total_out - start_out;
}

static ssize_t seek_deflate(frogfs_f_t *f, long offset, int mode)
{
    const frogfs_file_comp_t *file = (const frogfs_file_comp_t *) f->file;
    ssize_t new_pos = STREAM(f)->total_out;

    if (mode == SEEK_SET) {
        if (offset < 0) {
            return -1;
        }
        if (offset > file->uncompressed_len) {
            offset = file->uncompressed_len;
        }
        new_pos = offset;
    } else if (mode == SEEK_CUR) {
        if (new_pos + offset < 0) {
            new_pos = 0;
        } else if (new_pos > file->uncompressed_len) {
            new_pos = file->uncompressed_len;
        } else {
            new_pos += offset;
        }
    } else if (mode == SEEK_END) {
        if (offset > 0) {
            return -1;
        }
        if (offset < -(ssize_t) file->uncompressed_len) {
            offset = 0;
        }
        new_pos = file->uncompressed_len + offset;
    } else {
        return -1;
    }

    if (STREAM(f)->total_out > new_pos) {
        f->data_ptr = f->data_start;
        inflateReset(STREAM(f));
    }

    while (STREAM(f)->total_out < new_pos) {
        uint8_t buf[BUFFER_LEN];
        size_t len = new_pos - STREAM(f)->total_out < BUFFER_LEN ?
                new_pos - STREAM(f)->total_out : BUFFER_LEN;

        ssize_t res = frogfs_read(f, buf, len);
        if (res < 0) {
            LOGE("frogfs_fread");
            return -1;
        }
    }

    return STREAM(f)->total_out;
}

static size_t tell_deflate(frogfs_f_t *f)
{
    return STREAM(f)->total_out;
}

const frogfs_decomp_funcs_t frogfs_decomp_deflate = {
    .open = open_deflate,
    .close = close_deflate,
    .read = read_deflate,
    .seek = seek_deflate,
    .tell = tell_deflate,
};