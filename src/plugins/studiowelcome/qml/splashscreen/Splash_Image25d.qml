/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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
import QtQuick 2.3

Rectangle {
    id: splashBackground
    width: 460
    height: 480
    color: "transparent"
    scale: 1.2

    layer.enabled: true
    layer.textureSize: Qt.size(width * 2, height * 2)
    layer.smooth: true

    Item {
        id: composition
        anchors.centerIn: parent
        width: 460
        height: 480
        visible: true
        anchors.verticalCenterOffset: -1
        anchors.horizontalCenterOffset: 14
        clip: true

        layer.enabled: true
        layer.textureSize: Qt.size(width * 2, height * 2)
        layer.smooth: true

        Splash_Image2d_png {
            x: -22
            y: -33
            width: 461
            height: 427
            layer.enabled: true
            layer.effect: ColorOverlayEffect {
                id: colorOverlay
                visible: true
                color: "#41cd52"
            }
            scale: 1
        }
    }
}
