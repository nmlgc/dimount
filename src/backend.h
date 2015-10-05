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

// Callbacks
// ---------
typedef struct {
	FILESYSTEM *FS;
	PFillFindData FillFindData;
	PDOKAN_FILE_INFO DokanFileInfo;
} FIND_CALLBACK_DATA;

int FindAddFileA(FIND_CALLBACK_DATA *FCD, WIN32_FIND_DATAA *FD);
int FindAddFileW(FIND_CALLBACK_DATA *FCD, WIN32_FIND_DATAW *FD);
// ---------

// All callbacks that handle file names come in both A and W functions.
// Depending on whether the file system stores its filenames in UTF-16 (W) or
// a different encoding (A), only one of those needs to be implemented.
typedef struct FSFORMAT {
	// Returns a user-friendly name of the format of [FS].
	// Should also be implemented for FS == NULL.
	const wchar_t* (*Name)(FILESYSTEM *FS);
	// Maximum filename length.
	const UINT FNLength;
	// Returns 0 if [FS] was successfully identified as a
	// file system of this format.
	int(*Probe)(FILESYSTEM *FS);
	// Returns [Total] and [Available] number of bytes on the file system.
	void(*DiskSizes)(FILESYSTEM *FS, uint64_t *Total, uint64_t *Available);
	// Returns a pointer to the directory entry structure of [FileName], or NULL
	// if the file does not exist.
	ULONG64(*FileLookupA)(FILESYSTEM *FS, const char *FileName);
	ULONG64(*FileLookupW)(FILESYSTEM *FS, const wchar_t *FileName);
	// Calls FindAddFileA()/FindAddFileW() for every file in [DirName].
	NTSTATUS(*FindFiles)(FILESYSTEM *FS, ULONG64 Dir, FIND_CALLBACK_DATA *FCD);
} FSFORMAT;

#define NEW_FSFORMAT(ID, _FNLength, CharSet) \
	const FSFORMAT FS_##ID = { \
		.Name = FS_##ID##_Name, \
		.FNLength = _FNLength, \
		.Probe = FS_##ID##_Probe, \
		.DiskSizes = FS_##ID##_DiskSizes, \
		.FileLookup##CharSet = FS_##ID##_FileLookup##CharSet, \
		.FindFiles = FS_##ID##_FindFiles, \
	}

typedef struct PTFORMAT {
	// User-friendly name of the format.
	const wchar_t *Name;
	// Calls FSNew() for every partition found and returns their total number.
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

/// Addressing
/// ----------
typedef struct {
	uint8_t *Memory;
	uint64_t Size;
} VIEW;

uint8_t *At(VIEW *View, uint64_t Pos, UINT Size);
uint8_t* CAtCHS(CONTAINER *Image, CHS *Pos, UINT Size);

#define LAt(Layer, Pos, Size) \
	At(&(Layer)->View, Pos, Size)

#define StructAt(Type, View, Pos) \
	(Type*)At(&(View), (Pos), sizeof(Type))

#define LStructAt(Type, Layer, Pos) \
	StructAt(Type, (Layer)->View, Pos)

#define FSAtSector(FS, Pos, SizeInSectors) \
	LAt(FS, (Pos) * (FS)->SectorSize, (SizeInSectors) * (FS)->SectorSize)

#define FSStructAtSector(Type, FS, Pos) \
	LStructAt(Type, FS, (Pos) * (FS)->SectorSize)

#define CStructAtCHS(Type, Image, Pos) \
	(Type*)CAtCHS(Image, (Pos), sizeof(Type))
/// ----------

/// Instance types
/// --------------
typedef struct FILESYSTEM {
	CONTAINER *Image; // CONTAINER that contains this file system
	VIEW View;
	const FSFORMAT *FSFormat;
	// Custom filesystem-specific data, allocated using HeapAlloc()
	void *FSData;

	UINT SectorSize;
	UINT CodePage;
	uint32_t Serial;
	wchar_t Label[MAX_PATH];
} FILESYSTEM;

// Returns a pointer to the new file system, or NULL if the file system is
// either empty or outside the bounds of the container.
// TODO: Actually "partition" the view of [Image] and prevent overlapping
// partitions?
FILESYSTEM* FSNew(CONTAINER *Image, unsigned int PartNum, uint64_t Start, uint64_t End);

BOOL FSLabelSetA(FILESYSTEM* FS, const char *Label, size_t LabelLen);

typedef struct CONTAINER {
	const CFORMAT *CFormat;
	const PTFORMAT *PTFormat;
	VIEW View;
	CHS CHSSizes;
	UINT CodePage;
	FILESYSTEM Partitions[16];
} CONTAINER;
/// --------------
