# shared with tests

if(CMAKE_VERSION VERSION_LESS 3.17.0)
set(CMAKE_CURRENT_FUNCTION_LIST_DIR ${CMAKE_CURRENT_LIST_DIR})
endif()

function(extend_with_qmldesigner_core target_name)
  if(NOT TARGET ${target_name})
    return()
  endif()

  extend_qtc_target(${target_name}
    DEPENDS
      Threads::Threads
      Qt5::CorePrivate
      CPlusPlus
      Sqlite
      Utils
      Qt5::Widgets
      Qt5::Qml
      Core
      ProjectExplorer
      QmakeProjectManager
      QmlJS
      QmlJSEditor
      QmlJSTools
      QmlProjectManager
      QtSupport
      TextEditor
    INCLUDES
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/components
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/components/componentcore
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/components/debugview
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/components/edit3d
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/components/formeditor
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/components/integration
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/components/itemlibrary
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/components/navigator
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/components/propertyeditor
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/components/stateseditor
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/components/texteditor
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/designercore
    SOURCES_PREFIX ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/designercore
    SOURCES
      rewritertransaction.cpp
      rewritertransaction.h
  )

  # autouic gets confused when adding the ui files to tests in the qtquickdesigner repo,
  # so manually add them for UIC
  set(UI_FILES
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/designercore/instances/puppetbuildprogressdialog.ui
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/designercore/instances/puppetdialog.ui
  )
  qt_wrap_ui(UI_SOURCES ${UI_FILES})
  extend_qtc_target(${target_name}
    INCLUDES ${CMAKE_CURRENT_BINARY_DIR}
    SOURCES
      ${UI_SOURCES}
      ${UI_FILES}
  )
  set_source_files_properties(${UI_FILES} PROPERTIES SKIP_AUTOUIC ON)

  extend_qtc_target(${target_name}
    INCLUDES
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/designercore/exceptions
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/designercore/filemanager
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/designercore/imagecache
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/designercore/include
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/designercore/instances
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/designercore/metainfo
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/designercore/model
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/designercore/pluginmanager
    SOURCES_PREFIX ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/designercore
    SOURCES
      exceptions/exception.cpp
      exceptions/invalidargumentexception.cpp
      exceptions/invalididexception.cpp
      exceptions/invalidmetainfoexception.cpp
      exceptions/invalidmodelnodeexception.cpp
      exceptions/invalidmodelstateexception.cpp
      exceptions/invalidpropertyexception.cpp
      exceptions/invalidqmlsourceexception.cpp
      exceptions/invalidreparentingexception.cpp
      exceptions/invalidslideindexexception.cpp
      exceptions/notimplementedexception.cpp
      exceptions/removebasestateexception.cpp
      exceptions/rewritingexception.cpp

      filemanager/addarraymembervisitor.cpp
      filemanager/addarraymembervisitor.h
      filemanager/addobjectvisitor.cpp
      filemanager/addobjectvisitor.h
      filemanager/addpropertyvisitor.cpp
      filemanager/addpropertyvisitor.h
      filemanager/astobjecttextextractor.cpp
      filemanager/astobjecttextextractor.h
      filemanager/changeimportsvisitor.cpp
      filemanager/changeimportsvisitor.h
      filemanager/changeobjecttypevisitor.cpp
      filemanager/changeobjecttypevisitor.h
      filemanager/changepropertyvisitor.cpp
      filemanager/changepropertyvisitor.h
      filemanager/firstdefinitionfinder.cpp
      filemanager/firstdefinitionfinder.h
      filemanager/moveobjectbeforeobjectvisitor.cpp
      filemanager/moveobjectbeforeobjectvisitor.h
      filemanager/moveobjectvisitor.cpp
      filemanager/moveobjectvisitor.h
      filemanager/objectlengthcalculator.cpp
      filemanager/objectlengthcalculator.h
      filemanager/qmlrefactoring.cpp
      filemanager/qmlrefactoring.h
      filemanager/qmlrewriter.cpp
      filemanager/qmlrewriter.h
      filemanager/removepropertyvisitor.cpp
      filemanager/removepropertyvisitor.h
      filemanager/removeuiobjectmembervisitor.cpp
      filemanager/removeuiobjectmembervisitor.h

      imagecache/asynchronousexplicitimagecache.cpp
      imagecache/asynchronousimagecache.cpp
      imagecache/asynchronousimagefactory.cpp
      imagecache/asynchronousimagefactory.h
      imagecache/imagecachecollector.cpp
      imagecache/imagecachecollector.h
      imagecache/imagecachecollectorinterface.h
      imagecache/imagecacheconnectionmanager.cpp
      imagecache/imagecacheconnectionmanager.h
      imagecache/imagecachefontcollector.cpp
      imagecache/imagecachefontcollector.h
      imagecache/imagecachegenerator.cpp
      imagecache/imagecachegenerator.h
      imagecache/imagecachegeneratorinterface.h
      imagecache/imagecachestorage.h
      imagecache/imagecachestorageinterface.h
      imagecache/meshimagecachecollector.cpp
      imagecache/meshimagecachecollector.h
      imagecache/synchronousimagecache.cpp
      imagecache/timestampprovider.cpp
      imagecache/timestampprovider.h
      imagecache/timestampproviderinterface.h

      include/abstractproperty.h
      include/abstractview.h
      include/anchorline.h
      include/annotation.h
      include/asynchronousexplicitimagecache.h
      include/asynchronousimagecache.h
      include/basetexteditmodifier.h
      include/bindingproperty.h
      include/componenttextmodifier.h
      include/customnotifications.h
      include/documentmessage.h
      include/exception.h
      include/forwardview.h
      include/imagecacheauxiliarydata.h
      include/import.h
      include/invalidargumentexception.h
      include/invalididexception.h
      include/invalidmetainfoexception.h
      include/invalidmodelnodeexception.h
      include/invalidmodelstateexception.h
      include/invalidpropertyexception.h
      include/invalidqmlsourceexception.h
      include/invalidreparentingexception.h
      include/invalidslideindexexception.h
      include/itemlibraryinfo.h
      include/mathutils.h
      include/metainfo.h
      include/metainforeader.h
      include/model.h
      include/modelmerger.h
      include/modelnode.h
      include/modelnodepositionstorage.h
      include/nodeabstractproperty.h
      include/nodehints.h
      include/nodeinstance.h
      include/nodeinstanceview.h
      include/nodelistproperty.h
      include/nodemetainfo.h
      include/nodeproperty.h
      include/notimplementedexception.h
      include/plaintexteditmodifier.h
      include/propertycontainer.h
      include/propertynode.h
      include/propertyparser.h
      include/qmlanchors.h
      include/qmlchangeset.h
      include/qmlconnections.h
      include/qmldesignercorelib_global.h
      include/qmlitemnode.h
      include/qmlmodelnodefacade.h
      include/qmlobjectnode.h
      include/qmlstate.h
      include/qmltimeline.h
      include/qmltimelinekeyframegroup.h
      include/removebasestateexception.h
      include/rewriterview.h
      include/rewritingexception.h
      include/signalhandlerproperty.h
      include/stylesheetmerger.h
      include/subcomponentmanager.h
      include/synchronousimagecache.h
      include/textmodifier.h
      include/variantproperty.h
      include/viewmanager.h

      instances/baseconnectionmanager.cpp
      instances/baseconnectionmanager.h
      instances/connectionmanager.cpp
      instances/connectionmanager.h
      instances/connectionmanagerinterface.cpp
      instances/connectionmanagerinterface.h
      instances/nodeinstance.cpp
      instances/nodeinstanceserverproxy.cpp
      instances/nodeinstanceserverproxy.h
      instances/nodeinstanceview.cpp
      instances/puppetbuildprogressdialog.cpp
      instances/puppetbuildprogressdialog.h
      instances/puppetcreator.cpp
      instances/puppetcreator.h
      instances/puppetdialog.cpp
      instances/puppetdialog.h
      instances/qprocessuniqueptr.h

      metainfo/itemlibraryinfo.cpp
      metainfo/metainfo.cpp
      metainfo/metainforeader.cpp
      metainfo/nodehints.cpp
      metainfo/nodemetainfo.cpp
      metainfo/subcomponentmanager.cpp

      model/abstractproperty.cpp
      model/abstractview.cpp
      model/anchorline.cpp
      model/annotation.cpp
      model/bindingproperty.cpp
      model/componenttextmodifier.cpp
      model/documentmessage.cpp
      model/import.cpp
      model/internalbindingproperty.cpp
      model/internalbindingproperty.h
      model/internalnode.cpp
      model/internalnode_p.h
      model/internalnodeabstractproperty.cpp
      model/internalnodeabstractproperty.h
      model/internalnodelistproperty.cpp
      model/internalnodelistproperty.h
      model/internalnodeproperty.cpp
      model/internalnodeproperty.h
      model/internalproperty.cpp
      model/internalproperty.h
      model/internalsignalhandlerproperty.cpp
      model/internalsignalhandlerproperty.h
      model/internalvariantproperty.cpp
      model/internalvariantproperty.h
      model/model.cpp
      model/model_p.h
      model/modelmerger.cpp
      model/modelnode.cpp
      model/modelnodepositionrecalculator.cpp
      model/modelnodepositionrecalculator.h
      model/modelnodepositionstorage.cpp
      model/modeltotextmerger.cpp
      model/modeltotextmerger.h
      model/nodeabstractproperty.cpp
      model/nodelistproperty.cpp
      model/nodeproperty.cpp
      model/plaintexteditmodifier.cpp
      model/propertycontainer.cpp
      model/propertynode.cpp
      model/propertyparser.cpp
      model/qml3dnode.cpp
      model/qmlanchors.cpp
      model/qmlchangeset.cpp
      model/qmlconnections.cpp
      model/qmlitemnode.cpp
      model/qmlmodelnodefacade.cpp
      model/qmlobjectnode.cpp
      model/qmlstate.cpp
      model/qmltextgenerator.cpp
      model/qmltextgenerator.h
      model/qmltimeline.cpp
      model/qmltimelinekeyframegroup.cpp
      model/qmlvisualnode.cpp
      model/rewriteaction.cpp
      model/rewriteaction.h
      model/rewriteactioncompressor.cpp
      model/rewriteactioncompressor.h
      model/rewriterview.cpp
      model/signalhandlerproperty.cpp
      model/stylesheetmerger.cpp
      model/textmodifier.cpp
      model/texttomodelmerger.cpp
      model/texttomodelmerger.h
      model/variantproperty.cpp
      model/viewmanager.cpp

      pluginmanager/widgetpluginmanager.cpp
      pluginmanager/widgetpluginmanager.h
      pluginmanager/widgetpluginpath.cpp
      pluginmanager/widgetpluginpath.h
  )

  extend_qtc_target(${target_name}
    SOURCES_PREFIX ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../../share/qtcreator/qml/qmlpuppet
    INCLUDES
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../../share/qtcreator/qml/qmlpuppet/commands
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../../share/qtcreator/qml/qmlpuppet/container
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../../share/qtcreator/qml/qmlpuppet/interfaces
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../../share/qtcreator/qml/qmlpuppet/types
    SOURCES
      commands/captureddatacommand.h
      commands/changeauxiliarycommand.cpp
      commands/changeauxiliarycommand.h
      commands/changebindingscommand.cpp
      commands/changebindingscommand.h
      commands/changefileurlcommand.cpp
      commands/changefileurlcommand.h
      commands/changeidscommand.cpp
      commands/changeidscommand.h
      commands/changelanguagecommand.cpp
      commands/changelanguagecommand.h
      commands/changenodesourcecommand.cpp
      commands/changenodesourcecommand.h
      commands/changepreviewimagesizecommand.cpp
      commands/changepreviewimagesizecommand.h
      commands/changeselectioncommand.cpp
      commands/changeselectioncommand.h
      commands/changestatecommand.cpp
      commands/changestatecommand.h
      commands/changevaluescommand.cpp
      commands/changevaluescommand.h
      commands/childrenchangedcommand.cpp
      commands/childrenchangedcommand.h
      commands/clearscenecommand.cpp
      commands/clearscenecommand.h
      commands/completecomponentcommand.cpp
      commands/completecomponentcommand.h
      commands/componentcompletedcommand.cpp
      commands/componentcompletedcommand.h
      commands/createinstancescommand.cpp
      commands/createinstancescommand.h
      commands/createscenecommand.cpp
      commands/createscenecommand.h
      commands/debugoutputcommand.cpp
      commands/debugoutputcommand.h
      commands/endpuppetcommand.cpp
      commands/endpuppetcommand.h
      commands/informationchangedcommand.cpp
      commands/informationchangedcommand.h
      commands/nanotracecommand.cpp
      commands/nanotracecommand.h
      commands/inputeventcommand.cpp
      commands/inputeventcommand.h
      commands/pixmapchangedcommand.cpp
      commands/pixmapchangedcommand.h
      commands/puppetalivecommand.cpp
      commands/puppetalivecommand.h
      commands/puppettocreatorcommand.cpp
      commands/puppettocreatorcommand.h
      commands/removeinstancescommand.cpp
      commands/removeinstancescommand.h
      commands/removepropertiescommand.cpp
      commands/removepropertiescommand.h
      commands/removesharedmemorycommand.cpp
      commands/removesharedmemorycommand.h
      commands/reparentinstancescommand.cpp
      commands/reparentinstancescommand.h
      commands/requestmodelnodepreviewimagecommand.cpp
      commands/requestmodelnodepreviewimagecommand.h
      commands/scenecreatedcommand.h
      commands/statepreviewimagechangedcommand.cpp
      commands/statepreviewimagechangedcommand.h
      commands/synchronizecommand.cpp
      commands/synchronizecommand.h
      commands/tokencommand.cpp
      commands/tokencommand.h
      commands/update3dviewstatecommand.cpp
      commands/update3dviewstatecommand.h
      commands/valueschangedcommand.cpp
      commands/valueschangedcommand.h
      commands/view3dactioncommand.cpp
      commands/view3dactioncommand.h

      container/addimportcontainer.cpp
      container/addimportcontainer.h
      container/idcontainer.cpp
      container/idcontainer.h
      container/imagecontainer.cpp
      container/imagecontainer.h
      container/informationcontainer.cpp
      container/informationcontainer.h
      container/instancecontainer.cpp
      container/instancecontainer.h
      container/mockuptypecontainer.cpp
      container/mockuptypecontainer.h
      container/propertyabstractcontainer.cpp
      container/propertyabstractcontainer.h
      container/propertybindingcontainer.cpp
      container/propertybindingcontainer.h
      container/propertyvaluecontainer.cpp
      container/propertyvaluecontainer.h
      container/reparentcontainer.cpp
      container/reparentcontainer.h
      container/sharedmemory.h

      interfaces/commondefines.h
      interfaces/nodeinstanceclientinterface.h
      interfaces/nodeinstanceglobal.h
      interfaces/nodeinstanceserverinterface.cpp
      interfaces/nodeinstanceserverinterface.h

      types/enumeration.h
  )

  extend_qtc_target(${target_name}
    CONDITION UNIX
    SOURCES_PREFIX ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../../share/qtcreator/qml/qmlpuppet/container
    SOURCES sharedmemory_unix.cpp
  )

  extend_qtc_target(${target_name}
    CONDITION UNIX AND NOT APPLE
    DEPENDS rt
  )

  extend_qtc_target(${target_name}
    CONDITION NOT UNIX
    SOURCES_PREFIX ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../../share/qtcreator/qml/qmlpuppet/container
    SOURCES sharedmemory_qt.cpp
  )

  set(export_symbol_declaration DEFINES QMLDESIGNER_LIBRARY)
  if (QTC_STATIC_BUILD)
    set(export_symbol_declaration PUBLIC_DEFINES QMLDESIGNER_STATIC_LIBRARY)
  endif()
  extend_qtc_target(${target_name} ${export_symbol_declaration})

endfunction()
