/*
 * Dokan Image Mounter
 *
 * HDI container format.
 */

typedef struct {
	uint32_t Dummy;
	uint32_t HDDType;
	uint32_t HeaderSize;
	uint32_t HDDSize;
	uint32_t SectorSize;
	uint32_t Sectors;
	uint32_t Heads;
	uint32_t Cylinders;
} HDIHDR;

uint64_t C_HDI_Probe(CONTAINER *Image)
{
	HDIHDR *hdi = StructAt(HDIHDR, Image->View, 0);
	if(!hdi) {
		return -1;
	}
	uint64_t expsize =
		hdi->SectorSize * hdi->Sectors * hdi->Heads * hdi->Cylinders;
	if(
		hdi->HDDSize != expsize
		|| !At(&Image->View, hdi->HeaderSize + hdi->HDDSize, 0)
	) {
		return -1;
	}
	Image->CHSSizes.Sector = hdi->SectorSize;
	Image->CHSSizes.Head = Image->CHSSizes.Sector * hdi->Sectors;
	Image->CHSSizes.Cylinder = Image->CHSSizes.Head * hdi->Heads;
	return hdi->HeaderSize;
}

NEW_CFORMAT(HDI, L"Anex86 hard disk image");
