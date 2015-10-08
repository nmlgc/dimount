/*
 * Dokan Image Mounter - Main compilation unit
 */

#pragma comment(lib, "dokan.lib")

#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <dokan.h>

#define DOKAN_VERSION_REQUIRED 800
#define DOKAN_VERSION_REQUIRED_STR "v0.8.0"

#if DOKAN_VERSION < DOKAN_VERSION_REQUIRED
# error dimount requires at least Dokan v0.8.0. Please install the latest release from https://github.com/dokan-dev/dokany.
#elif DOKAN_VERSION > DOKAN_VERSION_REQUIRED
# define _STR(x) #x
# define STR(x) _STR(x)
# pragma message(\
	"warning: "\
	"dimount was written for Dokan "DOKAN_VERSION_REQUIRED_STR" "\
	"("STR(DOKAN_VERSION_REQUIRED)") and may not compile or run properly "\
	"with version "STR(DOKAN_VERSION)"."\
)
#endif

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
