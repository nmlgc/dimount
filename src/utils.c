/*
 * Dokan Image Mounter - Random utility functions
 */

// Returns the length of [str] with any space characters trimmed from its end.
size_t TrimmedLength(const char *str, size_t len)
{
	assert(str);
	if(len == 0) {
		return 0;
	}
	int i = (int)(len) - 1;
	while(i >= 0 && str[i--] == ' ') {
		len--;
	}
	return len;
}

bool IsDirSepA(const char c)
{
	return c == '/' || c == '\\';
}

bool IsDirSepW(const wchar_t c)
{
	return c == L'/' || c == L'\\';
}
