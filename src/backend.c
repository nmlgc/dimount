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
			break;
		}
	}
	if(!Image->PTFormat) {
		return -1;
	}
	for(unsigned int i = 0; i < partitions_found; i++) {
		bool remove = false;
		FILESYSTEM *fs = &Image->Partitions[i];
		fs->Image = Image;

		if(fs->End == fs->Start) {
			remove = true;
		}
		if(!FSAt(fs, fs->End - fs->Start, 0)) {
			fwprintf(stderr,
				L"**Error** Partition #%u (%llu - %llu) exceeds container size (%llu bytes)\n",
				i + 1, fs->Start, fs->End, Image->CSize - Image->Sector0Offset
			);
			remove = true;
		}
		if(remove) {
			memmove(fs, fs + 1, sizeof(Image->Partitions) - i * sizeof(FILESYSTEM));
			i--;
			partitions_found--;
		}
	}
	return partitions_found;
}

const CFORMAT* ImageCFormatProbe(CONTAINER *Image)
{
	assert(Image);
	for(const CFORMAT **c = CFormats; *c; c++) {
		uint64_t offset = (*c)->Probe(Image);
		if(offset != -1) {
			Image->Sector0Offset = offset;
			Image->CFormat = *c;
			return *c;
		}
	}
	return NULL;
}
/// -------

/// Instance types
/// --------------
BOOL FSLabelSetA(FILESYSTEM* FS, const char *Label, size_t LabelLen)
{
	assert(FS);
	return MultiByteToWideChar(FS->CodePage, 0, Label, -1, FS->Label, LabelLen);
}
/// --------------

/// Addressing
/// ----------
uint8_t* FSAt(FILESYSTEM *FS, uint64_t Pos, UINT Size)
{
	assert(FS);
	assert(FS->Image);
	uint8_t *ret = CAt(FS->Image, FS->Start + Pos, Size);
	return ret && ((Pos + Size) <= FS->End) ? ret : NULL;
}

uint8_t* CAt(CONTAINER *Image, uint64_t Pos, UINT Size)
{
	assert(Image);
	uint8_t *ret = Image->View + Image->Sector0Offset + Pos;
	return (ret + Size) <= (Image->View + Image->CSize) ? ret : NULL;
}

uint8_t* CAtCHS(CONTAINER *Image, CHS *Pos, UINT Size)
{
	assert(Image);
	uint64_t bytepos = (uint64_t)(
		Pos->Cylinder * Image->CHSSizes.Cylinder
		+ Pos->Head * Image->CHSSizes.Head
		+ Pos->Sector * Image->CHSSizes.Sector
	);
	return CAt(Image, bytepos, Size);
}
/// ----------
