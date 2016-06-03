/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * LXDE-Qt - a lightweight, Qt based, desktop toolset
 * http://razor-qt.org
 *
 * Copyright (C) 2012  Alec Moskvin <alecm@gmx.com>
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

#include "x11helper.h"

#include <QtGui/QGuiApplication>
#include <qpa/qplatformnativeinterface.h>

Display* X11Helper::display()
{
    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    void* display = native->nativeResourceForWindow("display", 0);
    return reinterpret_cast<Display*>(display);
}

xcb_connection_t* X11Helper::connection()
{
    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    void* connection = native->nativeResourceForWindow("connection", 0);
    return reinterpret_cast<xcb_connection_t*>(connection);
}
