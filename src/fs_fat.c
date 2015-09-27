/*
 * Dokan Image Mounter
 *
 * FAT file system.
 */

typedef enum {
	FAT_UNKNOWN,
	FAT12 = 1,
	FAT16 = 2,
	FAT32 = 3
} FAT_TYPE;

// FAT cluster number. Since we only need 28 bits, signed is better here.
typedef int32_t fat_cluster_t;

const fat_cluster_t FAT_CLUSTERS_MIN[4] = {0, 2, 0xFF7, 0xFFF7};
const fat_cluster_t FAT_CLUSTERS_MAX[4] = {0, 0xFF6, 0xFFF6, 0x0FFFFFF6};

#pragma pack(push, 1)
typedef struct {
	uint8_t DriveNumber;
	uint8_t MountState;
	uint8_t Signature;
	uint32_t Serial;
	char Label[11];
	char FSType[8];
} FAT_EXTENDED_BOOT_RECORD;

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

	union {
		FAT_EXTENDED_BOOT_RECORD EBPB;

		struct {
			uint32_t FATSectors32;
			uint16_t Flags;
			uint16_t Version;
			uint32_t RootDirCluster;
			uint16_t FSInfoSector;
			uint16_t BootBackupSector;
			FAT_EXTENDED_BOOT_RECORD EBPB;
		} FAT32;
	};
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

typedef fat_cluster_t FAT_Lookup_t(void *fat, fat_cluster_t Num);

// Some precalculated filesystem constants
typedef struct {
	FAT_DIR_ENTRY *RootDir;
	FAT_Lookup_t *Lookup;
	uint8_t **FATs;
	FAT_TYPE Type;
	fat_cluster_t ClusterChainEnd;
	uint32_t FSSectors;
	uint32_t FATSectors;
	uint32_t DataSectors;
	fat_cluster_t Clusters;
} FAT_INFO;

#define FBR_GET \
	const FAT_BOOT_RECORD *fbr = LStructAt(FAT_BOOT_RECORD, FS, 0);
#define FAT_INFO_GET \
	FAT_INFO *fat_info = FS->FSData;
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

fat_cluster_t FAT12_ClusterLookup(uint8_t *fat, fat_cluster_t Num)
{
	fat_cluster_t c = (Num * 3) / 2;
	fat_cluster_t ret;
	if(Num & 1) {
		ret = (fat[c] & 0xF0) >> 4 | (fat[c + 1] << 4);
	} else {
		ret = fat[c] | (fat[c + 1] & 0xF) << 8;
	}
	if(ret > FAT_CLUSTERS_MAX[FAT12]) {
		ret |= 0x0FFFF000;
	}
	return ret;
}

fat_cluster_t FAT16_ClusterLookup(uint16_t *fat, fat_cluster_t Num)
{
	fat_cluster_t ret = fat[Num];
	if(ret > FAT_CLUSTERS_MAX[FAT16]) {
		ret |= 0x0FFF0000;
	}
	return ret;
}

fat_cluster_t FAT32_ClusterLookup(uint32_t *fat, fat_cluster_t Num)
{
	return fat[Num] & 0x0FFFFFFF;
}

fat_cluster_t FAT_ClusterLookup(FAT_INFO *FI, fat_cluster_t Num)
{
	uint8_t *fat = FI->FATs[0];
	if(Num < FI->Clusters) {
		return FI->Lookup(fat, Num);
	}
	return 0;
}

const wchar_t* FS_FAT_Name(FILESYSTEM *FS)
{
	if(!FS) {
		return L"FAT";
	}
	FAT_INFO_GET;
	switch(fat_info->Type) {
	case FAT12:
		return L"FAT12";
	case FAT16:
		return L"FAT16";
	case FAT32:
		return L"FAT32";
	default:
		return L"(unknown)";
	}
}

static FAT_TYPE FAT_TypeFromField(const char *Type)
{
	if(!memcmp(Type, "FAT12   ", 8)) {
		return FAT12;
	} else if(!memcmp(Type, "FAT16   ", 8)) {
		return FAT16;
	} else if(!memcmp(Type, "FAT32   ", 8)) {
		return FAT32;
	}
	return FAT_UNKNOWN;
}

static FAT_TYPE FAT_TypeFromClusterCount(fat_cluster_t Count)
{
	for(FAT_TYPE i = FAT12; i <= FAT32; i++) {
		if(Count >= FAT_CLUSTERS_MIN[i] && Count <= FAT_CLUSTERS_MAX[i]) {
			return i;
		}
	}
	return FAT_UNKNOWN;
}

int FS_FAT_Probe(FILESYSTEM *FS)
{
	FAT_INFO fi = {0};
	FBR_GET;
	if(!fbr) {
		return 1;
	}
	if(!FAT_ValidMedia(fbr->Media) || fbr->FATs == 0) {
		return 1;
	}
	FS->SectorSize = fbr->SecSize;

	// Sort of reliably determine the FAT width using the algorithm from
	// http://homepage.ntlworld.com/jonathan.deboynepollard/FGA/determining-fat-widths.html

	fi.FSSectors = fbr->FSSectors16 ? fbr->FSSectors16 : fbr->FSSectors32;

	if(fbr->FAT32.EBPB.Signature == 0x28 || fbr->FAT32.EBPB.Signature == 0x29) {
		fi.FATSectors = fbr->FATSectors16 ? fbr->FATSectors16 : fbr->FAT32.FATSectors32;
		fi.Type = FAT_TypeFromField(fbr->FAT32.EBPB.FSType);
	} else {
		fi.FATSectors = fbr->FATSectors16;
		fi.Type = FAT_TypeFromField(fbr->EBPB.FSType);
	}
	uint32_t root_dir_len =
		fbr->RootDirEntries * sizeof(FAT_DIR_ENTRY) + fbr->SecSize - 1;
	uint32_t root_dir_start_sec = fbr->SecsReserved + (fi.FATSectors * fbr->FATs);
	uint32_t data_start_sec = root_dir_start_sec + (root_dir_len / fbr->SecSize);
	fi.DataSectors = fi.FSSectors - data_start_sec;
	fi.Clusters = 2 + fi.DataSectors / fbr->SecsPerClus;
	fi.RootDir = FSStructAtSector(FAT_DIR_ENTRY, FS, root_dir_start_sec);
	if(fi.Type == FAT_UNKNOWN) {
		fi.Type = FAT_TypeFromClusterCount(fi.Clusters);
		if(fi.Type == FAT_UNKNOWN) {
			return 1;
		}
	}
	uint32_t size = fi.FSSectors * FS->SectorSize;
	if(!LAt(FS, size, 0)) {
		return 1;
	}
	FS->View.Size = size;

	fat_cluster_t max_clusters = fi.FATSectors * fbr->SecSize;
	switch(fi.Type) {
	case FAT12:
		fi.Lookup = FAT12_ClusterLookup;
		max_clusters = (max_clusters * 2) / 3;
		FS->Serial = fbr->EBPB.Serial;
		break;
	case FAT16:
		fi.Lookup = FAT16_ClusterLookup;
		max_clusters /= 2;
		FS->Serial = fbr->EBPB.Serial;
		break;
	case FAT32:
		fi.Lookup = FAT32_ClusterLookup;
		max_clusters /= 4;
		FS->Serial = fbr->FAT32.EBPB.Serial;
		break;
	}
	fi.Clusters = fi.DataSectors / fbr->SecsPerClus;
	if(fi.Clusters > max_clusters) {
		return 1;
	}

	fi.FATs = HeapAlloc(GetProcessHeap(), 0, sizeof(uint8_t*) * fbr->FATs);
	if(!fi.FATs) {
		return ERROR_OUTOFMEMORY;
	}
	for(uint8_t i = 0; i < fbr->FATs; i++) {
		fi.FATs[i] = FSAtSector(
			FS, fbr->SecsReserved + (fi.FATSectors * i), fi.FATSectors
		);
	}

	FS->FSData = HeapAlloc(GetProcessHeap(), 0, sizeof(FAT_INFO));
	if(!FS->FSData) {
		return ERROR_OUTOFMEMORY;
	}
	fi.ClusterChainEnd = FAT_ClusterLookup(&fi, 1);
	memcpy(FS->FSData, &fi, sizeof(FAT_INFO));
	return 0;
}

void FS_FAT_DiskSizes(FILESYSTEM *FS, uint64_t *Total, uint64_t *Available)
{
	FBR_GET_ASSERT;
	FAT_INFO_GET;
	*Total = fat_info->DataSectors * FS->SectorSize;
	*Available = 0;
	for(fat_cluster_t i = 2; i < fat_info->Clusters; i++) {
		if(FAT_ClusterLookup(fat_info, i) == 0) {
			*Available += FS->SectorSize * fbr->SecsPerClus;
		}
	}
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
