/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * LXDE-Qt - a lightweight, Qt based, desktop toolset
 * http://razor-qt.org
 *
 * Copyright (C) 2013  Alec Moskvin <alecm@gmx.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include <xcb/dpms.h>
#include <xcb/screensaver.h>
#include <QCoreApplication>
#include <QDebug>
#include <QDBusServiceWatcher>

#include <LXQt/ProgramFinder>

#include "screensaveradaptor.h"
#include "idlenesswatcher.h"
#include "x11helper.h"

/* lockers:
 *
 * xlock(more)
 * i3lock -n
 * slock
 * alock
 * xtrlock
 */


IdlenessWatcher::IdlenessWatcher(QObject* parent):
    Watcher(parent),
    mPSettings(),
    mErrorNotification(tr("LXQt Idleness watcher failed to start")),
    mDBusWatcher(this),
    mInhibitorCookie(0),
    mIsLocked(false)
{
    qDebug() << "Starting idlenesswatcher";
    mConn = X11Helper::connection();
    xcb_prefetch_extension_data(mConn, &xcb_screensaver_id);
    xcb_prefetch_extension_data(mConn, &xcb_dpms_id);
    xcb_screensaver_query_version_cookie_t verCookie = xcb_screensaver_query_version_unchecked(mConn, XCB_SCREENSAVER_MAJOR_VERSION, XCB_SCREENSAVER_MINOR_VERSION);
    xcb_dpms_get_version_cookie_t dpmsVerCookie = xcb_dpms_get_version_unchecked(mConn, XCB_DPMS_MAJOR_VERSION, XCB_DPMS_MINOR_VERSION);
    // Note that XCB is asynchronous, so we want to make requests ASAP and get the responses as late as possible.

    mScreen = screenOfDisplay(mConn, 0);
    mErrorNotification.setUrgencyHint(LXQt::Notification::UrgencyCritical);
    mErrorNotification.setIcon("object-unlocked");
    mErrorNotification.setTimeout(0);

    new ScreenSaverAdaptor(this);
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.registerService("org.freedesktop.ScreenSaver")
        || !sessionBus.registerObject("/ScreenSaver", this))
    {
        mErrorNotification.setBody(tr("D-Bus interface org.freedesktop.ScreenSaver is already registered"));
        mErrorNotification.update();
        qWarning() << "ERROR: D-Bus interface org.freedesktop.ScreenSaver is already registered";
    }

    mDBusWatcher.setConnection(QDBusConnection::sessionBus());
    mDBusWatcher.setWatchMode(QDBusServiceWatcher::WatchForUnregistration);

    connect(&mTimer, SIGNAL(timeout()), SLOT(idleTimeout()));
    connect(&mPSettings, SIGNAL(settingsChanged()), SLOT(restartTimer()));
    connect(this, SIGNAL(done()), this, SLOT(restartTimer()));
    connect(&mDBusWatcher, SIGNAL(serviceUnregistered(QString)), SLOT(serviceUnregistered(QString)));
    connect(&mLockProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(screenUnlocked(int,QProcess::ExitStatus)));
    connect(&mErrorNotification, SIGNAL(actionActivated(int)), SLOT(notificationAction(int)));

    // Get XCB responses ...
    const xcb_query_extension_reply_t* extReply = xcb_get_extension_data(mConn, &xcb_screensaver_id);
    const xcb_query_extension_reply_t* dpmsExtReply = xcb_get_extension_data(mConn, &xcb_dpms_id);
    xcb_screensaver_query_version_reply_t* verReply = xcb_screensaver_query_version_reply(mConn, verCookie, NULL);
    xcb_dpms_get_version_reply_t* dpmsVerReply = xcb_dpms_get_version_reply(mConn, dpmsVerCookie, NULL);

    if (mScreen && extReply && extReply->present && dpmsExtReply && dpmsExtReply->present
        && verReply && dpmsVerReply
        && verReply->server_major_version == XCB_SCREENSAVER_MAJOR_VERSION
        && verReply->server_minor_version >= XCB_SCREENSAVER_MINOR_VERSION
      //&& dpmsVerReply->server_major_version == XCB_DPMS_MAJOR_VERSION
      //&& dpmsVerReply->server_minor_version >= XCB_DPMS_MINOR_VERSION
            )
    {
        free(verReply);
        free(dpmsVerReply);
    }
    else
    {
        mErrorNotification.setBody(tr("The X11 Screensaver extension is not usable"));
        mErrorNotification.update();
        if (verReply)
            free(verReply);
        qCritical() << "ERROR: Can't use the X11 Screensaver Extension!";
    }

    mErrorNotification.setActions(QStringList(tr("Configure...")));

    qDebug() << "LXQt Screenlocker started.";
    qDebug() << "timeout:" << getMaxIdleTimeoutMs() << "ms, lock command:" << mLockCommand;
    restartTimer();
}

xcb_screen_t* IdlenessWatcher::screenOfDisplay(xcb_connection_t* conn, int screen)
{
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(conn));
    for (; iter.rem; --screen, xcb_screen_next(&iter))
        if (screen == 0)
            return iter.data;
    return NULL;
}

uint IdlenessWatcher::getIdleTimeMs()
{
    xcb_screensaver_query_info_cookie_t infoCookie = xcb_screensaver_query_info_unchecked(mConn, mScreen->root);
    xcb_screensaver_query_info_reply_t* infoReply = xcb_screensaver_query_info_reply(mConn, infoCookie, NULL);
    if (!infoReply)
    {
        qWarning() << "Bad reply from X11 Screensaver";
        return 0;
    }
    uint msSinceUserInput = infoReply->ms_since_user_input;
    free(infoReply);
    return msSinceUserInput;
}

uint IdlenessWatcher::getMaxIdleTimeoutMs()
{
    return 1000 * mPSettings.getIdlenessTimeSecs();
}

void IdlenessWatcher::idleTimeout()
{
    uint msSinceUserInput = getIdleTimeMs();
    if (msSinceUserInput >= getMaxIdleTimeoutMs())
    {
        mTimer.stop();
        doAction(mPSettings.getIdlenessAction());
    }
    else
        mTimer.start(getMaxIdleTimeoutMs() - msSinceUserInput);
}

void IdlenessWatcher::restartTimer()
{
    qDebug() << ">>> Timer Restarted, waiting: " << getMaxIdleTimeoutMs() << "msecs";
    mTimer.start(getMaxIdleTimeoutMs());
}

void IdlenessWatcher::screenUnlocked(int exitCode, QProcess::ExitStatus exitStatus)
{

    mIsLocked = false;
    emit ActiveChanged(false);

    if (exitCode == 0)
    {
        restartTimer();
    }
    else
    {
        mErrorNotification.setSummary(tr("ERROR: Screen unlocked"));
        if (exitStatus == QProcess::NormalExit)
            mErrorNotification.setBody(tr("Locking program \"%1\" exited with error code %2").arg(mLockCommand).arg(exitCode));
        else
            mErrorNotification.setBody(tr("Locking program \"%1\" crashed with error code %2").arg(mLockCommand).arg(exitCode));
        mErrorNotification.update();
        mTimer.stop();
        connect(&mErrorNotification, SIGNAL(notificationClosed(CloseReason)), SLOT(idleTimeout()));
    }
}

void IdlenessWatcher::notificationAction(int num)
{
    switch (num)
    {
    case 0: // "Configure"
        QProcess::startDetached("lxqt-config-screenlocker");
    }
}

void IdlenessWatcher::serviceUnregistered(const QString& service)
{
    for (QMutableMapIterator<uint, QString> iter(mInhibitors); iter.hasNext();)
    {
        if (iter.next().value() == service)
        {
            qDebug() << "Service unregistered:" << iter.value();
            mDBusWatcher.removeWatchedService(iter.value());
            iter.remove();
        }
    }

    if (mInhibitors.isEmpty())
        restartTimer();
}

/* ---------- D-Bus methods ---------- */

void IdlenessWatcher::Lock()
{
    // lockScreen();
}

uint IdlenessWatcher::GetSessionIdleTime()
{
    return getIdleTimeMs() / 1000;
}

uint IdlenessWatcher::GetActiveTime()
{
    if (!mIsLocked)
        return 0;
    return mLockTime.secsTo(QDateTime::currentDateTime());
}

bool IdlenessWatcher::GetActive()
{
    return mIsLocked;
}

bool IdlenessWatcher::SetActive(bool activate)
{
    //if (!activate)
    return false;
    //return lockScreen();
}

void IdlenessWatcher::SimulateUserActivity()
{
    restartTimer();
}

uint IdlenessWatcher::Inhibit(const QString& applicationName, const QString& reasonForInhibit)
{
    mInhibitorCookie++;
    QString service(this->message().service());
    qDebug() << "*** Inhibit by" << applicationName << ":" << reasonForInhibit << ". Service:" << service << "->" << mInhibitorCookie;
    mDBusWatcher.addWatchedService(service);
    qDebug() << mDBusWatcher.watchedServices();
    mInhibitors.insert(mInhibitorCookie, service);
    mTimer.stop();
    return mInhibitorCookie;
}

void IdlenessWatcher::UnInhibit(uint cookie)
{
    qDebug() << "*** Uninhibit" << cookie;
    mDBusWatcher.removeWatchedService(mInhibitors.value(cookie));
    mInhibitors.remove(cookie);

    if (mInhibitors.isEmpty())
        restartTimer();
}

uint IdlenessWatcher::Throttle(const QString& applicationName, const QString& reasonForThrottle)
{
    Q_UNUSED(applicationName);
    Q_UNUSED(reasonForThrottle);
    return 0;
}

void IdlenessWatcher::UnThrottle(uint cookie)
{
    Q_UNUSED(cookie);
}
