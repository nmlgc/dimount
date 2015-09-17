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

#define FBR_GET \
	const FAT_BOOT_RECORD *fbr = FSStructAt(FAT_BOOT_RECORD, FS, 0);
#define FBR_GET_ASSERT \
	FBR_GET; \
	assert(fbr);

static int FAT_ValidMedia(uint8_t media)
{
	return 0xf8 <= media || media == 0xf0;
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
	uint32_t size = sectors * FS->SectorSize;
	if(!FSAt(FS, size, 0) || !FAT_ValidMedia(fbr->Media)) {
		return 1;
	}
	FS->End = FS->Start + size;
	FS->Serial = fbr->Serial;
	return 0;
}

void FS_FAT_DiskSizes(FILESYSTEM *FS, uint64_t *Total, uint64_t *Available)
{
	FBR_GET_ASSERT;
	uint64_t sectors = fbr->Sectors16 ? fbr->Sectors16 : fbr->Sectors32;
	sectors -= (fbr->FATLength * fbr->FATs) - fbr->SecsReserved;
	*Total = sectors * FS->SectorSize;
	*Available = 0;
}

NEW_FSFORMAT(FAT, L"FAT", 12);
