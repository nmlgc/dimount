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

/// Dokan callbacks
/// ---------------
#define PrintEnter fprintf(stderr, __FUNCTION__);
#define PrintEnterln fprintf(stderr, "%s\n", __FUNCTION__);
#define DIMCallbackEnter \
	FILESYSTEM *fs = (FILESYSTEM*)DokanFileInfo->DokanOptions->GlobalContext; \
	const FSFORMAT *fmt = fs->FSFormat;

#define DIMCodePageCall(Func, ...) \
	bool unicode = fmt->Func##W != NULL; \
	char filename_a[MAX_PATH]; \
	ULONG64 ret; \
	if(unicode) { \
		ret = fmt->Func##W(fs, FileNameW, __VA_ARGS__); \
	} else { \
		WideCharToMultiByte( \
			fs->CodePage, 0, FileNameW, -1, filename_a, sizeof(filename_a), NULL, NULL \
		); \
		ret = fmt->Func##A(fs, filename_a, __VA_ARGS__); \
	}

// TODO: Apparently, some functions that take the file name can be called
// on an invalid handle?! The file system would then have to call CreateFile()
// anyway, so we might as well do that in the frontend, avoiding the need for
// separate A/W versions of those functions in the process.
#define DIMFileShouldBeOpen \
	assert(DokanFileInfo->Context);

ULONG64 DOKAN_CALLBACK DIMFileLookup(
	LPCWSTR FileNameW,
	PDOKAN_FILE_INFO DokanFileInfo
)
{
	DIMCallbackEnter;
	DIMCodePageCall(FileLookup);
	return ret;
}

NTSTATUS DOKAN_CALLBACK DIMCreateFile(
	LPCWSTR FileNameW,
	DWORD AccessMode,
	DWORD ShareMode,
	DWORD CreationDisposition,
	DWORD FlagsAndAttributes,
	PDOKAN_FILE_INFO DokanFileInfo
)
{
	DIMCallbackEnter;
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
	fwprintf(stderr, L"(%s, %s)\n", FileNameW, DISPOSITION);
#endif
	// TODO: Implement ShareMode in the frontend.
	if(
		CreationDisposition == CREATE_NEW
		|| CreationDisposition == CREATE_ALWAYS
		|| CreationDisposition == TRUNCATE_EXISTING
	) {
		fwprintf(stderr, L"(write access not implemented yet)");
		return -ERROR_ACCESS_DENIED;
	}
	HANDLE handle = DokanOpenRequestorToken(DokanFileInfo);
	CloseHandle(handle);
	DIMCodePageCall(CreateFile, AccessMode, CreationDisposition, FlagsAndAttributes, DokanFileInfo);
	return (NTSTATUS)ret;
}

NTSTATUS DOKAN_CALLBACK DIMFindFiles(
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
	FIND_CALLBACK_DATA fcd;
	fcd.FS = fs;
	fcd.FillFindData = FillFindData;
	fcd.DokanFileInfo = DokanFileInfo;
	return fmt->FindFiles(fs, DIMFileLookup(FileNameW, DokanFileInfo), &fcd);
}

NTSTATUS DOKAN_CALLBACK DIMGetDiskFreeSpace(
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
	return STATUS_SUCCESS;
}

NTSTATUS DOKAN_CALLBACK DIMGetFileInformation(
	LPCWSTR FileName,
	LPBY_HANDLE_FILE_INFORMATION HandleFileInfo,
	PDOKAN_FILE_INFO DokanFileInfo
)
{
	DIMCallbackEnter;
#ifdef _DEBUG
	PrintEnter;
	fwprintf(stderr, L"(%s)\n", FileName);
#endif
	DIMFileShouldBeOpen;
	HandleFileInfo->dwVolumeSerialNumber = fs->Serial;
	return fmt->GetFileInformation(fs, HandleFileInfo, DokanFileInfo);
}

NTSTATUS DOKAN_CALLBACK DIMGetVolumeInformation(
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
	wcscpy_s(VolumeNameBuffer, VolumeNameSize, fs->Label);
	*VolumeSerialNumber = fs->Serial;
	*MaximumComponentLength = fmt->FNLength;
	*FileSystemFlags = FILE_READ_ONLY_VOLUME;
	wcscpy_s(FileSystemNameBuffer, FileSystemNameSize, fmt->Name(fs));
	return STATUS_SUCCESS;
}

NTSTATUS DOKAN_CALLBACK DIMReadFile(
	LPCWSTR FileName,
	LPVOID Buffer,
	DWORD BufferLength,
	LPDWORD ReadLength,
	LONGLONG Offset,
	PDOKAN_FILE_INFO DokanFileInfo
)
{
	DIMCallbackEnter;
#ifdef _DEBUG
	PrintEnter;
	fwprintf(stderr, L"(%s, %u bytes from %I64u)\n", FileName, BufferLength, Offset);
#endif
	// Yes, normal ReadFile() crashes inside the kernel if [ReadLength] is NULL,
	// but that doesn't mean we should depend on that or, worse, do the same!
	if(!ReadLength) {
		return STATUS_INVALID_PARAMETER;
	}
	*ReadLength = 0;
	if(!Buffer) {
		return BufferLength == 0 ? STATUS_SUCCESS : STATUS_ACCESS_VIOLATION;
	} else if(BufferLength == 0) {
		return STATUS_SUCCESS;
	}
	DIMFileShouldBeOpen;
	LONGLONG size = fmt->FileSize((void*)DokanFileInfo->Context);
	LONGLONG end = Offset + BufferLength;
	if(Offset >= size) {
		return STATUS_SUCCESS;
	} else if(end > size || end < Offset) {
		BufferLength = (DWORD)(size - Offset);
	}
	return fmt->ReadFile(fs, Buffer, BufferLength, ReadLength, Offset, DokanFileInfo);
}

// This is the magical required function that makes everything else work in
// Explorer.
NTSTATUS DOKAN_CALLBACK DIMOpenDirectory(
	LPCWSTR FileName,
	PDOKAN_FILE_INFO DokanFileInfo
)
{
#ifdef _DEBUG
	PrintEnter;
	fwprintf(stderr, L"(%s)\n", FileName);
#endif
	return STATUS_SUCCESS;
}

DOKAN_OPERATIONS operations = {
	.CreateFile = DIMCreateFile,
	.FindFiles = DIMFindFiles,
	.GetDiskFreeSpace = DIMGetDiskFreeSpace,
	.GetFileInformation = DIMGetFileInformation,
	.GetVolumeInformation = DIMGetVolumeInformation,
	.OpenDirectory = DIMOpenDirectory,
	.ReadFile = DIMReadFile,
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
	if(image_size.QuadPart == 0) {
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
	image.View.Memory = MapViewOfFile(image_map, FILE_MAP_READ, 0, 0, 0);
	image.View.Size = image_size.QuadPart;
	W32_ERR_REPORT(
		!image.View.Memory, -6, L"Error mapping %s into memory", ImageFN
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
#ifdef _DEBUG
		.Options = DOKAN_OPTION_DEBUG,
		.Timeout = -1,
#endif
		.GlobalContext = (ULONG64)fs_to_mount,
	};
	ret = DokanMain(&options, &operations);

end:
	UnmapViewOfFile(image.View.Memory);
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
