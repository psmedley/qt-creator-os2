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

#pragma once

#include <QtGlobal>

namespace Python {
namespace Constants {

const char C_PYTHONEDITOR_ID[] = "PythonEditor.PythonEditor";
const char C_PYTHONRUNCONFIGURATION_ID[] = "PythonEditor.RunConfiguration.";

const char C_EDITOR_DISPLAY_NAME[] =
        QT_TRANSLATE_NOOP("OpenWith::Editors", "Python Editor");

const char C_PYTHONOPTIONS_PAGE_ID[] = "PythonEditor.OptionsPage";
const char C_PYLSCONFIGURATION_PAGE_ID[] = "PythonEditor.PythonLanguageServerConfiguration";
const char C_PYTHON_SETTINGS_CATEGORY[] = "P.Python";

const char PYTHON_OPEN_REPL[] = "Python.OpenRepl";
const char PYTHON_OPEN_REPL_IMPORT[] = "Python.OpenReplImport";
const char PYTHON_OPEN_REPL_IMPORT_TOPLEVEL[] = "Python.OpenReplImportToplevel";

const char PYLS_SETTINGS_ID[] = "Python.PyLSSettingsID";

/*******************************************************************************
 * MIME type
 ******************************************************************************/
const char C_PY_MIMETYPE[] = "text/x-python";
const char C_PY3_MIMETYPE[] = "text/x-python3";
const char C_PY_MIME_ICON[] = "text-x-python";

} // namespace Constants
} // namespace Python
