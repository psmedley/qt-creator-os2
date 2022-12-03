/****************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
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

#include "imagecachecollector.h"
#include "imagecacheconnectionmanager.h"

#include <metainfo.h>
#include <model.h>
#include <nodeinstanceview.h>
#include <plaintexteditmodifier.h>
#include <rewriterview.h>

#include <projectexplorer/project.h>
#include <projectexplorer/target.h>
#include <utils/fileutils.h>

#include <QPlainTextEdit>

namespace QmlDesigner {

namespace {

QByteArray fileToByteArray(QString const &filename)
{
    QFile file(filename);
    QFileInfo fleInfo(file);

    if (fleInfo.exists() && file.open(QFile::ReadOnly))
        return file.readAll();

    return {};
}

QString fileToString(const QString &filename)
{
    return QString::fromUtf8(fileToByteArray(filename));
}

} // namespace

ImageCacheCollector::ImageCacheCollector(ImageCacheConnectionManager &connectionManager,
                                         QSize captureImageMinimumSize,
                                         QSize captureImageMaximumSize,
                                         ImageCacheCollectorNullImageHandling nullImageHandling)
    : m_connectionManager{connectionManager}
    , captureImageMinimumSize{captureImageMinimumSize}
    , captureImageMaximumSize{captureImageMaximumSize}
    , nullImageHandling{nullImageHandling}
{}

ImageCacheCollector::~ImageCacheCollector() = default;

void ImageCacheCollector::start(Utils::SmallStringView name,
                                Utils::SmallStringView state,
                                const ImageCache::AuxiliaryData &auxiliaryData,
                                CaptureCallback captureCallback,
                                AbortCallback abortCallback)
{
    RewriterView rewriterView{RewriterView::Amend, nullptr};
    NodeInstanceView nodeInstanceView{m_connectionManager};
    nodeInstanceView.setCaptureImageMinimumAndMaximumSize(captureImageMinimumSize,
                                                          captureImageMaximumSize);

    const QString filePath{name};
    std::unique_ptr<Model> model{QmlDesigner::Model::create("QtQuick/Item", 2, 1)};
    model->setFileUrl(QUrl::fromLocalFile(filePath));

    auto textDocument = std::make_unique<QTextDocument>(fileToString(filePath));

    auto modifier = std::make_unique<NotIndentingTextEditModifier>(textDocument.get(),
                                                                   QTextCursor{textDocument.get()});

    rewriterView.setTextModifier(modifier.get());

    model->setRewriterView(&rewriterView);

    bool is3DRoot = rewriterView.errors().isEmpty()
            && (rewriterView.rootModelNode().isSubclassOf("QtQuick3D.Node")
                || rewriterView.rootModelNode().isSubclassOf("QtQuick3D.Material"));

    if (!rewriterView.errors().isEmpty() || (!rewriterView.rootModelNode().metaInfo().isGraphicalItem()
                                        && !is3DRoot)) {
        if (abortCallback)
            abortCallback(ImageCache::AbortReason::Failed);
        return;
    }

    if (is3DRoot) {
        if (auto libIcon = Utils::get_if<ImageCache::LibraryIconAuxiliaryData>(&auxiliaryData))
            rewriterView.rootModelNode().setAuxiliaryData("isLibraryIcon@NodeInstance", libIcon->enable);
    }

    ModelNode stateNode = rewriterView.modelNodeForId(QString{state});

    if (stateNode.isValid())
        rewriterView.setCurrentStateNode(stateNode);

    auto callback = [=, captureCallback = std::move(captureCallback)](const QImage &image) {
        if (nullImageHandling == ImageCacheCollectorNullImageHandling::CaptureNullImage
            || !image.isNull()) {
            QSize smallImageSize = image.size().scaled(QSize{96, 96}.boundedTo(image.size()),
                                                       Qt::KeepAspectRatio);
            QImage smallImage = image.isNull() ? QImage{}
                                               : image.scaled(smallImageSize,
                                                              Qt::IgnoreAspectRatio,
                                                              Qt::SmoothTransformation);
            captureCallback(image, smallImage);
        }
    };

    if (!m_target)
        return;

    nodeInstanceView.setTarget(m_target.data());
    m_connectionManager.setCallback(std::move(callback));
    nodeInstanceView.setCrashCallback([=] { abortCallback(ImageCache::AbortReason::Failed); });
    model->setNodeInstanceView(&nodeInstanceView);

    bool capturedDataArrived = m_connectionManager.waitForCapturedData();

    m_connectionManager.setCallback({});
    m_connectionManager.setCrashCallback({});

    model->setNodeInstanceView({});
    model->setRewriterView({});

    if (!capturedDataArrived && abortCallback)
        abortCallback(ImageCache::AbortReason::Failed);
}

std::pair<QImage, QImage> ImageCacheCollector::createImage(Utils::SmallStringView,
                                                           Utils::SmallStringView,
                                                           const ImageCache::AuxiliaryData &)
{
    return {};
}

QIcon ImageCacheCollector::createIcon(Utils::SmallStringView,
                                      Utils::SmallStringView,
                                      const ImageCache::AuxiliaryData &)
{
    return {};
}

void ImageCacheCollector::setTarget(ProjectExplorer::Target *target)
{
    m_target = target;
}

ProjectExplorer::Target *ImageCacheCollector::target() const
{
    return m_target;
}

} // namespace QmlDesigner
