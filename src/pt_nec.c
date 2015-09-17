/*
 * Dokan Image Mounter
 *
 * Partition tables used in the NEC PC-9801 series.
 */

#pragma pack(push, 1)
typedef struct {
	uint8_t Sector;
	uint8_t Head;
	uint16_t Cylinder;
} SHC_8_8_16;

typedef struct {
	uint8_t Boot;
	uint8_t Active;
	uint8_t Dummy[2]; // 32-bit padding
	SHC_8_8_16 IPL;
	SHC_8_8_16 Start;
	SHC_8_8_16 Length;
	char Name[16]; // Padded out with spaces
} PARTENTRY_98;
#pragma pack(pop)

static uint64_t SHCToBytes(SHC_8_8_16 *Pos, CHS *Sizes)
{
	assert(Pos && Sizes);
	return (uint64_t)(
		Pos->Cylinder * Sizes->Cylinder
		+ Pos->Head * Sizes->Head
		+ Pos->Sector * Sizes->Sector
	);
}

unsigned int P_NEC_Probe(CONTAINER *Image)
{
	CHS Cylinder1 = {.Sector = 1, .Head = 0, .Cylinder = 0};
	PARTENTRY_98 *p98 = CStructAtCHS(PARTENTRY_98, Image, &Cylinder1);
	int i = 0;
	int partitions_found = 0;
	while(p98 && i < elementsof(Image->Partitions)) {
		uint64_t start = SHCToBytes(&p98->Start, &Image->CHSSizes);
		uint64_t length = SHCToBytes(&p98->Length, &Image->CHSSizes);
		uint64_t end = start + length;
		if(p98->Active && CAt(Image, start, 1) && CAt(Image, end, 0)) {
			FILESYSTEM *fs = &Image->Partitions[partitions_found++];
			fs->CodePage = 932;
			fs->SectorSize = Image->CHSSizes.Sector;
			fs->Start = start;
			fs->End = end;
			FSLabelSetA(fs, p98->Name, sizeof(p98->Name));
		}
		p98++;
		i++;
	}
	return partitions_found;
}

NEW_PTFORMAT(NEC, L"NEC PC-9801 series partition table");
