/*
 * Dokan Image Mounter
 *
 * Fallback partition table. Simply assigns the entire remaining image to a
 * single partition.
 */

unsigned int P_None_Probe(CONTAINER *Image)
{
	FSNew(Image, 0, 0, Image->View.Size);
	return 1;
}

NEW_PTFORMAT(None, L"(none)");
