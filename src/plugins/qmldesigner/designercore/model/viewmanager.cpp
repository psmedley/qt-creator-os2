/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "viewmanager.h"

#ifndef QMLDESIGNER_TEST

#include <abstractview.h>
#include <capturingconnectionmanager.h>
#include <componentaction.h>
#include <componentview.h>
#include <crumblebar.h>
#include <debugview.h>
#include <designeractionmanagerview.h>
#include <designmodewidget.h>
#include <edit3dview.h>
#include <formeditorview.h>
#include <itemlibraryview.h>
#include <assetslibraryview.h>
#include <navigatorview.h>
#include <nodeinstanceview.h>
#include <propertyeditorview.h>
#include <materialeditorview.h>
#include <materialbrowserview.h>
#include <rewriterview.h>
#include <stateseditorview.h>
#include <texteditorview.h>
#include <qmldesignerplugin.h>

#include <utils/algorithm.h>

#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QTabWidget>

namespace QmlDesigner {

static Q_LOGGING_CATEGORY(viewBenchmark, "qtc.viewmanager.attach", QtWarningMsg)

class ViewManagerData
{
public:
    ViewManagerData(AsynchronousImageCache &imageCache, AsynchronousImageCache &meshImageCache)
        : itemLibraryView(imageCache)
        , propertyEditorView(meshImageCache)
    {}

    InteractiveConnectionManager connectionManager;
    CapturingConnectionManager capturingConnectionManager;
    QmlModelState savedState;
    Internal::DebugView debugView;
    DesignerActionManagerView designerActionManagerView;
    NodeInstanceView nodeInstanceView{
        QCoreApplication::arguments().contains("-capture-puppet-stream") ? capturingConnectionManager
                                                                         : connectionManager};
    ComponentView componentView;
    Edit3DView edit3DView;
    FormEditorView formEditorView;
    TextEditorView textEditorView;
    AssetsLibraryView assetsLibraryView;
    ItemLibraryView itemLibraryView;
    NavigatorView navigatorView;
    PropertyEditorView propertyEditorView;
    MaterialEditorView materialEditorView;
    MaterialBrowserView materialBrowserView;
    StatesEditorView statesEditorView;

    std::vector<std::unique_ptr<AbstractView>> additionalViews;
    bool disableStandardViews = false;
};

static CrumbleBar *crumbleBar() {
    return QmlDesignerPlugin::instance()->mainWidget()->crumbleBar();
}

ViewManager::ViewManager(AsynchronousImageCache &imageCache, AsynchronousImageCache &meshImageCache)
    : d(std::make_unique<ViewManagerData>(imageCache, meshImageCache))
{
    d->formEditorView.setGotoErrorCallback([this](int line, int column) {
        d->textEditorView.gotoCursorPosition(line, column);
        if (Internal::DesignModeWidget *designModeWidget = QmlDesignerPlugin::instance()
                                                               ->mainWidget())
            designModeWidget->showDockWidget("TextEditor");
    });
}

ViewManager::~ViewManager() = default;

DesignDocument *ViewManager::currentDesignDocument() const
{
    return QmlDesignerPlugin::instance()->documentManager().currentDesignDocument();
}

void ViewManager::attachNodeInstanceView()
{
    if (nodeInstanceView()->isAttached())
        return;

    QElapsedTimer time;
    if (viewBenchmark().isInfoEnabled())
        time.start();

    qCInfo(viewBenchmark) << Q_FUNC_INFO;

    setNodeInstanceViewTarget(currentDesignDocument()->currentTarget());
    currentModel()->setNodeInstanceView(&d->nodeInstanceView);

     qCInfo(viewBenchmark) << "NodeInstanceView:" << time.elapsed();
}

void ViewManager::attachRewriterView()
{
    QElapsedTimer time;
    if (viewBenchmark().isInfoEnabled())
        time.start();

    qCInfo(viewBenchmark) << Q_FUNC_INFO;

    if (RewriterView *view = currentDesignDocument()->rewriterView()) {
        view->setWidgetStatusCallback([this](bool enable) {
            if (enable)
                enableWidgets();
            else
                disableWidgets();
        });

        currentModel()->setRewriterView(view);
        view->reactivateTextMofifierChangeSignals();
        view->restoreAuxiliaryData();
    }

    qCInfo(viewBenchmark) << "RewriterView:" << time.elapsed();
}

void ViewManager::detachRewriterView()
{
    if (RewriterView *view = currentDesignDocument()->rewriterView()) {
        view->deactivateTextMofifierChangeSignals();
        currentModel()->setRewriterView(nullptr);
    }
}

void ViewManager::switchStateEditorViewToBaseState()
{
    if (d->statesEditorView.isAttached()) {
        d->savedState = d->statesEditorView.currentState();
        d->statesEditorView.setCurrentState(d->statesEditorView.baseState());
    }
}

void ViewManager::switchStateEditorViewToSavedState()
{
    if (d->savedState.isValid() && d->statesEditorView.isAttached())
        d->statesEditorView.setCurrentState(d->savedState);
}

QList<AbstractView *> ViewManager::views() const
{
    auto list = Utils::transform<QList<AbstractView *>>(d->additionalViews,
                                                        [](auto &&view) { return view.get(); });
    list.append(standardViews());
    return list;
}

QList<AbstractView *> ViewManager::standardViews() const
{
    QList<AbstractView *> list = {&d->edit3DView,
                                  &d->formEditorView,
                                  &d->textEditorView,
                                  &d->assetsLibraryView,
                                  &d->itemLibraryView,
                                  &d->navigatorView,
                                  &d->propertyEditorView,
                                  &d->materialEditorView,
                                  &d->materialBrowserView,
                                  &d->statesEditorView,
                                  &d->designerActionManagerView};

    if (QmlDesignerPlugin::instance()->settings().value(
                DesignerSettingsKey::ENABLE_DEBUGVIEW).toBool())
         list.append(&d->debugView);

    return list;
}

void ViewManager::resetPropertyEditorView()
{
    d->propertyEditorView.resetView();
}

void ViewManager::registerFormEditorTool(std::unique_ptr<AbstractCustomTool> &&tool)
{
    d->formEditorView.registerTool(std::move(tool));
}

void ViewManager::detachViewsExceptRewriterAndComponetView()
{
    switchStateEditorViewToBaseState();
    detachAdditionalViews();

    detachStandardViews();

    currentModel()->setNodeInstanceView(nullptr);
}

void ViewManager::attachAdditionalViews()
{
    for (auto &view : d->additionalViews)
        currentModel()->attachView(view.get());
}

void ViewManager::detachAdditionalViews()
{
    for (auto &view : d->additionalViews)
        currentModel()->detachView(view.get());
}

void ViewManager::detachStandardViews()
{
    for (const auto &view : standardViews()) {
        if (view->isAttached())
            currentModel()->detachView(view);
    }
}

void ViewManager::attachComponentView()
{
    documentModel()->attachView(&d->componentView);
    QObject::connect(d->componentView.action(), &ComponentAction::currentComponentChanged,
                     currentDesignDocument(), &DesignDocument::changeToSubComponent);
    QObject::connect(d->componentView.action(), &ComponentAction::changedToMaster,
                     currentDesignDocument(), &DesignDocument::changeToMaster);
}

void ViewManager::detachComponentView()
{
    QObject::disconnect(d->componentView.action(), &ComponentAction::currentComponentChanged,
                        currentDesignDocument(), &DesignDocument::changeToSubComponent);
    QObject::disconnect(d->componentView.action(), &ComponentAction::changedToMaster,
                        currentDesignDocument(), &DesignDocument::changeToMaster);

    documentModel()->detachView(&d->componentView);
}

void ViewManager::attachViewsExceptRewriterAndComponetView()
{
    if (QmlDesignerPlugin::instance()->settings().value(
            DesignerSettingsKey::ENABLE_DEBUGVIEW).toBool())
        currentModel()->attachView(&d->debugView);

    attachNodeInstanceView();

    QElapsedTimer time;
    if (viewBenchmark().isInfoEnabled())
        time.start();

    qCInfo(viewBenchmark) << Q_FUNC_INFO;

    int last = time.elapsed();
    int currentTime = 0;
    if (!d->disableStandardViews) {
        for (const auto &view : standardViews()) {
            currentModel()->attachView(view);
            currentTime = time.elapsed();
            qCInfo(viewBenchmark) << view->widgetInfo().uniqueId << currentTime - last;
            last = currentTime;
        }
    }

    attachAdditionalViews();

    currentTime = time.elapsed();
    qCInfo(viewBenchmark) << "AdditionalViews:" << currentTime - last;
    last = currentTime;

    currentTime = time.elapsed();
    qCInfo(viewBenchmark) << "All:" << time.elapsed();
    last = currentTime;

    switchStateEditorViewToSavedState();
}

void ViewManager::setComponentNode(const ModelNode &componentNode)
{
    d->componentView.setComponentNode(componentNode);
}

void ViewManager::setComponentViewToMaster()
{
    d->componentView.setComponentToMaster();
}

void ViewManager::setNodeInstanceViewTarget(ProjectExplorer::Target *target)
{
    d->nodeInstanceView.setTarget(target);
}

QList<WidgetInfo> ViewManager::widgetInfos() const
{
    QList<WidgetInfo> widgetInfoList;

    widgetInfoList.append(d->edit3DView.widgetInfo());
    widgetInfoList.append(d->formEditorView.widgetInfo());
    widgetInfoList.append(d->textEditorView.widgetInfo());
    widgetInfoList.append(d->assetsLibraryView.widgetInfo());
    widgetInfoList.append(d->itemLibraryView.widgetInfo());
    widgetInfoList.append(d->navigatorView.widgetInfo());
    widgetInfoList.append(d->propertyEditorView.widgetInfo());
    widgetInfoList.append(d->materialEditorView.widgetInfo());
    widgetInfoList.append(d->materialBrowserView.widgetInfo());
    widgetInfoList.append(d->statesEditorView.widgetInfo());
    if (d->debugView.hasWidget())
        widgetInfoList.append(d->debugView.widgetInfo());

    for (auto &view : d->additionalViews) {
        if (view->hasWidget())
            widgetInfoList.append(view->widgetInfo());
    }

    Utils::sort(widgetInfoList, [](const WidgetInfo &firstWidgetInfo, const WidgetInfo &secondWidgetInfo) {
        return firstWidgetInfo.placementPriority < secondWidgetInfo.placementPriority;
    });

    return widgetInfoList;
}

QWidget *ViewManager::widget(const QString &uniqueId) const
{
    const QList<WidgetInfo> widgetInfoList = widgetInfos();
    for (const WidgetInfo &widgetInfo : widgetInfoList) {
        if (widgetInfo.uniqueId == uniqueId)
            return widgetInfo.widget;
    }
    return nullptr;
}

void ViewManager::disableWidgets()
{
    for (const auto &view : views())
        view->disableWidget();
}

void ViewManager::enableWidgets()
{
    for (const auto &view : views())
        view->enableWidget();
}

void ViewManager::pushFileOnCrumbleBar(const Utils::FilePath &fileName)
{
    crumbleBar()->pushFile(fileName);
}

void ViewManager::pushInFileComponentOnCrumbleBar(const ModelNode &modelNode)
{
    crumbleBar()->pushInFileComponent(modelNode);
}

void ViewManager::nextFileIsCalledInternally()
{
    crumbleBar()->nextFileIsCalledInternally();
}

NodeInstanceView *ViewManager::nodeInstanceView() const
{
    return &d->nodeInstanceView;
}

QWidgetAction *ViewManager::componentViewAction() const
{
    return d->componentView.action();
}

DesignerActionManager &ViewManager::designerActionManager()
{
    return d->designerActionManagerView.designerActionManager();
}

const DesignerActionManager &ViewManager::designerActionManager() const
{
    return d->designerActionManagerView.designerActionManager();
}

void ViewManager::qmlJSEditorContextHelp(const Core::IContext::HelpCallback &callback) const
{
    d->textEditorView.qmlJSEditorContextHelp(callback);
}

Model *ViewManager::currentModel() const
{
    return currentDesignDocument()->currentModel();
}

Model *ViewManager::documentModel() const
{
    return currentDesignDocument()->documentModel();
}

void ViewManager::exportAsImage()
{
    d->formEditorView.exportAsImage();
}

void ViewManager::reformatFileUsingTextEditorView()
{
    d->textEditorView.reformatFile();
}

bool ViewManager::usesRewriterView(RewriterView *rewriterView)
{
    return currentDesignDocument()->rewriterView() == rewriterView;
}

void ViewManager::disableStandardViews()
{
    d->disableStandardViews = true;
    detachStandardViews();
}

void ViewManager::enableStandardViews()
{
    d->disableStandardViews = false;
    attachViewsExceptRewriterAndComponetView();
}

void ViewManager::addView(std::unique_ptr<AbstractView> &&view)
{
    d->additionalViews.push_back(std::move(view));
}

} // namespace QmlDesigner

#endif //QMLDESIGNER_TEST
