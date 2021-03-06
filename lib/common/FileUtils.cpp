#include "vapor/FileUtils.h"

#ifdef WIN32
#else
#include <sys/stat.h>
#include <libgen.h>
#endif

using namespace VAPoR;
using FileUtils::FileType;
using std::string;

string FileUtils::ReadFileToString(const string &path)
{
    FILE *f = fopen(path.c_str(), "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long length = ftell(f);
        rewind(f);
        char *buf = new char[length + 1];
        fread(buf, length, 1, f);
        buf[length] = 0;
        string ret(buf);
        delete [] buf;
        return ret;
    } else {
        return "";
    }
}

std::string FileUtils::Basename(const std::string &path)
{
#ifdef WIN32
    return path;
#else
    char *copy = new char[path.length()+1];
    strcpy(copy, path.c_str());
    string ret(basename(copy));
    delete [] copy;
    return ret;
#endif
}

long FileUtils::GetFileModifiedTime(const string &path)
{
	struct stat attrib;
    stat(path.c_str(), &attrib);
    return attrib.st_mtime;
}

bool FileUtils::FileExists(const std::string &path)
{
    return FileUtils::GetFileType(path) != FileType::Does_Not_Exist;
}

bool FileUtils::IsRegularFile(const std::string &path)
{
    return FileUtils::GetFileType(path) == FileType::File;
}

bool FileUtils::IsDirectory(const std::string &path)
{
    return FileUtils::GetFileType(path) == FileType::Directory;
}

FileType FileUtils::GetFileType(const std::string &path)
{
    struct stat s;
    if (stat(path.c_str(), &s) == 0) {
        if (s.st_mode & S_IFDIR)
            return FileType::Directory;
        else if (s.st_mode & S_IFREG)
            return FileType::File;
        else
            return FileType::Other;
    }
    else
        return FileType::Does_Not_Exist;
}
