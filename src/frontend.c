/*
 * Dokan Image Mounter - Frontend
 */

/// Error reporting
/// ---------------
#define W32_ERR_REPORT(FailCondition, ReturnValue, Prefix, ...) \
	if(FailCondition) { \
		ret = ReportError(ReturnValue, GetLastError(), Prefix, __VA_ARGS__); \
		goto end; \
	}

int ReportError(int ReturnValue, DWORD Error, const wchar_t *Prefix, ...)
{
	va_list va;
	wchar_t *msg_str = NULL;

	va_start(va, Prefix);

	FormatMessageW(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		NULL, Error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&msg_str, 0, NULL
	);

	vfwprintf(stderr, Prefix, va);
	fwprintf(stderr, L": %s", msg_str);
	LocalFree(msg_str);
	va_end(va);
	return ReturnValue;
}
/// ---------------

static void CopyFindDataAToW(
	UINT CodePage,
	LPWIN32_FIND_DATAW fd_w,
	LPWIN32_FIND_DATAA fd_a
)
{
	fd_w->dwFileAttributes = fd_a->dwFileAttributes;
	fd_w->ftCreationTime = fd_a->ftCreationTime;
	fd_w->ftLastAccessTime = fd_a->ftLastAccessTime;
	fd_w->ftLastWriteTime = fd_a->ftLastWriteTime;
	fd_w->nFileSizeHigh = fd_a->nFileSizeHigh;
	fd_w->nFileSizeLow = fd_a->nFileSizeLow;
	fd_w->dwReserved0 = fd_a->dwReserved0;
	fd_w->dwReserved1 = fd_a->dwReserved1;
	MultiByteToWideChar(
		CodePage, 0, fd_a->cFileName, -1, fd_w->cFileName, sizeof(fd_w->cFileName)
	);
}

/// Dokan callbacks
/// ---------------
#define PrintEnter fprintf(stderr, __FUNCTION__);
#define PrintEnterln fprintf(stderr, "%s\n", __FUNCTION__);
#define DIMCallbackEnter \
	FILESYSTEM *fs = (FILESYSTEM*)DokanFileInfo->DokanOptions->GlobalContext; \
	const FSFORMAT *fmt = fs->FSFormat;

static int DOKAN_CALLBACK DIMCreateFile(
	LPCWSTR FileName,
	DWORD AccessMode,
	DWORD ShareMode,
	DWORD CreationDisposition,
	DWORD FlagsAndAttributes,
	PDOKAN_FILE_INFO DokanFileInfo
)
{
#ifdef _DEBUG
	PrintEnter;
	const wchar_t *DISPOSITION =
		CreationDisposition == CREATE_NEW ? L"CREATE_NEW"
		: CreationDisposition == OPEN_ALWAYS ? L"OPEN_ALWAYS"
		: CreationDisposition == CREATE_ALWAYS ? L"CREATE_ALWAYS"
		: CreationDisposition == OPEN_EXISTING ? L"OPEN_EXISTING"
		: CreationDisposition == TRUNCATE_EXISTING ? L"TRUNCATE_EXISTING"
		: L"???"
	;
	fwprintf(stderr, L"(%s, %s)\n", FileName, DISPOSITION);
#endif
	return 0;
}

static int DOKAN_CALLBACK DIMFindFiles(
	LPCWSTR FileNameW,
	PFillFindData FillFindData,
	PDOKAN_FILE_INFO DokanFileInfo
)
{
	DIMCallbackEnter;
#ifdef _DEBUG
	PrintEnter;
	fwprintf(stderr, L"(%s)\n", FileNameW);
#endif
	bool unicode = fmt->FindFilesW != NULL;
	const void *filename;
	char filename_a[MAX_PATH];
	FindFiles_t *func;
	if(unicode) {
		filename = FileNameW;
		func = fmt->FindFilesW;
	} else {
		WideCharToMultiByte(
			fs->CodePage, 0, FileNameW, -1, filename_a, sizeof(filename_a), NULL, NULL
		);
		filename = filename_a;
		func = fmt->FindFilesA;
	}
	uint64_t state = 0;
	WIN32_FIND_DATAA fd_a = {0};
	WIN32_FIND_DATAW fd_w = {0};
	WIN32_FIND_DATAW *fd_for_fs = unicode ? &fd_w : (WIN32_FIND_DATAW*)&fd_a;
	while(func(fs, filename, &state, fd_for_fs) > 0) {
		if(fd_w.cFileName[0] != '\0' || fd_a.cFileName[0] != '\0') {
			if(!unicode) {
				CopyFindDataAToW(fs->CodePage, &fd_w, &fd_a);
			}
			FillFindData(&fd_w, DokanFileInfo);
			fd_w.cFileName[0] = '\0';
			fd_a.cFileName[0] = '\0';
		}
	}
	return 0;
}

static int DOKAN_CALLBACK DIMGetDiskFreeSpace(
	PULONGLONG FreeBytesAvailable,
	PULONGLONG TotalNumberOfBytes,
	PULONGLONG TotalNumberOfFreeBytes,
	PDOKAN_FILE_INFO DokanFileInfo
)
{
	DIMCallbackEnter;
#ifdef _DEBUG
	PrintEnterln;
#endif
	fmt->DiskSizes(fs, TotalNumberOfBytes, TotalNumberOfFreeBytes);
	*FreeBytesAvailable = *TotalNumberOfFreeBytes;
	return 0;
}

static int DOKAN_CALLBACK DIMGetVolumeInformation(
	LPWSTR VolumeNameBuffer,
	DWORD VolumeNameSize,
	LPDWORD VolumeSerialNumber,
	LPDWORD MaximumComponentLength,
	LPDWORD FileSystemFlags,
	LPWSTR FileSystemNameBuffer,
	DWORD FileSystemNameSize,
	PDOKAN_FILE_INFO DokanFileInfo
)
{
	DIMCallbackEnter;
#ifdef _DEBUG
	PrintEnterln;
#endif
	UNREFERENCED_PARAMETER(DokanFileInfo);
	wcscpy_s(VolumeNameBuffer, VolumeNameSize / sizeof(WCHAR), fs->Label);
	*VolumeSerialNumber = fs->Serial;
	*MaximumComponentLength = fmt->FNLength;
	wcscpy_s(FileSystemNameBuffer, FileSystemNameSize / sizeof(WCHAR), fmt->Name(fs));
	return 1;
}

// This is the magical required function that makes everything else work in
// Explorer.
static int DOKAN_CALLBACK DIMOpenDirectory(
	LPCWSTR FileName,
	PDOKAN_FILE_INFO DokanFileInfo
)
{
#ifdef _DEBUG
	PrintEnter;
	fwprintf(stderr, L"(%s)\n", FileName);
#endif
	return 0;
}

DOKAN_OPERATIONS operations = {
	.CreateFile = DIMCreateFile,
	.FindFiles = DIMFindFiles,
	.GetDiskFreeSpace = DIMGetDiskFreeSpace,
	.GetVolumeInformation = DIMGetVolumeInformation,
	.OpenDirectory = DIMOpenDirectory,
};
/// ---------------

int dimount(const wchar_t *Mountpoint, const wchar_t *ImageFN)
{
	int ret = 0;
	HANDLE image_map = NULL;
	HANDLE image_file = NULL;

	CONTAINER image = {0};

	// TODO: Open writable.
	// TODO: Don't lock the image file.
	image_file = CreateFileW(
		ImageFN, GENERIC_READ, 0, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
	);
	W32_ERR_REPORT(image_file == INVALID_HANDLE_VALUE,
		-2, L"Error opening %s", ImageFN
	);
	LARGE_INTEGER image_size;
	W32_ERR_REPORT(!GetFileSizeEx(image_file, &image_size),
		-3, L"Error retrieving the file size of %s", ImageFN
	);
	image.CSize = image_size.QuadPart;
	if(image.CSize == 0) {
		fwprintf(stderr, L"Not mounting an empty file.\n");
		ret = -4;
		goto end;
	}
	// TODO: Writable, again.
	image_map = CreateFileMapping(
		image_file, NULL, PAGE_READONLY, 0, 0, NULL
	);
	W32_ERR_REPORT(
		!image_map, -5, L"Error mapping %s", ImageFN
	);

	// TODO: Writable, again.
	image.View = MapViewOfFile(image_map, FILE_MAP_READ, 0, 0, 0);
	W32_ERR_REPORT(
		!image.View, -6, L"Error mapping %s into memory", ImageFN
	);
	if(ImageCFormatProbe(&image)) {
		fwprintf(stdout, L"Container format: %s\n", image.CFormat->Name);
	} else {
		fwprintf(stderr, L"Unknown container format.\n");
		ret = -7;
		goto end;
	}
	unsigned int partitions_found = ImagePTFormatProbe(&image);
	if(partitions_found > 0) {
		fwprintf(stdout, L"Partition table format: %s\n", image.PTFormat->Name);
	} else if(partitions_found == 0) {
		fwprintf(stderr, L"No partitions in image.\n");
		ret = -8;
		goto end;
	} else if(partitions_found < 0) {
		fwprintf(stderr, L"Unknown partition table format.\n");
		ret = -9;
		goto end;
	}

	FILESYSTEM *fs_to_mount = NULL;
	unsigned int i;
	for(i = 0; i < partitions_found; i++) {
		if(!ImageFSFormatProbe(&image.Partitions[i])) {
			fs_to_mount = &image.Partitions[i];
		}
	}

	if(fs_to_mount) {
		fwprintf(stdout,
			L"Mounting partition #%d. File system format: %s\n",
			i, fs_to_mount->FSFormat->Name(fs_to_mount)
		);
	} else {
		fwprintf(stderr, L"Found no supported file system on any partition.\n");
		ret = -10;
		goto end;
	}

	DOKAN_OPTIONS options = {
		.Version = DOKAN_VERSION,
		.ThreadCount = 0,
		.MountPoint = Mountpoint,
		.Options = DOKAN_OPTION_KEEP_ALIVE | DOKAN_OPTION_DEBUG,
		.GlobalContext = (ULONG64)fs_to_mount,
	};
	ret = DokanMain(&options, &operations);

end:
	UnmapViewOfFile(image.View);
	CloseHandle(image_map);
	CloseHandle(image_file);
	return ret;
}

int __cdecl wmain(ULONG argc, const wchar_t *argv[])
{
	if(argc < 3) {
		fwprintf(stderr, L"Usage: %s mountpoint imagefile\n", argv[0]);
		return -1;
	}
	return dimount(argv[1], argv[2]);
}
