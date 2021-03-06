#include "vapor/glutil.h"
#include "vapor/FontManager.h"

using namespace VAPoR;

using std::string;
using std::map;
using std::pair;

FontManager::FontManager(GLManager *glManager)
: _glManager(glManager), _library(nullptr)
{
    assert(glManager);
    assert(!FT_Init_FreeType(&_library));
}

FontManager::~FontManager()
{
    for (auto it = _map.begin(); it != _map.end(); ++it)
        delete it->second;
    _map.clear();
    if (_library)
        FT_Done_FreeType(_library);
}

#ifdef _WINDOWS
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

Font *FontManager::GetFont(const std::string &name, unsigned int size)
{
    return GetResource(pair<string, unsigned int>(name, size));
}

int FontManager::LoadResourceByKey(const std::pair<std::string, unsigned int> &key)
{
    if (HasResource(key)) {
        assert(!"Font already loaded");
        return -1;
    }
    const string name = key.first;
    const unsigned int size = key.second;
    const string path = _resourceDirectory + PATH_SEPARATOR + name + ".ttf";
    Font *f = new Font(_glManager, path, size, _library);
    AddResource(key, f);
    return 1;
}
