project(lxqt-powermanagement)

set(QTX_LIBRARIES Qt5::Widgets Qt5::DBus Qt5::Svg Qt5::X11Extras)

set(SOURCES
    watcher.cpp
    batteryhelper.cpp
    batteryinfoframe.cpp
    batteryinfodialog.cpp
    batterywatcher.cpp
    idlenesswatcher.cpp
    lid.cpp
    lidwatcher.cpp
    main.cpp
    powermanagementd.cpp
    trayicon.cpp
    iconproducer.cpp
    x11helper.cpp
    ../config/powermanagementsettings.cpp
)

set(UI_FILES
    batteryinfoframe.ui
    batteryinfodialog.ui
)

set(DESKTOP_FILES_IN
    lxqt-powermanagement.desktop.in
)

qt5_add_dbus_adaptor(SOURCES
    org.freedesktop.ScreenSaver.xml
    idlenesswatcher.h
    IdlenessWatcher
)

lxqt_translate_ts(QM_FILES
    UPDATE_TRANSLATIONS ${UPDATE_TRANSLATIONS}
    SOURCES
        ${SOURCES}
        ${UI_FILES}
    INSTALL_DIR
        ${LXQT_TRANSLATIONS_DIR}/${PROJECT_NAME}
    PULL_TRANSLATIONS
        ${PULL_TRANSLATIONS}
    CLEAN_TRANSLATIONS
        ${CLEAN_TRANSLATIONS}
    TRANSLATIONS_REPO
        ${TRANSLATIONS_REPO}
    TRANSLATIONS_REFSPEC
        ${TRANSLATIONS_REFSPEC}
    REPO_SUBDIR
        "lxqt-powermanagement/${PROJECT_NAME}"
)

lxqt_app_translation_loader(lxqt-powermanagement_QM_LOADER ${PROJECT_NAME})

# XCB stuff (TODO: move into findXCB)
find_package(PkgConfig)

pkg_check_modules(PKG_XCB xcb)
find_path(XCB_INCLUDE_DIR xcb/xcb.h
    ${PKG_XCB_INCLUDE_DIRS}
)
find_library(XCB_LIBRARY NAMES xcb
    PATHS
    ${PKG_XCB_LIBRARY_DIRS}
)

pkg_check_modules(PKG_X11_XCB x11-xcb)
find_path(X11_XCB_INCLUDE_DIR X11/Xlib-xcb.h
    ${PKG_X11_XCB_INCLUDE_DIRS}
)
find_library(X11_XCB_LIBRARY NAMES X11-xcb
    PATHS
    ${PKG_X11_XCB_LIBRARY_DIRS}
)

pkg_check_modules(PKG_XCB_SCREENSAVER xcb-screensaver)
find_path(XCB_SCREENSAVER_INCLUDE_DIR xcb/screensaver.h
    ${PKG_XCB_SCREENSAVER_INCLUDE_DIRS}
)
find_library(XCB_SCREENSAVER_LIBRARY NAMES xcb-screensaver
    PATHS
    ${PKG_XCB_SCREENSAVER_LIBRARY_DIRS}
)

pkg_check_modules(PKG_XCB_DPMS xcb-dpms)
find_path(XCB_DPMS_INCLUDE_DIR xcb/dpms.h
    ${PKG_XCB_SCREENSAVER_INCLUDE_DIRS}
)
find_library(XCB_DPMS_LIBRARY NAMES xcb-dpms
    PATHS
    ${PKG_XCB_DPMS_LIBRARY_DIRS}
)


include_directories(
    ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
    ${XCB_INCLUDE_DIR}
    ${X11_XCB_INCLUDE_DIR}
    ${XCB_SCREENSAVER_INCLUDE_DIR}
    ${XCB_DPMS_INCLUDE_DIR}
)

add_executable(lxqt-powermanagement
    ${SOURCES}
    ${RSCS}
    ${QM_FILES}
    ${lxqt-powermanagement_QM_LOADER}
    ${DESKTOP_FILES}
)

target_link_libraries(lxqt-powermanagement
    lxqt
    ${QTX_LIBRARIES}
    ${XCB_LIBRARY}
    ${XCB_SCREENSAVER_LIBRARY}
    ${XCB_DPMS_LIBRARY}
    ${X11_XCB_LIBRARY}
    KF5::Solid
)

install(TARGETS
    ${PROJECT_NAME}
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT Runtime
)
install(FILES
    ${DESKTOP_FILES}
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications"
    COMPONENT Runtime
)
