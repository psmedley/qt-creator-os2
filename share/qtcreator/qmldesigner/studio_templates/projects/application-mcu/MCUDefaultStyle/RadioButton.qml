/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Design Studio.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
**
****************************************************************************/

import QtQuick 2.15
import QtQuick.Templates 2.15 as T

T.RadioButton {
    id: control

    implicitWidth: Math.max((background ? background.implicitWidth : 0) + leftInset + rightInset,
                            (contentItem ? contentItem.implicitWidth : 0) + leftPadding + rightPadding)
    // TODO: https://bugreports.qt.io/browse/UL-783
//    implicitHeight: Math.max((background ? background.implicitHeight : 0) + topInset + bottomInset,
//                             (contentItem ? contentItem.implicitHeight : 0) + topPadding + bottomPadding,
//                             (indicator ? indicator.implicitHeight : 0) + topPadding + bottomPadding)
    implicitHeight: Math.max((background ? background.implicitHeight : 0) + topInset + bottomInset,
                             Math.max((contentItem ? contentItem.implicitHeight : 0) + topPadding + bottomPadding,
                             (indicator ? indicator.implicitHeight : 0) + topPadding + bottomPadding))

    leftPadding: DefaultStyle.padding
    rightPadding: DefaultStyle.padding
    topPadding: DefaultStyle.padding
    bottomPadding: DefaultStyle.padding

    leftInset: DefaultStyle.inset
    topInset: DefaultStyle.inset
    rightInset: DefaultStyle.inset
    bottomInset: DefaultStyle.inset

    spacing: 10

    indicator: Image {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        source: !control.enabled
                ? "images/radiobutton-disabled.png"
                : (control.checked
                   ? (control.down ? "images/radiobutton-checked-pressed.png" : "images/radiobutton-checked.png")
                   : (control.down ? "images/radiobutton-pressed.png" : "images/radiobutton.png"))
    }

    // Need a background for insets to work.
    background: Item {
        implicitWidth: 100
        implicitHeight: 42
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? DefaultStyle.textColor : DefaultStyle.disabledTextColor
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator ? (control.indicator.width + control.spacing) : 0
    }
}
