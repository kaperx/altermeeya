#include "stdafx.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <string>
#include <cwchar>
#include <algorithm>
#include <windows.h>
#include <objbase.h>
#include "md5/md5.hh"
#include "profile.h"

void logErrorV(_TCHAR *format, va_list args)
{
	FILE *file = stderr;
	fwprintf(file, _T("error: "));
	vfwprintf(file, format, args);
	fwprintf(file, _T("\n"));
}

void logError(_TCHAR *format...)
{
	va_list args;
	va_start(args, format);
	logErrorV(format, args);
	va_end(args);
}

void die(_TCHAR *format...)
{
	va_list args;
	va_start(args, format);
	logErrorV(format, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

static std::wstring getFileNameExtension(const std::wstring &path)
{
	// TODO: handle *.tar.gz
	std::wstring::size_type pos = path.find_last_of(_T('.'));
	if (pos == std::wstring::npos)
		return _T("");
	return path.substr(pos);
}

static std::wstring getFileName(const std::wstring &path)
{
	std::wstring::size_type pos = path.find_last_of(_T("/\\"));
	if (pos == std::wstring::npos)
		return _T("");
	return path.substr(pos+1);
}

class HardLink
{
	std::wstring target_;

	HardLink(const HardLink &);
	HardLink& operator=(const HardLink &);

public:
	HardLink(const std::wstring &source, const std::wstring &target)
		: target_(target)
	{
		CreateHardLink(target.c_str(), source.c_str(), NULL);
	}
	~HardLink()
	{
		DeleteFile(target_.c_str());
	}
};

template<typename T, typename AllocT = std::allocator<T> >
class ScopedArray
{

public:
	typedef T value_type;
    typedef AllocT allocator_type;
    typedef typename allocator_type::size_type size_type;
private:
	T *data_;
	size_type size_;
    allocator_type alloc_;
public:

	ScopedArray(const ScopedArray&);
	ScopedArray& operator=(const ScopedArray&);

	ScopedArray(size_type size)
		: size_(size)
	{
        data_ = alloc_.allocate(size);
	}

	ScopedArray(T const *array, size_type size)
		: size_(size)
	{
        data_ = alloc_.allocate(size);
        std::copy(array, array + size, data_);
	}

	~ScopedArray()
	{
        alloc_.deallocate(data_, size_);
	}

    void reset(size_type size)
    {
        alloc_.deallocate(data_, size_);
        size_ = size;
        data_ = new T[size];
    }

    void reset(T const *array, size_type size)
    {
        delete[] data_;
        size_ = size;
        data_ = new T[size];
        std::copy(array, array + size, data_);
    }

	T *data() { return data_; }
	T const *data() const { return data_; }

	T& operator[](size_type n) { return data_[n]; }
	T const& operator[](size_type n) const { return data_[n]; }

	size_type size() const { return size_; }
};

bool isHardLinkSupported(_TCHAR driver)
{
	_TCHAR rootPath[10];
	swprintf(rootPath, 10, _T("%c:\\"), driver);
	_TCHAR volNameBuf[MAX_PATH+1];
	_TCHAR fileSystemNameBuf[MAX_PATH+1];

	DWORD dwVolSerialNumber = 0;
	DWORD dwMaxComponentLength = 0;
	DWORD dwFileSystemFlags = 0;
	BOOL val = GetVolumeInformation(rootPath, volNameBuf, MAX_PATH+1, 
		&dwVolSerialNumber, &dwMaxComponentLength, &dwFileSystemFlags,
		fileSystemNameBuf, MAX_PATH+1);
	//return (dwFileSystemFlags & FILE_SUPPORTS_HARD_LINKS) != 0;
    return 0 == wcscmp(fileSystemNameBuf, _T("NTFS"));
}

static void appendFileAttributes(__in LPCTSTR lpFileName, __in DWORD dwFileAttributes)
{
	DWORD oldAttributes = GetFileAttributes(lpFileName);
	SetFileAttributes(lpFileName, oldAttributes | dwFileAttributes);
}

// all hard links on the given driver will be created in this folder
static std::wstring createMountFolder(_TCHAR driver, _TCHAR *folderName)
{
	std::wstring folder;
	folder += driver;
	folder += _T(":\\");
	folder += folderName;

	CreateDirectory(folder.c_str(), NULL);
	// TODO : check folder existence
	appendFileAttributes(folder.c_str(), FILE_ATTRIBUTE_HIDDEN);

	return folder;
}

static void logSystemError( LSTATUS status ) 
{
	_TCHAR buffer[1024];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, status, 0, buffer, 1024, NULL);
	_TCHAR *msg = buffer;
	logError(_T("error %d: %s"), status, msg);
}

static LSTATUS setRegistry(const std::wstring &applicationPath)
{
    HKEY hkResult = 0;
    DWORD dwDisposition = 0;
    LSTATUS status = RegCreateKeyEx(HKEY_CLASSES_ROOT, _T("*\\shell\\AlterMeeya\\command"), 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkResult, &dwDisposition);
    if (status != ERROR_SUCCESS) {
		logSystemError(status);
        return status;
    }
    std::wstring command = _T("\"");
    command += applicationPath;
    command += _T("\" \"%1\"");
    status = RegSetKeyValue(hkResult, NULL, NULL, REG_SZ, command.c_str(), (1+command.size()) * sizeof(std::wstring::value_type));
    if (status != ERROR_SUCCESS) {
		logSystemError(status);
		RegCloseKey(hkResult);
        return status;
    }
    RegCloseKey(hkResult);
    return ERROR_SUCCESS;
}

std::wstring getApplicationPath()
{
    _TCHAR buffer[2048];
    DWORD len = GetModuleFileName(0, buffer, 2048);
    return buffer;
}

std::wstring getDirectoryFromPath(const std::wstring &path)
{
	std::wstring::size_type pos = path.find_last_of(_T("/\\"));
	if (pos == std::wstring::npos)
		return _T("");
	return path.substr(0, pos);
}

std::wstring getProfilePath(const std::wstring &fileName)
{
    std::wstring appPath = getApplicationPath();
    std::wstring dir = getDirectoryFromPath(appPath);
    return dir + _T('\\') + fileName;
}

std::wstring removeInvalidCharacters(const std::wstring &text)
{
	ScopedArray<char> acp_buffer(text.size() * 2);
	int ret = ::WideCharToMultiByte(GetACP(), 0, text.c_str(), -1, acp_buffer.data(), acp_buffer.size(), NULL, NULL);
	if (!ret) {
		DWORD err = GetLastError();
		switch (err) {
		case ERROR_INSUFFICIENT_BUFFER:
			die(_T("A supplied buffer size was not large enough, or it was incorrectly set to NULL."));
			break;
		case ERROR_INVALID_FLAGS:
			die(_T("The values supplied for flags were not valid."));
			break;
		case ERROR_INVALID_PARAMETER:
			die(_T("Any of the parameter values was invalid."));
			break;
		case ERROR_NO_UNICODE_TRANSLATION:
			die(_T("Invalid Unicode was found in a string."));
			break;
		}
	}
    std::string result = acp_buffer.data();
    result.erase(std::remove(result.begin(), result.end(), '?'), result.end());

    acp_buffer.reset(result.c_str(), result.size());
	ScopedArray<_TCHAR> wcsBuffer(text.size()+20);
    ret = ::MultiByteToWideChar(GetACP(), 0, acp_buffer.data(), acp_buffer.size(), wcsBuffer.data(), wcsBuffer.size());
	if (!ret) {
		DWORD err = GetLastError();
		switch (err) {
		case ERROR_INSUFFICIENT_BUFFER:
			die(_T("A supplied buffer size was not large enough, or it was incorrectly set to NULL."));
			break;
		case ERROR_INVALID_FLAGS:
			die(_T("The values supplied for flags were not valid."));
			break;
		case ERROR_INVALID_PARAMETER:
			die(_T("Any of the parameter values was invalid."));
			break;
		case ERROR_NO_UNICODE_TRANSLATION:
			die(_T("Invalid Unicode was found in a string."));
			break;
		}
	}
    wcsBuffer[ret] = _T('\0');
    return wcsBuffer.data();
}

std::wstring locateMangaMeeya()
{
    const int bufferSize = 2048;
    WCHAR filename[bufferSize];
    filename[0] = _T('\0');

    WCHAR filetitle[bufferSize];

    OPENFILENAME ofn = {
        sizeof OPENFILENAME,
        NULL,
        NULL,
        _T("Executable Files\0*.exe\0\0"),
        NULL,
        0,
        1, // filter index
        filename, bufferSize,
        filetitle, bufferSize,
        NULL, // initial dir
        _T("Locate MangaMeeya"), // title
        OFN_ENABLESIZING | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR,
        0,
        0,
        NULL, // default extension
        0, NULL, NULL, NULL, 0, 0
    };
    BOOL bResult = GetOpenFileName(&ofn);
    if (!bResult)
        return _T("");
    return filename;
}

int _tmain(int argc, _TCHAR* argv[])
{
    if (argc == 1) {
        LSTATUS status = setRegistry(getApplicationPath());
        if (status != ERROR_SUCCESS) {
            _TCHAR buffer[1024];
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, status, 0, buffer, 1024, NULL);
            wcscat_s(buffer, 1024, _T("\nError occurred when writing to registry."));
            if (status == 5) {
                wcscat_s(buffer, 1024, _T("\nHave you tried running as administrator?"));
            }
            MessageBox(NULL, buffer, _T("Error"), 0);
        }
        else {
            MessageBox(NULL, _T("Successfully written to registry"), _T("Success"), 0);
        }
        return status;
    }

	if (argc != 2)
		die(_T("Require exactly one argument"));

	std::wstring path = argv[1];
	if (path.size() < 3)
		die(_T("Path too short"));
	if (path[1] != ':')
		die(_T("Require absolute path"));

    // get the path to MangeMeeya.exe
    Profile profile(getProfilePath(_T("altermeeya.ini")));
    Profile::Section section(profile, _T("General"));
    std::wstring mangaMeeyaPath = section.getString(_T("MangaMeeya"));
    if (mangaMeeyaPath.empty()) {
        mangaMeeyaPath = locateMangaMeeya();
        if (!mangaMeeyaPath.empty()) {
            section.writeString(_T("MangaMeeya"), mangaMeeyaPath);
        }
        else {
            die(_T("Cannot locate MangaMeeya.exe"));
        }
    }

    std::wstring validPath = removeInvalidCharacters(path);
    if (validPath == path) {
        // the original path does not contain invalid characters, so we just use the original path
        STARTUPINFO startupInfo;
        PROCESS_INFORMATION procInfo;
        ZeroMemory(&startupInfo, sizeof startupInfo);
        startupInfo.cb = sizeof startupInfo;
        _TCHAR cmdLine[4096];
        wsprintf(cmdLine, _T("\"%s\" \"%s\""), mangaMeeyaPath.c_str(), path.c_str());
        BOOL succ = CreateProcessW(mangaMeeyaPath.c_str(), cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &procInfo);
        if (!succ) {
            DWORD err = GetLastError();
            die(_T("Failed to create process"));
        }
        CloseHandle(procInfo.hProcess);
        CloseHandle(procInfo.hThread);
        return 0;
    }

	_TCHAR driver = path[0];

	if (!isHardLinkSupported(driver)) {
		die(_T("Hard link not supported on driver %c:\\"), driver);
	}

	ScopedArray<char> utf8_buffer(path.size() * 3);
	int ret = ::WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, utf8_buffer.data(), utf8_buffer.size(), NULL, NULL);
	if (!ret) {
		DWORD err = GetLastError();
		switch (err) {
		case ERROR_INSUFFICIENT_BUFFER:
			die(_T("A supplied buffer size was not large enough, or it was incorrectly set to NULL."));
			break;
		case ERROR_INVALID_FLAGS:
			die(_T("The values supplied for flags were not valid."));
			break;
		case ERROR_INVALID_PARAMETER:
			die(_T("Any of the parameter values was invalid."));
			break;
		case ERROR_NO_UNICODE_TRANSLATION:
			die(_T("Invalid Unicode was found in a string."));
			break;
		}
	}
    MD5 md5;
    md5.update((unsigned char*)utf8_buffer.data(), utf8_buffer.size());
    md5.finalize();
    char *digest = md5.hex_digest();

	ScopedArray<_TCHAR> wcsDigestBuffer(40);
    ret = ::MultiByteToWideChar(CP_UTF8, 0, digest, -1, wcsDigestBuffer.data(), wcsDigestBuffer.size());
	if (!ret) {
		DWORD err = GetLastError();
		switch (err) {
		case ERROR_INSUFFICIENT_BUFFER:
			die(_T("A supplied buffer size was not large enough, or it was incorrectly set to NULL."));
			break;
		case ERROR_INVALID_FLAGS:
			die(_T("The values supplied for flags were not valid."));
			break;
		case ERROR_INVALID_PARAMETER:
			die(_T("Any of the parameter values was invalid."));
			break;
		case ERROR_NO_UNICODE_TRANSLATION:
			die(_T("Invalid Unicode was found in a string."));
			break;
		}
	}

	std::wstring alterpath = createMountFolder(driver, _T(".altermeeya"));
	alterpath += _T("\\");
	alterpath += wcsDigestBuffer.data();
	alterpath += _T("_");
	alterpath += removeInvalidCharacters(getFileName(path));

	HardLink hlink(path, alterpath);

    STARTUPINFO startupInfo;
    PROCESS_INFORMATION procInfo;
    ZeroMemory(&startupInfo, sizeof startupInfo);
    startupInfo.cb = sizeof startupInfo;
    _TCHAR cmdLine[4096];
    wsprintf(cmdLine, _T("\"%s\" \"%s\""), mangaMeeyaPath.c_str(), alterpath.c_str());

    BOOL succ = CreateProcessW(mangaMeeyaPath.c_str(), cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &procInfo);
    if (!succ) {
        DWORD err = GetLastError();
        die(_T("Failed to create process"));
    }
    WaitForSingleObject(procInfo.hProcess, INFINITE);
    CloseHandle(procInfo.hProcess);
    CloseHandle(procInfo.hThread);

	return 0;
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
    return _tmain(__argc, __targv);
}
