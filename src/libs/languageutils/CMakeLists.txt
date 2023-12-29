add_qtc_library(LanguageUtils
  PUBLIC_DEPENDS Qt5::Core
  SOURCES
    componentversion.cpp componentversion.h
    fakemetaobject.cpp fakemetaobject.h
    languageutils_global.h
  OS2_SHORT_NAME LangUtl
)
