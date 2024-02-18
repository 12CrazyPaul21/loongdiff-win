module;

#include <string>
#include <set>
#include <cstdint>
#include <cassert>
#include <filesystem>

#include <Windows.h>
#include <ShlObj_core.h>
#include <Shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")

export module file;

#define RELEASE_HADNLE(H)                                                                                              \
    if (H) {                                                                                                           \
        CloseHandle(H);                                                                                                \
        H = NULL;                                                                                                      \
    }

export namespace fp {

enum class PathFlag
{
    NOT_EXISTS,
    FILE,
    FOLDER,
};

//
// File Class
//

class File
{
public:
    File()            = default;
    File(const File&) = delete;
    File(File&&)      = delete;
    ~File() { Close(); }

    bool Open(const std::string& path, bool fReadOnly);
    bool Create(const std::string& path, uint64_t size, bool fOverwrite = false);
    void Delete();
    void Close();

    bool Append(const std::string& path, uint64_t maxSize);
    bool Append(const uint8_t* pSource, uint64_t size, uint64_t maxSize);
    bool Write(uint8_t* pTarget, const uint8_t* pSource, uint64_t size);

    inline uint8_t*       Ptr() const { return (m_fOpened && !m_fReadOnly) ? m_pMapping : nullptr; }
    inline const uint8_t* ReadPtr() const { return m_fOpened ? m_pMapping : nullptr; }

    inline bool           IsReadOnly() const { return m_fReadOnly; }
    inline bool           IsOpened() const { return m_fOpened; }
    inline std::string    GetFullPath() const { return m_fullPath; }
    inline std::string    GetParentPath() const { return m_folderPath; }
    inline std::uintmax_t GetFileSize() const { return m_fileSize; }

    static std::string    GetApplicationFolder(const std::string& app);
    static std::string    GetFullPath(const std::string& path);
    static std::string    GetParentPath(const std::string& path);
    static std::string    GetFileName(const std::string& fullPath);
    static std::uintmax_t GetFileSize(const std::string& path);
    static std::uintmax_t CompareFileSize(const std::string& l, const std::string& r);
    static std::string    CombinePath(const std::string& parent, const std::string& sub);
    static PathFlag       CheckPathFlag(const std::string& path);
    static bool           IsFile(const std::string& path) { return CheckPathFlag(path) == PathFlag::FILE; }
    static bool           IsPathExists(const std::string& path);
    static bool           IsFolderEmpty(const std::string& path);
    static bool           Mkdir(const std::string& folder);
    static bool           MakeFilesFolder(const std::string& file);
    static bool           HidePath(const std::string& path);
    static bool           Copy(const std::string& src, const std::string& target);
    static bool           Remove(const std::string& path);
    static bool           OverWrite(const std::string& src, const std::string& target);
    static bool ListFolder(const std::string& folder, const std::string& subFolder, std::set<std::string>& stFolders,
                           std::set<std::string>& stFiles);

    static bool ExportFile(const std::string& target, const uint8_t* pSource, uint64_t size);
    static bool ExportResource(const std::string& folder, const std::string& fileName, const std::string& resourceName,
                               const std::string& resourceType);

private:
    bool map_view();
    void unmap_view();

private:
    std::string m_fullPath;
    std::string m_folderPath;
    bool        m_fReadOnly{};
    bool        m_fOpened{};
    HANDLE      m_hFile{};
    HANDLE      m_hFileMapping{};
    uint8_t*    m_pMapping{};
    uint64_t    m_fileSize{};
};

/**
 * @brief can't open empty file
 */
bool File::Open(const std::string& path, bool fReadOnly)
{
    char          full[MAX_PATH] = {0};
    LARGE_INTEGER fileSize       = {0};

    if (m_fOpened) {
        return false;
    }

    m_fReadOnly = fReadOnly;

    if (!GetFullPathNameA(path.c_str(), MAX_PATH, full, nullptr)) {
        return false;
    }

    if (!PathFileExistsA(full) || PathIsDirectoryA(full)) {
        return false;
    }

    m_hFile = CreateFileA(full, m_fReadOnly ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (m_hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    if (!GetFileSizeEx(m_hFile, &fileSize)) {
        RELEASE_HADNLE(m_hFile);
        return false;
    }

    if (!map_view()) {
        RELEASE_HADNLE(m_hFile);
        return false;
    }

    m_fullPath   = full;
    m_folderPath = std::filesystem::path(full).parent_path().string();
    m_fileSize   = fileSize.QuadPart;
    m_fOpened    = true;

    return true;
}

bool File::Create(const std::string& path, uint64_t size, bool fOverwrite)
{
    char          full[MAX_PATH] = {0};
    std::string   folderPath;
    LARGE_INTEGER targetSize = {0};

    if (m_fOpened) {
        return false;
    }

    if (!GetFullPathNameA(path.c_str(), MAX_PATH, full, nullptr)) {
        return false;
    }

    if (PathFileExistsA(full)) {
        if (!fOverwrite || PathIsDirectoryA(full) || !DeleteFileA(full)) {
            return false;
        }
    }

    folderPath = std::filesystem::path(full).parent_path().string();
    if (!PathIsDirectoryA(folderPath.c_str())) {
        if (SHCreateDirectoryExA(NULL, folderPath.c_str(), nullptr) != ERROR_SUCCESS) {
            return false;
        }
    }

    m_hFile = CreateFileA(full, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                          FILE_ATTRIBUTE_NORMAL, nullptr);
    if (m_hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    targetSize.QuadPart = size;
    if (!SetFilePointerEx(m_hFile, targetSize, nullptr, FILE_BEGIN) || !SetEndOfFile(m_hFile)) {
        RELEASE_HADNLE(m_hFile);
        DeleteFileA(full);
        return false;
    }

    m_fReadOnly = false;

    if (!map_view()) {
        RELEASE_HADNLE(m_hFile);
        DeleteFileA(full);
        return false;
    }

    m_fullPath   = full;
    m_folderPath = folderPath;
    m_fileSize   = size;
    m_fOpened    = true;

    return true;
}

void File::Delete()
{
    if (!m_fOpened) {
        return;
    }

    unmap_view();
    RELEASE_HADNLE(m_hFile);

    m_fOpened = false;

    DeleteFileA(m_fullPath.c_str());
}

void File::Close()
{
    if (!m_fOpened) {
        return;
    }

    unmap_view();
    RELEASE_HADNLE(m_hFile);

    m_fOpened = false;
}

bool File::Append(const std::string& path, uint64_t maxSize)
{
    File source;

    if (!m_fOpened || m_fReadOnly) {
        return false;
    }

    if (!source.Open(path, true)) {
        return false;
    }

    return Append(source.ReadPtr(), source.GetFileSize(), maxSize);
}

bool File::Append(const uint8_t* pSource, uint64_t size, uint64_t maxSize)
{
    LARGE_INTEGER newFileSize;

    if (!m_fOpened || m_fReadOnly) {
        return false;
    }

    newFileSize.QuadPart = m_fileSize + size;
    if (newFileSize.QuadPart >= (LONGLONG)maxSize) {
        return false;
    }

    //
    // adjust PE file size
    //

    unmap_view();

    if (!SetFilePointerEx(m_hFile, newFileSize, nullptr, FILE_BEGIN) || !SetEndOfFile(m_hFile)) {
        map_view();
        return false;
    }

    map_view();

    // append source file bytes
    if (memcpy_s(m_pMapping + m_fileSize, size, pSource, size)) {
        return false;
    }

    m_fileSize = newFileSize.QuadPart;

    return true;
}

bool File::Write(uint8_t* pTarget, const uint8_t* pSource, uint64_t size)
{
    if (!m_fOpened || m_fReadOnly) {
        return false;
    }

    if (!pTarget || !pSource || !size) {
        return false;
    }

    return !memcpy_s(pTarget, size, pSource, size);
}

bool File::map_view()
{
    m_hFileMapping = CreateFileMappingA(m_hFile, nullptr, m_fReadOnly ? PAGE_READONLY : PAGE_READWRITE, 0, 0, nullptr);
    if (!m_hFileMapping) {
        return false;
    }

    m_pMapping = (uint8_t*)MapViewOfFile(m_hFileMapping, FILE_MAP_READ | (m_fReadOnly ? 0 : FILE_MAP_WRITE), 0, 0, 0);
    if (!m_pMapping) {
        RELEASE_HADNLE(m_hFileMapping);
        return false;
    }

    return true;
}

void File::unmap_view()
{
    if (m_pMapping) {
        UnmapViewOfFile(m_pMapping);
        m_pMapping = nullptr;
    }

    RELEASE_HADNLE(m_hFileMapping);
}

std::string File::GetApplicationFolder(const std::string& app)
{
    char result[MAX_PATH]  = {0};
    char appdata[MAX_PATH] = {0};

    if (!SHGetSpecialFolderPathA(NULL, appdata, CSIDL_APPDATA, TRUE)) {
        return "";
    }

    if (!PathCombineA(result, appdata, app.c_str())) {
        return "";
    }

    if (PathFileExistsA(result)) {
        if (!PathIsDirectoryA(result)) {
            return "";
        }
        return result;
    }

    if (SHCreateDirectoryExA(NULL, result, nullptr) != ERROR_SUCCESS) {
        return "";
    }

    return result;
}

std::string File::GetFullPath(const std::string& path)
{
    char full[MAX_PATH] = {0};

    if (!GetFullPathNameA(path.c_str(), MAX_PATH, full, nullptr)) {
        return path;
    }

    return full;
}

std::string File::GetParentPath(const std::string& path) { return std::filesystem::path(path).parent_path().string(); }

std::string File::GetFileName(const std::string& fullPath)
{
    return std::filesystem::path(fullPath).filename().string();
}

std::uintmax_t File::GetFileSize(const std::string& path)
{
    std::error_code code;

    if (!PathFileExistsA(path.c_str())) {
        return 0;
    }

    return std::filesystem::file_size(path, code);
}

std::uintmax_t File::CompareFileSize(const std::string& l, const std::string& r)
{
    std::error_code code;
    return std::filesystem::file_size(l, code) - std::filesystem::file_size(r, code);
}

std::string File::CombinePath(const std::string& parent, const std::string& sub)
{
    char result[MAX_PATH] = {0};

    if (parent.empty()) {
        return sub;
    }

    if (!PathCombineA(result, parent.c_str(), sub.c_str())) {
        return "";
    }

    return result;
}

PathFlag File::CheckPathFlag(const std::string& path)
{
    if (!PathFileExistsA(path.c_str())) {
        return PathFlag::NOT_EXISTS;
    }

    if (!PathIsDirectoryA(path.c_str())) {
        return PathFlag::FILE;
    }

    return PathFlag::FOLDER;
}

bool File::IsPathExists(const std::string& path)
{
    if (path.empty()) {
        return false;
    }

    return PathFileExistsA(path.c_str());
}

bool File::IsFolderEmpty(const std::string& path)
{
    std::error_code code;
    return std::filesystem::is_empty(path, code);
}

bool File::Mkdir(const std::string& folder)
{
    if (folder.empty()) {
        return false;
    }

    if (PathIsDirectoryA(folder.c_str())) {
        return true;
    }

    return SHCreateDirectoryExA(NULL, folder.c_str(), nullptr) == ERROR_SUCCESS;
}

bool File::MakeFilesFolder(const std::string& file)
{
    if (file.empty()) {
        return false;
    }

    return Mkdir(std::filesystem::path(file).parent_path().string());
}

bool File::HidePath(const std::string& path)
{
    if (path.empty()) {
        return false;
    }

    return SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_HIDDEN);
}

bool File::Copy(const std::string& src, const std::string& target)
{
    using std::filesystem::copy_options;

    MakeFilesFolder(target);

    std::error_code err;
    std::filesystem::copy(src, target, copy_options::overwrite_existing | copy_options::recursive, err);

    return !err.value();
}

bool File::Remove(const std::string& path)
{
    std::error_code code;
    return std::filesystem::remove_all(path, code) > 0;
}

bool File::OverWrite(const std::string& src, const std::string& target)
{
    if (src.empty() || target.empty()) {
        return false;
    }

    if (IsPathExists(target)) {
        if (!Remove(target)) {
            return false;
        }
    }

    std::error_code code;
    std::filesystem::rename(src, target, code);

    return !code;
}

bool File::ListFolder(const std::string& folder, const std::string& subFolder, std::set<std::string>& stFolders,
                      std::set<std::string>& stFiles)
{
    char             fullPath[MAX_PATH] = {0};
    char             buffer[MAX_PATH]   = {0};
    char*            ptr                = nullptr;
    WIN32_FIND_DATAA findFileData       = {0};
    HANDLE           hFind              = NULL;

    if (!PathIsDirectoryA(folder.c_str())) {
        return false;
    }

    if (strcpy_s(fullPath, folder.c_str())) {
        return false;
    }

    if (!subFolder.empty() && !PathCombineA(fullPath, fullPath, subFolder.c_str())) {
        return false;
    }

    if (!PathCombineA(buffer, fullPath, R"(*.*)")) {
        return false;
    }

    hFind = ::FindFirstFileA(buffer, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }

    do {
        if (!strcmp(findFileData.cFileName, ".") || !strcmp(findFileData.cFileName, "..")) {
            continue;
        }

        if (!subFolder.empty()) {
            if (!PathCombineA(buffer, subFolder.c_str(), findFileData.cFileName)) {
                FindClose(hFind);
                return false;
            }

            ptr = buffer;
        }
        else {
            ptr = findFileData.cFileName;
        }

        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            stFolders.emplace(std::string(ptr));
        }
        else {
            stFiles.emplace(std::string(ptr));
        }
    } while (::FindNextFileA(hFind, &findFileData));

    FindClose(hFind);

    return true;
}

bool File::ExportFile(const std::string& target, const uint8_t* pSource, uint64_t size)
{
    File file;

    if (!file.Create(target, size, true)) {
        return false;
    }

    return file.Write(file.Ptr(), pSource, size);
}

bool File::ExportResource(const std::string& folder, const std::string& fileName, const std::string& resourceName,
                          const std::string& resourceType)
{
    bool    result;
    File    file;
    char    fullPath[MAX_PATH] = {0};
    HMODULE hModule            = NULL;
    HRSRC   hResource          = NULL;
    HGLOBAL hResData           = NULL;
    LPVOID  lpResData          = nullptr;
    DWORD   dwResData          = 0;

    if (!PathCombineA(fullPath, folder.c_str(), fileName.c_str())) {
        return false;
    }

    if (PathFileExistsA(fullPath)) {
        //if (!DeleteFileA(fullPath)) {
        //    return false;
        //}
        return true;
    }

    hModule = GetModuleHandleA(NULL);
    if (!hModule) {
        return false;
    }

    hResource = FindResourceA(hModule, resourceName.c_str(), resourceType.c_str());
    if (!hResource) {
        return false;
    }

    dwResData = SizeofResource(hModule, hResource);
    if (dwResData == 0) {
        return false;
    }

    hResData = LoadResource(hModule, hResource);
    if (!hResData) {
        return false;
    }

    lpResData = LockResource(hResData);
    if (!lpResData) {
        FreeResource(hResData);
        return false;
    }

    result = ExportFile(fullPath, (uint8_t*)lpResData, dwResData);

    UnlockResource(lpResData);
    FreeResource(hResData);

    return result;
}

//
// Class PatchPE
//

class PatchPE final
{
public:
    PatchPE();
    PatchPE(bool fReadOnly);
    PatchPE(const std::string& path, bool fReadOnly = true);
    PatchPE(const PatchPE&) = delete;
    PatchPE(PatchPE&&)      = delete;
    ~PatchPE() {}

    inline std::string GetFullPath() const { return m_file.GetFullPath(); }
    inline std::string GetFolderPath() const { return m_file.GetParentPath(); }
    inline bool        IsReadOnly() const { return m_file.IsReadOnly(); }
    inline bool        IsOpened() const { return m_file.IsOpened(); }
    inline bool        IsValidPE() const { return m_fValid; }
    inline bool        IsHasPatch() const { return m_fPatch; }
    inline uint64_t    GetPEFileSize() const { return m_peFileSize; }
    inline uint64_t    GetFileSize() const { return m_file.GetFileSize(); }
    inline uint64_t    GetPatchOffset() const { return m_peFileSize; }
    inline uint64_t    GetPatchSize() const { return m_file.GetFileSize() - m_peFileSize; }

    bool AppendPatch(const std::string& path);
    bool ExtractPatch(const std::string& path);

private:
    void verify();

private:
    fp::File m_file;
    bool     m_fValid{};
    bool     m_fPatch{};
    uint64_t m_peFileSize{};
};

PatchPE::PatchPE()
  : PatchPE::PatchPE(true)
{}

PatchPE::PatchPE(bool fReadOnly)
{
    char path[MAX_PATH] = {0};
    if (!GetModuleFileNameA(NULL, path, MAX_PATH)) {
        return;
    }

    if (m_file.Open(path, fReadOnly)) {
        verify();
    }
}

PatchPE::PatchPE(const std::string& path, bool fReadOnly)
{
    if (m_file.Open(path, fReadOnly)) {
        verify();
    }
}

void PatchPE::verify()
{
    const uint8_t* ptr              = m_file.ReadPtr();
    const uint8_t* pCoffHeader      = nullptr;
    const uint8_t* pSectionTable    = nullptr;
    const uint8_t* pLastSectionItem = nullptr;
    uint16_t       sectionCount     = 0;

    // DOS Signature -> "MZ"
    if (*(uint16_t*)ptr != 0x5A4D) {
        return;
    }

    // COFF Header Pointer
    pCoffHeader = ptr + *(uint32_t*)(ptr + 0x3C);

    // PE Signature -> "PE"
    if (*(uint16_t*)(pCoffHeader) != 0x4550) {
        return;
    }

    pSectionTable    = pCoffHeader + 0x18 + *(uint16_t*)(pCoffHeader + 0x14);
    sectionCount     = *(uint16_t*)(pCoffHeader + 0x06);
    pLastSectionItem = pSectionTable + (sectionCount - 1) * 0x28;

    m_fValid     = false;
    m_peFileSize = *(uint32_t*)(pLastSectionItem + 0x10) + *(uint32_t*)(pLastSectionItem + 0x14);

    if (m_file.GetFileSize() > m_peFileSize) {
        //
        // 7z Magic -> "7z"
        //

        if (m_file.GetFileSize() - m_peFileSize <= 2) {
            // Invalid Patch Package
            return;
        }
        else if (*(uint16_t*)(ptr + m_peFileSize) != 0x7A37) {
            // Invalid Patch Package
            return;
        }

        m_fPatch = true;
    }

    m_fValid = true;
}

bool PatchPE::AppendPatch(const std::string& path)
{
    // full patch PE file size must <= 3.8GB
    // because's PE file size must <= 2^32B
    return m_file.Append(path, 0xF3333333);
}

bool PatchPE::ExtractPatch(const std::string& path)
{
    if (!m_file.IsOpened() || !m_fPatch) {
        return false;
    }

    return File::ExportFile(path, m_file.ReadPtr() + m_peFileSize, GetPatchSize());
}

}  // namespace fp