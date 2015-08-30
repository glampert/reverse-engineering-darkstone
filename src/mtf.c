
/* ================================================================================================
 * -*- C -*-
 * File: mtf.c
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

#include "mtf.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

/* ========================================================
 * mtf_get_last_error()/mtf_error():
 * ======================================================== */

// Some compilers have Thread Local Storage support
// which can be used to make this error string thread
// safe, so that parallel file processing wouldn't step
// in each other's toes when reporting errors...
static const char * mtfLastErrorStr = "";

static inline bool mtf_error(const char * message) {
	mtfLastErrorStr = (message != NULL) ? message : "";
	// Always returns false to allow using "return mtf_error(...);"
	return false;
}

const char * mtf_get_last_error(void) {
	const char * err = mtfLastErrorStr;
	mtfLastErrorStr = "";
	return err;
}

/* ========================================================
 * Path/directory helpers:
 * ======================================================== */

static inline bool mtf_is_ascii(int ch) {
	return (ch >= 0) && (ch < 128);
}

static void mtf_fix_filepath(char * pathInOut) {
	assert(pathInOut != NULL);
	//
	// DarkStone used Windows-style paths, with
	// backslashes as directory separator.
	//
	// Also, there are a couple filenames in some
	// archives that use extended ASCII characters,
	// like accentuations (é, á, ç, etc), which don't
	// play very well on the Mac file system. So I'm
	// replacing such occurrences with a question mark '?'.
	//
	char * p = pathInOut;
	while (*p != '\0') {
		if (*p == '\\') {
			*p = '/';
		} else if (!mtf_is_ascii(*p)) {
			*p = '?';
		}
		++p;
	}
}

static bool mtf_make_directory(const char * dirPath) {
	assert(dirPath != NULL);

	// NOTE: stat/mkdir are defined differently on Windows,
	// so this will need a fix when porting this to Win/VS.
	//
	struct stat dirStat;
	if (stat(dirPath, &dirStat) != 0) {
		if (mkdir(dirPath, 0777) != 0) {
			return mtf_error("Impossible to create directory! mkdir(0777) failed.");
		}
	} else {
		// Path already exists.
		if (!S_ISDIR(dirStat.st_mode)) {
			// Looks like there is a file with the same name as the directory.
			return mtf_error("Can't mkdir()! Path points to a file.");
		}
	}

	return true;
}

static bool mtf_make_path(const char * pathEndedWithSeparatorOrFilename) {
	assert(pathEndedWithSeparatorOrFilename != NULL);
	assert(strlen(pathEndedWithSeparatorOrFilename) < MTF_MAX_PATH_LEN);

	char dirPath[MTF_MAX_PATH_LEN];
	strncpy(dirPath, pathEndedWithSeparatorOrFilename, MTF_MAX_PATH_LEN);

	char * pPath = dirPath;
	while (*pPath != '\0') {
		if (*pPath == '/' || *pPath == '\\') {
			*pPath = '\0';
			if (!mtf_make_directory(dirPath)) {
				return false;
			}
			*pPath = '/';
		}
		++pPath;
	}

	return true;
}

/* ========================================================
 * mtf_readX():
 * ======================================================== */

static inline bool mtf_read32(FILE * fileIn, uint32_t * dword) {
	if (fread(dword, sizeof(*dword), 1, fileIn) != 1) {
		return mtf_error("mtf_read32() failed!");
	}
	return true;
}

static inline bool mtf_read16(FILE * fileIn, uint16_t * word) {
	if (fread(word, sizeof(*word), 1, fileIn) != 1) {
		return mtf_error("mtf_read16() failed!");
	}
	return true;
}

static inline bool mtf_read8(FILE * fileIn, uint8_t * byte) {
	int ch = fgetc(fileIn);
	if (ch == EOF) {
		return mtf_error("mtf_read8() failed!");
	}
	*byte = (uint8_t)ch;
	return true;
}

/* ========================================================
 * mtf_read_compressed_header():
 * ======================================================== */

static inline bool mtf_read_compressed_header(FILE * fileIn, uint32_t offset,
                                              mtf_compressed_header_t * header) {
	assert(fileIn != NULL);
	assert(header != NULL);

	fseek(fileIn, offset, SEEK_SET);
	fread(header, sizeof(*header), 1, fileIn);

	return !ferror(fileIn);
}

/* ========================================================
 * mtf_is_compressed():
 * ======================================================== */

static inline bool mtf_is_compressed(const mtf_compressed_header_t * header) {
	//
	// These magic numbers are from Xentax Wiki:
	//  http://wiki.xentax.com/index.php?title=Darkstone
	//
	if (header->magic1 == 0xAE && header->magic2 == 0xBE) {
		return true;
	}
	if (header->magic1 == 0xAF && header->magic2 == 0xBE) {
		return true;
	}
	return false;
}

/* ========================================================
 * mtf_decompress_write_file():
 * ======================================================== */

static bool mtf_decompress_write_file(FILE * fileIn, FILE * fileOut, uint32_t decompressedSize,
                                      const mtf_compressed_header_t * compressedHeader) {

	// NOTE: `fileIn` must to point past the compressed header!
	assert(fileIn  != NULL);
	assert(fileOut != NULL);
	assert(compressedHeader != NULL);
	assert(decompressedSize != 0);

	// Would be better as a compile-time assert. I'm just being lazy...
	assert(sizeof(mtf_compressed_header_t) == 12 && "Unexpected size for this struct!");

	uint8_t * decompressBuffer = malloc(decompressedSize);
	uint8_t * decompressedPtr  = decompressBuffer;

	if (decompressBuffer == NULL) {
		return mtf_error("Failed to malloc decompression buffer!");
	}

	bool hadError = false;
	int bytesRead = sizeof(mtf_compressed_header_t);
	int bytesLeft = decompressedSize;

	// Do one byte at a time. Repeat until we have processed
	// the advertised decompressed size in bytes.
	while (bytesLeft) {

		// Each compressed block/chunk is prefixed by a one byte header.
		// Each bit in this chunk tells us how to handle the next byte
		// read from the file.
		uint8_t chunkBits;
		if (!mtf_read8(fileIn, &chunkBits)) {
			hadError = true;
			goto BAIL;
		}
		++bytesRead;

		// For each bit in the chunk header, staring from
		// the lower/right-hand bit (little endian)
		for (int b = 0; b < 8; ++b) {
			int flag = chunkBits & (1 << b);

			// If the bit is set, read the next byte unchanged:
			if (flag) {
				uint8_t byte;
				if (!mtf_read8(fileIn, &byte)) {
					hadError = true;
					goto BAIL;
				}

				*decompressedPtr++ = byte;
				++bytesRead;
				--bytesLeft;
			} else {

				// If the flag bit is zero, the next two bytes indicate
				// the offset and byte count to replicate from what was
				// already read. This seems somewhat similar to RLE compression...
				uint16_t word;
				if (!mtf_read16(fileIn, &word)) {
					hadError = true;
					goto BAIL;
				}

				bytesRead += 2;

				if (word == 0) {
					// Looks like a few entries have padding or something.
					// When we get here, bytesLeft is already zero, so this seems benign...
					// Q: Is the padding to align the buffers to a given boundary?
					break;
				}

				int count  = (word >> 10);    // Top 6 bits of the word
				int offset = (word & 0x03FF); // Lower 10 bits of the word

				// Copy count+3 bytes staring at offset to the end of the decompression buffer,
				// as explained here: http://wiki.xentax.com/index.php?title=Darkstone
				for (int n = 0; n < count + 3; ++n) {
					*decompressedPtr = *(decompressedPtr - offset);
					++decompressedPtr;
					--bytesLeft;
				}

				if (bytesLeft < 0) {
					mtf_error("Compressed/decompressed size mismatch!");
					hadError = true;
					goto BAIL;
				}
			}
		}
	}

BAIL:

	if (!hadError) {
		if (fwrite(decompressBuffer, 1, decompressedSize, fileOut) != decompressedSize) {
			mtf_error("Failed to write decompressed file data!");
			hadError = true;
		}
	}

	free(decompressBuffer);
	return !hadError;
}

/* ========================================================
 * mtf_write_file():
 * ======================================================== */

static bool mtf_write_file(FILE * fileIn, FILE * fileOut,
                           uint32_t sizeInBytes, uint32_t readOffset) {

	assert(fileIn  != NULL);
	assert(fileOut != NULL);

	void * readBuffer = malloc(sizeInBytes);
	if (readBuffer == NULL) {
		return mtf_error("mtf_write_file(): Failed to malloc buffer!");
	}

	if (fseek(fileIn, readOffset, SEEK_SET) != 0) {
		free(readBuffer);
		return mtf_error("mtf_write_file(): Can't fseek() entry offset!");
	}

	if (fread(readBuffer, 1, sizeInBytes, fileIn) != sizeInBytes) {
		free(readBuffer);
		return mtf_error("mtf_write_file(): Can't read source file entry!");
	}

	if (fwrite(readBuffer, 1, sizeInBytes, fileOut) != sizeInBytes) {
		free(readBuffer);
		return mtf_error("mtf_write_file(): Can't write dest file!");
	}

	free(readBuffer);
	return true;
}

/* ========================================================
 * mtf_sort_by_filename() => qsort() predicate:
 * ======================================================== */

static int mtf_sort_by_filename(const void * a, const void * b) {

	return strcmp(((const mtf_file_entry_t *)a)->filename,
	              ((const mtf_file_entry_t *)b)->filename);
}

/* ========================================================
 * mtf_file_open():
 * ======================================================== */

bool mtf_file_open(mtf_file_t * mtf, const char * filename) {

	assert(mtf != NULL);
	assert(filename != NULL && *filename != '\0');

	mtf->osFileHandle   = fopen(filename, "rb");
	mtf->fileEntries    = NULL;
	mtf->fileEntryCount = 0;

	if (mtf->osFileHandle == NULL) {
		return mtf_error("Can't open input MTF file!");
	}

	// First 4 bytes are the number of files in the MTF archive.
	if (!mtf_read32(mtf->osFileHandle, &mtf->fileEntryCount)) {
		mtf_file_close(mtf);
		return mtf_error("Failed to read file entry count.");
	}

	if (mtf->fileEntryCount == 0) {
		mtf_file_close(mtf);
		return mtf_error("MTF appears to have no file! fileEntryCount == 0.");
	}

	mtf->fileEntries = calloc(mtf->fileEntryCount, sizeof(mtf->fileEntries[0]));
	if (mtf->fileEntries == NULL) {
		mtf_file_close(mtf);
		return mtf_error("Failed to malloc MTF file entries!");
	}

	// Read in the file entry list:
	for (uint32_t e = 0; e < mtf->fileEntryCount; ++e) {
		mtf_file_entry_t * entry = &mtf->fileEntries[e];

		if (!mtf_read32(mtf->osFileHandle, &entry->filenameLength)) {
			mtf_file_close(mtf);
			return mtf_error("file to read a filename length.");
		}

		// Strings stored in the file are supposedly already null terminated,
		// but it is better not to rely on that and alloc an extra byte, then set it to \0.
		entry->filename = malloc(entry->filenameLength + 1);
		if (entry->filename == NULL) {
			mtf_file_close(mtf);
			return mtf_error("Failed to malloc filename string!");
		}

		// Reading a string or not, we continue...
		fread(entry->filename, 1, entry->filenameLength, mtf->osFileHandle);
		entry->filename[entry->filenameLength] = '\0';

		// Data start offset and decompressed size in bytes (for this file entry):
		if (!mtf_read32(mtf->osFileHandle, &entry->dataOffset) ||
		    !mtf_read32(mtf->osFileHandle, &entry->decompressedSize)) {
			mtf_file_close(mtf);
			return mtf_error("Failed to read data offset or size.");
		}
	}

	// Entries are probably already in sorted order, but since
	// we don't have a formal specification to ensure that,
	// sort them by filename now:
	qsort(mtf->fileEntries, mtf->fileEntryCount, sizeof(mtf->fileEntries[0]), &mtf_sort_by_filename);
	return true;
}

/* ========================================================
 * mtf_file_close():
 * ======================================================== */

void mtf_file_close(mtf_file_t * mtf) {
	if (mtf == NULL) {
		return; // Can be called even for an invalid file/pointer.
	}

	if (mtf->osFileHandle != NULL) {
		fclose(mtf->osFileHandle);
		mtf->osFileHandle = NULL;
	}

	if (mtf->fileEntries != NULL) {
		for (uint32_t e = 0; e < mtf->fileEntryCount; ++e) {
			free(mtf->fileEntries[e].filename);
		}

		free(mtf->fileEntries);
		mtf->fileEntries    = NULL;
		mtf->fileEntryCount = 0;
	}
}

/* ========================================================
 * mtf_file_extract_batch():
 * ======================================================== */

bool mtf_file_extract_batch(const char * srcMtfFile, const char * destPath,
                            int maxFileToExtract, int * filesExtracted) {

	assert(srcMtfFile != NULL && *srcMtfFile != '\0');
	assert(destPath   != NULL && *destPath   != '\0');

	// `maxFileToExtract` can be zero, negative or MTF_EXTRACT_ALL to extract everything.
	// `filesExtracted` is optional and may be null.

	// Attempt to open and read the headers and file entry list:
	mtf_file_t mtf;
	if (!mtf_file_open(&mtf, srcMtfFile)) {
		return false;
	}

	// Data for the individual files follow.
	// Now read each entry, decompress and write the output files.
	char extractionPath[MTF_MAX_PATH_LEN];
	int successCount = 0;

	for (uint32_t e = 0; e < mtf.fileEntryCount; ++e) {
		const mtf_file_entry_t * entry = &mtf.fileEntries[e];

		// A compressed file is prefixed by a 12 bytes compression info
		// header. If uncompressed, then there is no header; Problem
		// is, we can only tell if the file is compressed after reading in
		// the 12 bytes of a header, so if it is not compressed, we have
		// to seek back 12 bytes and then read the whole uncompressed block.

		mtf_compressed_header_t compressedHeader;
		if (!mtf_read_compressed_header(mtf.osFileHandle, entry->dataOffset, &compressedHeader)) {
			mtf_file_close(&mtf);
			return mtf_error("Failed to read a compression info header!");
		}

		// Set up the output file path, replacing Windows backslashes by forward slashes:
		snprintf(extractionPath, MTF_MAX_PATH_LEN, "%s/%s", destPath, entry->filename);
		mtf_fix_filepath(extractionPath);

		// Output path might not exist yet. This has no side effects if it does.
		mtf_make_path(extractionPath);

		FILE * fileOut = fopen(extractionPath, "wb");
		if (fileOut == NULL) {
			mtf_file_close(&mtf);
			return mtf_error("Can't create output file on extraction path!");
		}

		bool success;
		if (mtf_is_compressed(&compressedHeader)) {
			// Pointing to the correct offset thanks to mtf_read_compressed_header().
			success = mtf_decompress_write_file(mtf.osFileHandle,
					fileOut, entry->decompressedSize, &compressedHeader);
		} else {
			success = mtf_write_file(mtf.osFileHandle,
					fileOut, entry->decompressedSize, entry->dataOffset);
		}

		fclose(fileOut);

		if (success) {
			++successCount;
			if (maxFileToExtract > 0 && successCount == maxFileToExtract) {
				break;
			}
		}
	}

	if (filesExtracted != NULL) {
		*filesExtracted = successCount;
	}

	mtf_file_close(&mtf);
	return true;
}
