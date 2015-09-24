/*
 * Dokan Image Mounter
 *
 * FAT file system.
 */

#pragma pack(push, 1)
typedef struct {
	uint8_t Jump[3]; // Boot strap short or near jump
	uint8_t SystemID[8]; // Name - can be used to special case partition manager volumes
	uint16_t SecSize; // bytes per logical sector
	uint8_t SecsPerClus; // sectors/cluster
	uint16_t SecsReserved; // reserved sectors
	uint8_t	FATs; // number of FATs
	uint16_t RootDirEntries;
	uint16_t FSSectors16; // 16-bit number of sectors
	uint8_t	Media; // media code
	uint16_t FATSectors16; // sectors/FAT
	uint16_t SecsPerTrack; // sectors per track
	uint16_t Heads; // number of heads
	uint32_t SecsHidden; // hidden sectors (unused)
	uint32_t FSSectors32; // 32-bit number of sectors (if FSSectors16 == 0)
	uint8_t OSData[3]; // ???
	uint32_t Serial; // serial number
} FAT_BOOT_RECORD;

typedef struct {
	// Both BaseName and Extension are padded out with spaces. The first
	// character of FileName can be the following:
	// * 0x00: unused file
	// * 0x05: first character is actually 0xE5
	// * 0xE5: previously deleted file
	char BaseName[8];
	char Extension[3];
	uint8_t Attribute;
	uint8_t Reserved[10];
	uint16_t Time;
	uint16_t Date;
	uint16_t FirstCluster;
	uint32_t Size;
} FAT_DIR_ENTRY;
#pragma pack(pop)

// Some precalculated filesystem constants
typedef struct {
	FAT_DIR_ENTRY *RootDir;
	uint32_t FSSectors;
	uint32_t DataSectors;
} FAT_INFO;

#define FBR_GET \
	const FAT_BOOT_RECORD *fbr = LStructAt(FAT_BOOT_RECORD, FS, 0);
#define FAT_INFO_GET \
	const FAT_INFO *fat_info = FS->FSData;
#define FBR_GET_ASSERT \
	FBR_GET; \
	assert(fbr);

static int FAT_ValidMedia(uint8_t media)
{
	return 0xf8 <= media || media == 0xf0;
}

static size_t FAT_NameComponentCopy(char *dst, const char *src, size_t len)
{
	assert(dst);
	assert(src);
	len = TrimmedLength(src, len);
	memcpy(dst, src, len);
	dst[len] = '\0';
	return len;
}

const wchar_t* FS_FAT_Name(FILESYSTEM *FS)
{
	return L"FAT";
}

int FS_FAT_Probe(FILESYSTEM *FS)
{
	FBR_GET;
	if(!fbr) {
		return 1;
	}
	if(fbr->FATs == 0) {
		return 1;
	}
	FS->SectorSize = fbr->SecSize;
	uint32_t sectors = fbr->FSSectors16 ? fbr->FSSectors16 : fbr->FSSectors32;
	uint32_t root_dir_sector = fbr->SecsReserved + (fbr->FATSectors16 * fbr->FATs);
	uint32_t size = sectors * FS->SectorSize;
	if(!LAt(FS, size, 0) || !FAT_ValidMedia(fbr->Media)) {
		return 1;
	}
	FS->View.Size = size;
	FS->Serial = fbr->Serial;

	FS->FSData = HeapAlloc(GetProcessHeap(), 0, sizeof(FAT_INFO));
	if(!FS->FSData) {
		return ERROR_OUTOFMEMORY;
	}
	FAT_INFO *fat_info = FS->FSData;
	fat_info->RootDir = FSStructAtSector(FAT_DIR_ENTRY, FS, root_dir_sector);
	fat_info->FSSectors = sectors;
	fat_info->DataSectors = sectors - root_dir_sector;
	return 0;
}

void FS_FAT_DiskSizes(FILESYSTEM *FS, uint64_t *Total, uint64_t *Available)
{
	FAT_INFO_GET;
	*Total = fat_info->DataSectors * FS->SectorSize;
	*Available = 0;
}

int FS_FAT_FindFilesA(FILESYSTEM *FS, const char* DirName, uint64_t *State, WIN32_FIND_DATAA *FD)
{
	// Only root directory for now
	if(strcmp(DirName, "\\")) {
		return 0;
	}
	FBR_GET_ASSERT;
	FAT_INFO_GET;
	FAT_DIR_ENTRY *dentry = fat_info->RootDir + *State;
	if(!dentry) {
		return -1;
	}
	unsigned char first = dentry->BaseName[0];
	if(first == 0x00) {
		return 0;
	} else if(first != 0xE5 && (dentry->Attribute & 0x08) == 0) {
		char ext[sizeof(dentry->Extension) + 1] = {0};
		FAT_NameComponentCopy(ext, dentry->Extension, sizeof(dentry->Extension));
		size_t name_len = FAT_NameComponentCopy(
			FD->cFileName, dentry->BaseName, sizeof(dentry->BaseName)
		);
		if(first == 0x05) {
			FD->cFileName[0] = 0xE5;
		}
		if(ext[0] != '\0') {
			FD->cFileName[name_len++] = '.';
			memcpy(&FD->cFileName[name_len], ext, sizeof(ext));
		}
		FILETIME timestamp;
		DosDateTimeToFileTime(dentry->Date, dentry->Time, &timestamp);
		FD->nFileSizeHigh = 0;
		FD->nFileSizeLow = dentry->Size;
		FD->ftCreationTime = timestamp;
		FD->ftLastAccessTime = timestamp;
		FD->ftLastWriteTime = timestamp;
		FD->dwFileAttributes = dentry->Attribute;
	}
	(*State)++;
	return *State < fbr->RootDirEntries;
}

NEW_FSFORMAT(FAT, 12, A);
