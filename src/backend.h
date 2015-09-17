/*
 * Dokan Image Mounter - Backend
 */

/// Random stuff
/// ------------
#define elementsof(arr) (sizeof(arr) / sizeof(arr[0]))

typedef struct {
	UINT Cylinder;
	UINT Head;
	UINT Sector;
} CHS;
/// ------------

/// Format types
/// ------------
typedef struct CONTAINER CONTAINER;
typedef struct FILESYSTEM FILESYSTEM;

typedef struct FSFORMAT {
	// User-friendly name of the format.
	const wchar_t *Name;
	// Maximum filename length.
	const UINT FNLength;
	// Returns 0 if [FS] was successfully identified as a
	// file system of this format.
	int(*Probe)(FILESYSTEM *FS);
	// Returns [Total] and [Available] number of bytes on the file system.
	void(*DiskSizes)(FILESYSTEM *FS, uint64_t *Total, uint64_t *Available);
} FSFORMAT;

#define NEW_FSFORMAT(ID, _Name, _FNLength) \
	const FSFORMAT FS_##ID = { \
		.Name = _Name, \
		.FNLength = _FNLength, \
		.Probe = FS_##ID##_Probe, \
		.DiskSizes = FS_##ID##_DiskSizes \
	}

typedef struct PTFORMAT {
	// User-friendly name of the format.
	const wchar_t *Name;
	// Fills [Image]->Partitions with the start and end positions of all
	// partitions in the container and returns the number of partitions found.
	unsigned int(*Probe)(CONTAINER *Image);
} PTFORMAT;

#define NEW_PTFORMAT(ID, _Name) \
	const PTFORMAT PT_##ID = { \
		.Name = _Name, \
		.Probe = P_##ID##_Probe \
	}

typedef struct CFORMAT {
	// User-friendly name of the format.
	const wchar_t *Name;
	// Returns the offset to the beginning of the partition table past the
	// container format header, or -1 if [Image] doesn't use this container
	// format. Also fills [Image]->SHCSizes.
	uint64_t(*Probe)(CONTAINER *Image);
} CFORMAT;

#define NEW_CFORMAT(ID, _Name) \
	const CFORMAT C_##ID = { \
		.Name = _Name, \
		.Probe = C_##ID##_Probe \
	}
/// ------------

/// Probing
/// -------
// Returns 0 if a suitable file system has been found for [FS].
int ImageFSFormatProbe(FILESYSTEM *FS);
// Returns the number of partitions found in [Image], or -1 if no suitable
// partition table format was identified.
int ImagePTFormatProbe(CONTAINER *Image);
// Returns the container format identified for [Image], or NULL if no suitable
// format was identified.
const CFORMAT* ImageCFormatProbe(CONTAINER *Image);
/// -------

/// Instance types
/// --------------
typedef struct FILESYSTEM {
	CONTAINER *Image; // CONTAINER that contains this file system
	const FSFORMAT *FSFormat;
	UINT SectorSize;
	UINT CodePage;
	uint64_t Start;
	uint64_t End;
	uint32_t Serial;
	wchar_t Label[MAX_PATH];
} FILESYSTEM;

BOOL FSLabelSetA(FILESYSTEM* FS, const char *Label, size_t LabelLen);

typedef struct CONTAINER {
	UINT CodePage;
	uint64_t Sector0Offset;
	uint64_t CSize;
	const CFORMAT *CFormat;
	const PTFORMAT *PTFormat;
	CHS CHSSizes;
	uint8_t *View;
	FILESYSTEM Partitions[16];
} CONTAINER;
/// --------------

/// Addressing
/// ----------
uint8_t* FSAt(FILESYSTEM *FS, uint64_t Pos, UINT Size);

#define FSStructAt(Type, FS, Pos) \
	(Type*)FSAt(FS, (Pos), sizeof(Type))

uint8_t* CAt(CONTAINER *Image, uint64_t Pos, UINT Size);
uint8_t* CAtCHS(CONTAINER *Image, CHS *Pos, UINT Size);

#define CStructAt(Type, Image, Pos) \
	(Type*)CAt(Image, (Pos), sizeof(Type))

#define CStructAtCHS(Type, Image, Pos) \
	(Type*)CAtCHS(Image, (Pos), sizeof(Type))
/// ----------
