/*
 * Dokan Image Mounter - Main compilation unit
 */

#pragma comment(lib, "dokan.lib")

#define WIN32_LEAN_AND_MEAN

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <dokan.h>

#include "src/backend.h"
#include "src/utils.c"

#include "src/fs_fat.c"
#include "src/pt_nec.c"
#include "src/c_hdi.c"

const FSFORMAT *FSFormats[] = {
	&FS_FAT,
	NULL
};
const PTFORMAT *PTFormats[] = {
	&PT_NEC,
	NULL
};
const CFORMAT *CFormats[] = {
	&C_HDI,
	NULL
};

#include "src/backend.c"
#include "src/frontend.c"
