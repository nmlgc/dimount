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
	uint16_t Sectors16; // 16-bit number of sectors
	uint8_t	Media; // media code
	uint16_t FATLength; // sectors/FAT
	uint16_t SecsPerTrack; // sectors per track
	uint16_t Heads; // number of heads
	uint32_t SecsHidden; // hidden sectors (unused)
	uint32_t Sectors32; // 32-bit number of sectors (if sectors_16 == 0)
	uint8_t OSData[3]; // ???
	uint32_t Serial; // serial number
} FAT_BOOT_RECORD;
#pragma pack(pop)

// Some precalculated filesystem constants
typedef struct {
	uint32_t Sectors;
	uint32_t DataSectors;
} FAT_INFO;

#define FBR_GET \
	const FAT_BOOT_RECORD *fbr = FSStructAt(FAT_BOOT_RECORD, FS, 0);
#define FAT_INFO_GET \
	const FAT_INFO *fat_info = FS->FSData;
#define FBR_GET_ASSERT \
	FBR_GET; \
	assert(fbr);

static int FAT_ValidMedia(uint8_t media)
{
	return 0xf8 <= media || media == 0xf0;
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
	uint32_t sectors = fbr->Sectors16 ? fbr->Sectors16 : fbr->Sectors32;
	uint32_t root_dir_sector = fbr->SecsReserved + (fbr->FATLength * fbr->FATs);
	uint32_t size = sectors * FS->SectorSize;
	if(!FSAt(FS, size, 0) || !FAT_ValidMedia(fbr->Media)) {
		return 1;
	}
	FS->End = FS->Start + size;
	FS->Serial = fbr->Serial;

	FS->FSData = HeapAlloc(GetProcessHeap(), 0, sizeof(FAT_INFO));
	if(!FS->FSData) {
		return ERROR_OUTOFMEMORY;
	}
	FAT_INFO *fat_info = FS->FSData;
	fat_info->Sectors = sectors;
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
	return 0;
}

NEW_FSFORMAT(FAT, 12, A);
