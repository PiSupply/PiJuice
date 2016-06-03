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

#ifndef X11HELPER_H
#define X11HELPER_H

#include <xcb/xcb.h>

// Avoid polluting everything with X11/Xlib.h:
typedef struct _XDisplay Display;

/**
 * @brief The X11Helper class is class to get the X11 Display or XCB connection
 *
 * It's intended to be used as a wrapper/replacement for QX11Info, which is removed in Qt5.
 */
class X11Helper
{
public:
    /**
     * @brief display Returns the X11 display
     * @return
     */
    static Display* display();

    /**
     * @brief connection Returns the XCB connection
     * @return
     */
    static xcb_connection_t* connection();
};

#endif // X11HELPER_H
