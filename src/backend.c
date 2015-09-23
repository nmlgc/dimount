/*
 * Dokan Image Mounter - Backend
 */

/// Probing
/// -------
int ImageFSFormatProbe(FILESYSTEM *FS)
{
	assert(FS);
	for(const FSFORMAT **f = FSFormats; *f; f++) {
		int error = (*f)->Probe(FS);
		if(!error) {
			FS->FSFormat = *f;
			return error;
		}
	}
	return 1;
}

int ImagePTFormatProbe(CONTAINER *Image)
{
	assert(Image);
	unsigned int partitions_found = 0;
	for(const PTFORMAT **p = PTFormats; *p; p++) {
		partitions_found = (*p)->Probe(Image);
		if(partitions_found) {
			Image->PTFormat = *p;
			return partitions_found;
		}
	}
	return -1;
}

const CFORMAT* ImageCFormatProbe(CONTAINER *Image)
{
	assert(Image);
	for(const CFORMAT **c = CFormats; *c; c++) {
		uint64_t offset = (*c)->Probe(Image);
		if(offset != -1) {
			uint8_t *memory = LAt(Image, offset, 1);
			if(memory) {
				Image->View.Memory = memory;
				Image->View.Size -= offset;
				Image->CFormat = *c;
				return *c;
			}
		}
	}
	return NULL;
}
/// -------

/// Instance types
/// --------------
FILESYSTEM* FSNew(CONTAINER *Image, unsigned int PartNum, uint64_t Start, uint64_t End)
{
	assert(Image);
	uint8_t *memory = LAt(Image, Start, 1);
	uint64_t size = End - Start;
	if(!size || !memory) {
		return NULL;
	}
	FILESYSTEM *fs = &Image->Partitions[PartNum];
	fs->Image = Image;
	fs->View.Memory = memory;
	fs->View.Size = size;
	if(!LAt(fs, size, 0)) {
		fwprintf(stderr,
			L"**Error** Partition #%u (%llu - %llu) exceeds container size (%llu bytes)\n",
			PartNum + 1, Start, End, Image->View.Size
		);
		return NULL;
	}
	return fs;
}

BOOL FSLabelSetA(FILESYSTEM* FS, const char *Label, size_t LabelLen)
{
	assert(FS);
	return MultiByteToWideChar(
		FS->CodePage, 0, Label, -1, FS->Label, TrimmedLength(Label, LabelLen)
	);
}
/// --------------

/// Addressing
/// ----------
uint8_t *At(VIEW *View, uint64_t Pos, UINT Size)
{
	assert(View);
	assert(View->Memory);
	return (Pos + Size) <= View->Size ? View->Memory + Pos : NULL;
}

uint8_t* CAtCHS(CONTAINER *Image, CHS *Pos, UINT Size)
{
	assert(Image);
	uint64_t bytepos = (uint64_t)(
		Pos->Cylinder * Image->CHSSizes.Cylinder
		+ Pos->Head * Image->CHSSizes.Head
		+ Pos->Sector * Image->CHSSizes.Sector
	);
	return At(&Image->View, bytepos, Size);
}
/// ----------
