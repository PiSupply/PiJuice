/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * LXDE-Qt - a lightweight, Qt based, desktop toolset
 *
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

#include <QDebug>
#include <QStringList>

#include "batteryhelper.h"
#include "powermanagementd.h"
#include "../config/powermanagementsettings.h"
#include "idlenesswatcher.h"
#include "lidwatcher.h"
#include "batterywatcher.h"

#define CURRENT_RUNCHECK_LEVEL 1

PowerManagementd::PowerManagementd() :
        mBatterywatcherd(0),
        mLidwatcherd(0),
        mIdlenesswatcherd(0),
        mSettings()
 {
    connect(&mSettings, SIGNAL(settingsChanged()), this, SLOT(settingsChanged()));
    settingsChanged();

    if (mSettings.getRunCheckLevel() < CURRENT_RUNCHECK_LEVEL)
    {
        performRunCheck();
        mSettings.setRunCheckLevel(CURRENT_RUNCHECK_LEVEL);
    }
}

PowerManagementd::~PowerManagementd()
{
}

void PowerManagementd::settingsChanged()
{
    if (mSettings.isBatteryWatcherEnabled() && !mBatterywatcherd)
        mBatterywatcherd = new BatteryWatcher(this);
    else if (mBatterywatcherd && ! mSettings.isBatteryWatcherEnabled())
    {
        mBatterywatcherd->deleteLater();
        mBatterywatcherd = 0;
    }

    if (mSettings.isLidWatcherEnabled() && !mLidwatcherd)
    {
        mLidwatcherd = new LidWatcher(this);
    }
    else if (mLidwatcherd && ! mSettings.isLidWatcherEnabled())
    {
        mLidwatcherd->deleteLater();
        mLidwatcherd = 0;
    }

    if (mSettings.isIdlenessWatcherEnabled() && !mIdlenesswatcherd)
    {
        mIdlenesswatcherd = new IdlenessWatcher(this);
    }
    else if (mIdlenesswatcherd && !mSettings.isIdlenessWatcherEnabled())
    {
        mIdlenesswatcherd->deleteLater();
        mIdlenesswatcherd = 0;
    }

}

void PowerManagementd::runConfigure()
{
    mNotification.close();
    QProcess::startDetached("lxqt-config-powermanagement");
}

void PowerManagementd::performRunCheck()
{
    mSettings.setLidWatcherEnabled(Lid().haveLid());
    bool hasBattery = false;
    foreach (Solid::Device device, Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString()))
        if (device.as<Solid::Battery>()->type() == Solid::Battery::PrimaryBattery)
            hasBattery = true;
    mSettings.setBatteryWatcherEnabled(hasBattery);
    mSettings.setIdlenessWatcherEnabled(true);
    mSettings.sync();

    mNotification.setSummary(tr("Power Management"));
    mNotification.setBody(tr("You are running LXQt Power Management for the first time.\nYou can configure it from settings... "));
    mNotification.setActions(QStringList() << tr("Configure..."));
    mNotification.setTimeout(10000);
    connect(&mNotification, SIGNAL(actionActivated(int)), SLOT(runConfigure()));
    mNotification.update();
}
