
/* ================================================================================================
 * -*- C -*-
 * File: mtf_unpacker.c
 * Created on: 27/08/15
 * Brief: Very simple command-line tool to unpack a DarkStone MTF archive.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char * progName) {
	printf(
		"\n"
		"Usage:\n"
		"$ %s <input_mtf> <output_dir>\n"
		"  Decompresses each file in the given MTF archive to the provided path.\n"
		"  Creates directories as needed. Existing files are overwritten.\n"
		"\n"
		"Usage:\n"
		"$ %s --help | -h\n"
		"  Prints this help text.\n"
		"\n",
	progName, progName);
}

int main(int argc, const char * argv[]) {
	if (argc < 2) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	// Printing help is not treated and an error.
	if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
		print_usage(argv[0]);
		return EXIT_SUCCESS;
	}

	// From here on we need an input filename and an output path.
	if (argc < 3) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	const char * mtfFilename = argv[1];
	const char * outputDir   = argv[2];

	int filesExtracted = 0;
	bool success = mtf_file_extract_batch(mtfFilename, outputDir, MTF_EXTRACT_ALL, &filesExtracted);

	if (success) {
		printf("Successfully extracted %d files from MTF archive \"%s\".\n", filesExtracted, mtfFilename);
		return EXIT_SUCCESS;
	} else {
		fprintf(stderr, "Error while extracting \"%s\": %s\n", mtfFilename, mtf_get_last_error());
		fprintf(stderr, "Managed to extract %d files.\n", filesExtracted);
		return EXIT_FAILURE;
	}
}
