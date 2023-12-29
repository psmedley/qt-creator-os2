#include "yaml-cpp/exceptions.h"
#ifdef __OS2__
#define DllExport   __declspec( dllexport )
#else
#define DllExport
#endif

#define YAML_CPP_NOEXCEPT noexcept

namespace YAML {

// These destructors are defined out-of-line so the vtable is only emitted once.
DllExport Exception::~Exception() YAML_CPP_NOEXCEPT {}
DllExport ParserException::~ParserException() YAML_CPP_NOEXCEPT {}
DllExport RepresentationException::~RepresentationException() YAML_CPP_NOEXCEPT {}
DllExport InvalidScalar::~InvalidScalar() YAML_CPP_NOEXCEPT {}
DllExport KeyNotFound::~KeyNotFound() YAML_CPP_NOEXCEPT {}
DllExport InvalidNode::~InvalidNode() YAML_CPP_NOEXCEPT {}
DllExport BadConversion::~BadConversion() YAML_CPP_NOEXCEPT {}
DllExport BadDereference::~BadDereference() YAML_CPP_NOEXCEPT {}
DllExport BadSubscript::~BadSubscript() YAML_CPP_NOEXCEPT {}
DllExport BadPushback::~BadPushback() YAML_CPP_NOEXCEPT {}
DllExport BadInsert::~BadInsert() YAML_CPP_NOEXCEPT {}
DllExport EmitterException::~EmitterException() YAML_CPP_NOEXCEPT {}
DllExport BadFile::~BadFile() YAML_CPP_NOEXCEPT {}
}

#undef YAML_CPP_NOEXCEPT


