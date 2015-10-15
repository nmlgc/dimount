/*
 * Dokan Image Mounter
 *
 * Fallback container format.
 */

uint64_t C_None_Probe(CONTAINER *Image)
{
	return 0;
}

NEW_CFORMAT(None, L"(none)");
