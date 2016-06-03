/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * LXDE-Qt - a lightweight, Qt based, desktop toolset
 * http://razor-qt.org
 *
 * Copyright: 2012 Razor team
 * Authors:
 *   Christian Surlykke <christian@surlykke.dk>
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */
#ifndef IDLE_SETTINGS_H
#define IDLE_SETTINGS_H

#include <QGroupBox>
#include <LXQt/Settings>

#include "powermanagementsettings.h"

namespace Ui {
    class IdlenessWatcherSettings;
}

class IdlenessWatcherSettings : public QWidget
{
    Q_OBJECT

public:
    explicit IdlenessWatcherSettings(QWidget *parent = 0);
    ~IdlenessWatcherSettings();

public slots:
    void loadSettings();

private slots:
    void minutesChanged(int newVal);
    void secondsChanged(int newVal);
    void saveSettings();

private:
    PowerManagementSettings mSettings;
    Ui::IdlenessWatcherSettings *mUi;
};

#endif // IDLE_SETTINGS_H
