
/* ================================================================================================
 * -*- C -*-
 * File: mtf.h
 * Created on: 27/08/15
 * Brief: Functions to decompress DarkStone MTF game archives.
 *
 * Source code licensed under the MIT license.
 * Copyright (C) 2015 Guilherme R. Lampert
 *
 * This software is provided "as is" without express or implied
 * warranties. You may freely copy and compile this source into
 * applications you distribute provided that this copyright text
 * is included in the resulting source code.
 * ================================================================================================ */

#ifndef DARKSTONE_MTF_H
#define DARKSTONE_MTF_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* ========================================================
 * DarkStone MTF game archive structures:
 * ======================================================== */

typedef struct mtf_compressed_header {
	uint8_t  magic1;                   // (Apparently) always 0xAE (174) or 0xAF (175) for a compressed file.
	uint8_t  magic2;                   // (Apparently) always 0xBE (190) for a compressed file.
	uint16_t unknown;                  // Unknown data. Seems to repeat a lot. We can decompressed without it anyways.
	uint32_t compressedSize;           // Advertised compressed size in byte of the entry.
	uint32_t decompressedSize;         // Decompressed size from `mtf_file_entry_t` is repeated here.
} mtf_compressed_header_t;

typedef struct mtf_file_entry {
	char   * filename;                 // Allocated in the heap; Read from file. Null terminated.
	uint32_t filenameLength;           // Filename length, including null terminator.
	uint32_t dataOffset;               // Absolute MTF archive offset to this file entry.
	uint32_t decompressedSize;         // Decompressed size in bytes of the file.
} mtf_file_entry_t;

typedef struct mtf_file {
	FILE             * osFileHandle;   // MTF file handle returned by fopen().
	mtf_file_entry_t * fileEntries;    // Sorted alphabetically by filename.
	uint32_t           fileEntryCount; // Size of fileEntries[].
} mtf_file_t;

enum {
	MTF_EXTRACT_ALL  = -1,
	MTF_MAX_PATH_LEN = 1024
};

/* ========================================================
 * Decompression functions:
 * ======================================================== */

/*
 * Opens a DarkStone MTF archive for reading.
 * It is safe to call mtf_file_close() even if this function fails.
 */
bool mtf_file_open(mtf_file_t * mtf, const char * filename);

/*
 * Closes an MTF archive previously opened by mtf_file_open().
 */
void mtf_file_close(mtf_file_t * mtf);

/*
 * Extract the contents of an MTF archive to normal files
 * in the local file system. Overwrites existing files.
 * The internal directory structure of the MTF is preserved.
 *
 * You may specify a maximum number of files to extract or
 * pass MTF_EXTRACT_ALL to `maxFileToExtract` and allow the
 * extraction of all files in the archive. `filesExtracted`
 * is optional and may be null. If provided, it will output
 * the number of files successfully extracted.
 */
bool mtf_file_extract_batch(const char * srcMtfFile, const char * destPath,
                            int maxFileToExtract, int * filesExtracted);

/*
 * All the above functions will set a global string with
 * an error description if something goes wrong. You can
 * recover the error description by calling this function
 * after a failure happens.
 *
 * Calling this function will clear the internal error string.
 */
const char * mtf_get_last_error(void);

#endif // DARKSTONE_MTF_H
