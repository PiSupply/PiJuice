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

#ifndef COMMON_H
#define COMMON_H

#include <QComboBox>
#include <QString>

#include <LXQt/Settings>

class PowerManagementSettings : public LXQt::Settings
{
    Q_OBJECT

public:
    PowerManagementSettings(QObject* parent = 0);
    ~PowerManagementSettings();

    int getRunCheckLevel();
    void setRunCheckLevel(int newLevel);

    bool isBatteryWatcherEnabled();
    void setBatteryWatcherEnabled(bool batteryWatcherEnabled);

    int getPowerLowAction();
    void setPowerLowAction(int powerLowAction);

    int getPowerLowLevel();
    void setPowerLowLevel(int powerLowLevel);

    int getPowerLowWarningTime();
    void setPowerLowWarningTime(int powerLowWarningTime);

    bool isShowIcon();
    void setShowIcon(bool showIcon);

    bool isUseThemeIcons();
    void setUseThemeIcons(bool useThemeIcons);


    bool isLidWatcherEnabled();
    void setLidWatcherEnabled(bool lidWatcherEnabled);

    int getLidClosedAcAction();
    void setLidClosedAcAction(int lidClosedAcAction);

    int getLidClosedAction();
    void setLidClosedAction(int lidClosedAction);

    int getLidClosedExtMonAcAction();
    void setLidClosedExtMonAcAction(int lidClosedExtMonAcAction);

    int getLidClosedExtMonAction();
    void setLidClosedExtMonAction(int lidClosedExtMonAction);

    bool isEnableExtMonLidClosedActions();
    void setEnableExtMonLidClosedActions(bool enableExtMonLidClosedActions);

    int getIdlenessAction();
    void setIdlenessAction(int idlenessAction);

    int getIdlenessTimeSecs();
    void setIdlenessTimeSecs(int idlenessTimeSecs);

    bool isIdlenessWatcherEnabled();
    void setIdlenessWatcherEnabled(bool idlenessWatcherEnabled);
};


#endif // COMMON_H
