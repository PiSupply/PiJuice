/*
* Copyright (c) 2014 Christian Surlykke
*
* This file is part of the LXQt project. <http://lxqt.org>
* It is distributed under the LGPL 2.1 or later license.
* Please refer to the LICENSE file for a copy of the license.
*/

#ifndef POWERMANAGEMENTD_H
#define	POWERMANAGEMENTD_H

#include <LXQt/Notification>

#include "../config/powermanagementsettings.h"

class BatteryWatcher;
class LidWatcher;
class IdlenessWatcher;

class PowerManagementd : public QObject
{
    Q_OBJECT

public:
    PowerManagementd();
    virtual ~PowerManagementd();

private slots:
    void settingsChanged();
    void runConfigure();

private:
    void performRunCheck();

    BatteryWatcher* mBatterywatcherd;
    LidWatcher* mLidwatcherd;
    IdlenessWatcher* mIdlenesswatcherd;

    PowerManagementSettings mSettings;
    LXQt::Notification mNotification;
};

#endif	/* POWERMANAGEMENTD_H */

