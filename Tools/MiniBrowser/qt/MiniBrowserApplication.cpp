/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2010 University of Szeged
 * Copyright (C) 2015 The Qt Company Ltd.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "MiniBrowserApplication.h"

#include "BrowserWindow.h"
#if HAVE(QTTESTSUPPORT)
#include "QtTestSupport.h"
#endif
#include "private/qquickwebview_p.h"
#include "utils.h"
#include <QRegularExpression>
#include <QEvent>
#include <QMouseEvent>
#include <QTouchEvent>

static inline bool isTouchEvent(const QEvent* event)
{
    switch (event->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
        return true;
    default:
        return false;
    }
}

static inline bool isMouseEvent(const QEvent* event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
        return true;
    default:
        return false;
    }
}

MiniBrowserApplication::MiniBrowserApplication(int& argc, char** argv)
    : QGuiApplication(argc, argv)
    , m_realTouchEventReceived(false)
    , m_pendingFakeTouchEventCount(0)
    , m_isRobotized(false)
    , m_robotTimeoutSeconds(0)
    , m_robotExtraTimeSeconds(0)
    , m_windowOptions(this)
    , m_holdingControl(false)
{
    setOrganizationName("QtProject");
    setApplicationName("QtMiniBrowser");
    setApplicationVersion("0.1");

    handleUserOptions();
}

bool MiniBrowserApplication::notify(QObject* target, QEvent* event)
{
    // We try to be smart, if we received real touch event, we are probably on a device
    // with touch screen, and we should not have touch mocking.

    if (!event->spontaneous() || m_realTouchEventReceived || !m_windowOptions.touchMockingEnabled())
        return QGuiApplication::notify(target, event);

    if (isTouchEvent(event)) {
        if (m_pendingFakeTouchEventCount)
            --m_pendingFakeTouchEventCount;
        else
            m_realTouchEventReceived = true;
        return QGuiApplication::notify(target, event);
    }

    BrowserWindow* browserWindow = qobject_cast<BrowserWindow*>(target);
    if (!browserWindow)
        return QGuiApplication::notify(target, event);

    m_holdingControl = QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier);

    // In QML events are propagated through parents. But since the WebView
    // may consume key events, a shortcut might never reach the top QQuickItem.
    // Therefore we are checking here for shortcuts.
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if ((keyEvent->key() == Qt::Key_R && keyEvent->modifiers() == Qt::ControlModifier) || keyEvent->key() == Qt::Key_F5) {
            browserWindow->reload();
            return true;
        }
        if ((keyEvent->key() == Qt::Key_L && keyEvent->modifiers() == Qt::ControlModifier) || keyEvent->key() == Qt::Key_F6) {
            browserWindow->focusAddressBar();
            return true;
        }
        if ((keyEvent->key() == Qt::Key_F && keyEvent->modifiers() == Qt::ControlModifier) || keyEvent->key() == Qt::Key_F3) {
            browserWindow->toggleFind();
            return true;
        }
    }

    if (event->type() == QEvent::KeyRelease && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Control) {
        Q_FOREACH (int id, m_heldTouchPoints)
            if (m_touchPoints.contains(id) && !QGuiApplication::mouseButtons().testFlag(Qt::MouseButton(id))) {
                m_touchPoints[id].setState(QEventPoint::Released);
                m_heldTouchPoints.remove(id);
            } else
                m_touchPoints[id].setState(QEventPoint::Stationary);

        sendTouchEvent(browserWindow, m_heldTouchPoints.isEmpty() ? QEvent::TouchEnd : QEvent::TouchUpdate, static_cast<QKeyEvent*>(event)->timestamp());
    }

    if (isMouseEvent(event)) {
        const QMouseEvent* const mouseEvent = static_cast<QMouseEvent*>(event);

        QMutableEventPoint touchPoint;
        touchPoint.setPressure(1);

        QEvent::Type touchType = QEvent::None;

        switch (mouseEvent->type()) {
        case QEvent::MouseButtonPress:
            touchPoint.setId(mouseEvent->button());
            if (m_touchPoints.contains(touchPoint.id())) {
                touchPoint.setState(QEventPoint::Updated);
                touchType = QEvent::TouchUpdate;
            } else {
                touchPoint.setState(QEventPoint::Pressed);
                // Check if more buttons are held down than just the event triggering one.
                if (mouseEvent->buttons() > mouseEvent->button())
                    touchType = QEvent::TouchUpdate;
                else
                    touchType = QEvent::TouchBegin;
            }
            break;
        case QEvent::MouseMove:
            if (!mouseEvent->buttons()) {
                // We have to swallow the event instead of propagating it,
                // since we avoid sending the mouse release events and if the
                // Flickable is the mouse grabber it would receive the event
                // and would move the content.
                event->accept();
                return true;
            }
            touchType = QEvent::TouchUpdate;
            touchPoint.setId(mouseEvent->buttons());
            touchPoint.setState(QEventPoint::Updated);
            break;
        case QEvent::MouseButtonRelease:
            // Check if any buttons are still held down after this event.
            if (mouseEvent->buttons())
                touchType = QEvent::TouchUpdate;
            else
                touchType = QEvent::TouchEnd;
            touchPoint.setId(mouseEvent->button());
            touchPoint.setState(QEventPoint::Released);
            break;
        case QEvent::MouseButtonDblClick:
            // Eat double-clicks, their accompanying press event is all we need.
            event->accept();
            return true;
        default:
            Q_ASSERT_X(false, "multi-touch mocking", "unhandled event type");
        }

        // A move can have resulted in multiple buttons, so we need check them individually.
        if (touchPoint.id() & Qt::LeftButton)
            updateTouchPoint(mouseEvent, touchPoint, Qt::LeftButton);
        if (touchPoint.id() & Qt::MiddleButton)
            updateTouchPoint(mouseEvent, touchPoint, Qt::MiddleButton);
        if (touchPoint.id() & Qt::RightButton)
            updateTouchPoint(mouseEvent, touchPoint, Qt::RightButton);

        if (m_holdingControl && touchPoint.state() == QEventPoint::Released) {
            // We avoid sending the release event because the Flickable is
            // listening to mouse events and would start a bounce-back
            // animation if it received a mouse release.
            event->accept();
            return true;
        }

        // Update states for all other touch-points
        for (QHash<int, QMutableEventPoint>::iterator it = m_touchPoints.begin(), end = m_touchPoints.end(); it != end; ++it) {
            if (!(it.value().id() & touchPoint.id()))
                it.value().setState(QEventPoint::Stationary);
        }

        Q_ASSERT(touchType != QEvent::None);

        if (!sendTouchEvent(browserWindow, touchType, mouseEvent->timestamp()))
            return QGuiApplication::notify(target, event);

        event->accept();
        return true;
    }

    return QGuiApplication::notify(target, event);
}

void MiniBrowserApplication::updateTouchPoint(const QMouseEvent* mouseEvent, QMutableEventPoint touchPoint, Qt::MouseButton mouseButton)
{
    // Ignore inserting additional touch points if Ctrl isn't held because it produces
    // inconsistent touch events and results in assers in the gesture recognizers.
    if (!m_holdingControl && m_touchPoints.size() && !m_touchPoints.contains(mouseButton))
        return;

    if (m_holdingControl && touchPoint.state() == QEventPoint::Released) {
        m_heldTouchPoints.insert(mouseButton);
        return;
    }

    // Gesture recognition uses the screen position for the initial threshold
    // but since the canvas translates touch events we actually need to pass
    // the screen position as the scene position to deliver the appropriate
    // coordinates to the target.
    touchPoint.setPosition(mouseEvent->localPos());
    touchPoint.setScenePosition(mouseEvent->screenPos());
    touchPoint.setEllipseDiameters({40, 40});

    if (touchPoint.state() == QEventPoint::Pressed)
        touchPoint.setGlobalPressPosition(mouseEvent->globalPosition());
    else {
        const QMutableEventPoint& oldTouchPoint = m_touchPoints[mouseButton];
        touchPoint.setGlobalPressPosition(oldTouchPoint.globalPressPosition());
        touchPoint.setGlobalLastPosition(oldTouchPoint.globalPosition());
    }

    // Update current touch-point.
    touchPoint.setId(mouseButton);
    m_touchPoints.insert(mouseButton, touchPoint);
}


bool MiniBrowserApplication::sendTouchEvent(BrowserWindow* browserWindow, QEvent::Type type, ulong timestamp)
{
    static QPointingDevice* device = nullptr;
    if (!device) {
        device = new QPointingDevice;
        device->setType(QInputDevice::DeviceType::TouchScreen);
        QWindowSystemInterface::registerInputDevice(device);
    }

    m_pendingFakeTouchEventCount++;

    QList<QEventPoint> currentTouchPoints;
    QEventPoint::States touchPointStates{};
    Q_FOREACH (const QMutableEventPoint& touchPoint, m_touchPoints.values()) {
        currentTouchPoints.append(touchPoint);
        touchPointStates |= touchPoint.state();
    }

    QTouchEvent event(type, device, Qt::NoModifier, touchPointStates, currentTouchPoints);
    event.setTimestamp(timestamp);
    event.setAccepted(false);

    QGuiApplication::notify(browserWindow, &event);

    if (QQuickWebViewExperimental::flickableViewportEnabled())
        browserWindow->updateVisualMockTouchPoints(m_holdingControl ? currentTouchPoints : QList<QEventPoint>());

    // Get rid of touch-points that are no longer valid
    Q_FOREACH (const QEventPoint& touchPoint, currentTouchPoints) {
        if (touchPoint.state() == QEventPoint::Released)
            m_touchPoints.remove(touchPoint.id());
    }

    return event.isAccepted();
}

static void printHelp(const QString& programName)
{
    qDebug() << "Usage:" << programName.toLatin1().data()
         << "[--desktop]"
         << "[-r list]"
         << "[--remote-inspector [port|addr]]"
         << "[--robot-timeout seconds]"
         << "[--robot-extra-time seconds]"
         << "[--window-size (width)x(height)]"
         << "[--maximize]"
         << "[-f]                                    Full screen mode."
         << "[--user-agent string]"
         << "[-v]"
         << "URL";
}

void MiniBrowserApplication::handleUserOptions()
{
    QStringList args = arguments();
    QFileInfo program(args.takeAt(0));
    QString programName("MiniBrowser");
    if (program.exists())
        programName = program.baseName();

    if (takeOptionFlag(&args, "--help")) {
        printHelp(programName);
        appQuit(0);
    }

    const bool useDesktopBehavior = takeOptionFlag(&args, "--desktop");
    if (useDesktopBehavior)
        windowOptions()->setTouchMockingEnabled(false);

    // QTFIXME: flickable viewport has painting artifacts so we cannot enable it by default
    // QQuickWebViewExperimental::setFlickableViewportEnabled(!useDesktopBehavior);
    if (!useDesktopBehavior)
        qputenv("QT_WEBKIT_USE_MOBILE_THEME", QByteArray("1"));
    m_windowOptions.setPrintLoadedUrls(takeOptionFlag(&args, "-v"));
    m_windowOptions.setStartMaximized(takeOptionFlag(&args, "--maximize"));
    m_windowOptions.setStartFullScreen(takeOptionFlag(&args, "-f"));

    if (args.contains("--user-agent"))
        m_windowOptions.setUserAgent(takeOptionValue(&args, "--user-agent"));

    if (args.contains("--window-size")) {
        QString value = takeOptionValue(&args, "--window-size");
        QStringList list = value.split(QRegularExpression("\\D+"), Qt::SkipEmptyParts);
        if (list.length() == 2)
            m_windowOptions.setRequestedWindowSize(QSize(list.at(0).toInt(), list.at(1).toInt()));
    }

    if (args.contains("--remote-inspector")) {
        QString value = takeOptionValue(&args, "--remote-inspector");
        if (!value.isEmpty())
            qputenv("QTWEBKIT_INSPECTOR_SERVER", value.toUtf8());
    }

#if HAVE(QTTESTSUPPORT)
    if (takeOptionFlag(&args, QStringLiteral("--use-test-fonts")))
        WebKit::QtTestSupport::initializeTestFonts();
#endif

    if (args.contains("-r")) {
        QString listFile = takeOptionValue(&args, "-r");
        if (listFile.isEmpty())
            appQuit(1, "-r needs a list file to start in robotized mode");
        if (!QFile::exists(listFile))
            appQuit(1, "The list file supplied to -r does not exist.");

        m_isRobotized = true;
        m_urls = QStringList(listFile);

        // toInt() returns 0 if it fails parsing.
        m_robotTimeoutSeconds = takeOptionValue(&args, "--robot-timeout").toInt();
        m_robotExtraTimeSeconds = takeOptionValue(&args, "--robot-extra-time").toInt();
    } else {
        int urlArg;

        while ((urlArg = args.indexOf(QRegularExpression("^[^-].*"))) != -1)
            m_urls += args.takeAt(urlArg);
    }

    if (!args.isEmpty()) {
        printHelp(programName);
        appQuit(1, QString("Unknown argument(s): %1").arg(args.join(",")));
    }
}
