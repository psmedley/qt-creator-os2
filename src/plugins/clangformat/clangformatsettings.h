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

#pragma once

#include <QString>

namespace ClangFormat {

class ClangFormatSettings
{
public:
    static ClangFormatSettings &instance();

    ClangFormatSettings();
    void write() const;

    void setOverrideDefaultFile(bool enable);
    bool overrideDefaultFile() const;

    enum Mode {
        Indenting = 0,
        Formatting,
        Disable
    };

    void setMode(Mode mode);
    Mode mode() const;

    void setFormatWhileTyping(bool enable);
    bool formatWhileTyping() const;

    void setFormatOnSave(bool enable);
    bool formatOnSave() const;

private:
    Mode m_mode;
    bool m_overrideDefaultFile = false;
    bool m_formatWhileTyping = false;
    bool m_formatOnSave = false;
};

} // namespace ClangFormat
