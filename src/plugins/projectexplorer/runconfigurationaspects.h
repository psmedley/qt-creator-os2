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

#include "environmentaspect.h"

#include <utils/aspects.h>

#include <QPointer>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QComboBox;
class QPlainTextEdit;
class QToolButton;
QT_END_NAMESPACE

namespace Utils { class ExpandButton; }

namespace ProjectExplorer {

class ProjectConfiguration;

class PROJECTEXPLORER_EXPORT TerminalAspect : public Utils::BaseAspect
{
    Q_OBJECT

public:
    TerminalAspect();

    void addToLayout(Utils::LayoutBuilder &builder) override;

    bool useTerminal() const;
    void setUseTerminalHint(bool useTerminal);

    bool isUserSet() const;

    struct Data : BaseAspect::Data
    {
        bool useTerminal;
        bool isUserSet;
    };

private:
    void fromMap(const QVariantMap &map) override;
    void toMap(QVariantMap &map) const override;

    void calculateUseTerminal();

    bool m_useTerminalHint = false;
    bool m_useTerminal = false;
    bool m_userSet = false;
    QPointer<QCheckBox> m_checkBox; // Owned by RunConfigWidget
};

class PROJECTEXPLORER_EXPORT WorkingDirectoryAspect : public Utils::BaseAspect
{
    Q_OBJECT

public:
    explicit WorkingDirectoryAspect(const Utils::MacroExpander *expander,
                                    EnvironmentAspect *envAspect);

    void addToLayout(Utils::LayoutBuilder &builder) override;

    Utils::FilePath workingDirectory() const;
    Utils::FilePath defaultWorkingDirectory() const;
    Utils::FilePath unexpandedWorkingDirectory() const;
    void setDefaultWorkingDirectory(const Utils::FilePath &defaultWorkingDirectory);
    Utils::PathChooser *pathChooser() const;

private:
    void fromMap(const QVariantMap &map) override;
    void toMap(QVariantMap &map) const override;

    void resetPath();

    EnvironmentAspect *m_envAspect = nullptr;
    Utils::FilePath m_workingDirectory;
    Utils::FilePath m_defaultWorkingDirectory;
    QPointer<Utils::PathChooser> m_chooser;
    QPointer<QToolButton> m_resetButton;
    const Utils::MacroExpander *m_macroExpander = nullptr;
};

class PROJECTEXPLORER_EXPORT ArgumentsAspect : public Utils::BaseAspect
{
    Q_OBJECT

public:
    explicit ArgumentsAspect(const Utils::MacroExpander *macroExpander);

    void addToLayout(Utils::LayoutBuilder &builder) override;

    QString arguments() const;
    QString unexpandedArguments() const;

    void setArguments(const QString &arguments);
    void setLabelText(const QString &labelText);
    void setResetter(const std::function<QString()> &resetter);
    void resetArguments();

    struct Data : BaseAspect::Data
    {
        QString arguments;
    };

private:
    void fromMap(const QVariantMap &map) override;
    void toMap(QVariantMap &map) const override;

    QWidget *setupChooser();

    QString m_arguments;
    QString m_labelText;
    QPointer<Utils::FancyLineEdit> m_chooser;
    QPointer<QPlainTextEdit> m_multiLineChooser;
    QPointer<Utils::ExpandButton> m_multiLineButton;
    QPointer<QToolButton> m_resetButton;
    bool m_multiLine = false;
    mutable bool m_currentlyExpanding = false;
    std::function<QString()> m_resetter;
    const Utils::MacroExpander *m_macroExpander = nullptr;
};

class PROJECTEXPLORER_EXPORT UseLibraryPathsAspect : public Utils::BoolAspect
{
    Q_OBJECT

public:
    UseLibraryPathsAspect();
};

class PROJECTEXPLORER_EXPORT UseDyldSuffixAspect : public Utils::BoolAspect
{
    Q_OBJECT

public:
    UseDyldSuffixAspect();
};

class PROJECTEXPLORER_EXPORT RunAsRootAspect : public Utils::BoolAspect
{
    Q_OBJECT

public:
    RunAsRootAspect();
};

class PROJECTEXPLORER_EXPORT ExecutableAspect : public Utils::BaseAspect
{
    Q_OBJECT

public:
    enum ExecutionDeviceSelector { HostDevice, BuildDevice, RunDevice };

    explicit ExecutableAspect(Target *target, ExecutionDeviceSelector selector);
    ~ExecutableAspect() override;

    Utils::FilePath executable() const;
    void setExecutable(const Utils::FilePath &executable);

    void setSettingsKey(const QString &key);
    void makeOverridable(const QString &overridingKey, const QString &useOverridableKey);
    void addToLayout(Utils::LayoutBuilder &builder) override;
    void setLabelText(const QString &labelText);
    void setPlaceHolderText(const QString &placeHolderText);
    void setHistoryCompleter(const QString &historyCompleterKey);
    void setExpectedKind(const Utils::PathChooser::Kind expectedKind);
    void setEnvironmentChange(const Utils::EnvironmentChange &change);
    void setDisplayStyle(Utils::StringAspect::DisplayStyle style);

    struct Data : BaseAspect::Data
    {
        Utils::FilePath executable;
    };

protected:
    void fromMap(const QVariantMap &map) override;
    void toMap(QVariantMap &map) const override;

private:
    QString executableText() const;
    void updateDevice();

    Utils::StringAspect m_executable;
    Utils::StringAspect *m_alternativeExecutable = nullptr;
    Target *m_target = nullptr;
    ExecutionDeviceSelector m_selector = RunDevice;
};

class PROJECTEXPLORER_EXPORT SymbolFileAspect : public Utils::StringAspect
{
    Q_OBJECT

public:
     SymbolFileAspect() = default;
};

class PROJECTEXPLORER_EXPORT Interpreter
{
public:
    Interpreter();
    Interpreter(const QString &id,
                const QString &name,
                const Utils::FilePath &command,
                bool autoDetected = true);

    inline bool operator==(const Interpreter &other) const
    {
        return id == other.id && name == other.name && command == other.command;
    }

    QString id;
    QString name;
    Utils::FilePath command;
    bool autoDetected = true;
};

class PROJECTEXPLORER_EXPORT InterpreterAspect : public Utils::BaseAspect
{
    Q_OBJECT

public:
    InterpreterAspect();

    Interpreter currentInterpreter() const;
    void updateInterpreters(const QList<Interpreter> &interpreters);
    void setDefaultInterpreter(const Interpreter &interpreter);
    void setCurrentInterpreter(const Interpreter &interpreter);
    void setSettingsDialogId(Utils::Id id) { m_settingsDialogId = id; }

    void fromMap(const QVariantMap &) override;
    void toMap(QVariantMap &) const override;
    void addToLayout(Utils::LayoutBuilder &builder) override;

    struct Data : Utils::BaseAspect::Data { Interpreter interpreter; };

private:
    void updateCurrentInterpreter();
    void updateComboBox();
    QList<Interpreter> m_interpreters;
    QPointer<QComboBox> m_comboBox;
    QString m_defaultId;
    QString m_currentId;
    Utils::Id m_settingsDialogId;
};

class PROJECTEXPLORER_EXPORT MainScriptAspect : public Utils::StringAspect
{
    Q_OBJECT

public:
    MainScriptAspect() = default;
};

} // namespace ProjectExplorer
