#include "FileDialog.h"

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>

std::string FileDialog::OpenFile(const char* filter) {
    char filename[MAX_PATH] = { 0 };
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = filename;
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        return std::string(filename);
    }
    return "";
}
#else
std::string FileDialog::OpenFile(const char* filter) {
    return "";
}
#endif
