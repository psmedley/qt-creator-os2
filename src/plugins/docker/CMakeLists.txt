add_qtc_plugin(Docker
  DEPENDS Utils
  PLUGIN_DEPENDS Core ProjectExplorer QtSupport
  SOURCES
    docker_global.h
    dockerapi.cpp dockerapi.h
    dockerconstants.h
    dockerdevice.cpp dockerdevice.h
    dockerdevicewidget.cpp dockerdevicewidget.h
    dockerplugin.cpp dockerplugin.h
    dockersettings.cpp dockersettings.h
    kitdetector.cpp kitdetector.h
)
