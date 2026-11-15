#include "core/ApplicationInfo.h"
#include "gui/MainWindow.h"

#include <QApplication>
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>

#include <cstdio>
#include <cstring>

int main(int argc, char *argv[])
{
    // Answered before any window system is touched, so a package or a
    // script can ask on a machine with no display.
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--version")) {
            std::printf("%s %s (build %s)\n", qPrintable(ts::core::applicationName()),
                        qPrintable(ts::core::applicationVersion()),
                        qPrintable(ts::core::applicationBuild()));
            return 0;
        }
        if (!std::strcmp(argv[i], "--help") || !std::strcmp(argv[i], "-h")) {
            std::printf("%s - %s\n\nUsage: TenSense [--version]\n\nThe window guides every step. "
                        "For analysis without a window, use ts-analyse.\n",
                        qPrintable(ts::core::applicationName()),
                        qPrintable(ts::core::applicationSubtitle()));
            return 0;
        }
    }

    // Must precede QApplication: QVTKOpenGLNativeWidget needs its own default
    // surface format to get a usable OpenGL context.
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());

    QApplication app(argc, argv);
    QApplication::setApplicationName(ts::core::applicationName());
    QApplication::setOrganizationName(QStringLiteral("Katalyst Nord"));
    QApplication::setApplicationVersion(ts::core::applicationVersion());
    QApplication::setDesktopFileName(QStringLiteral("tensense"));

    ts::gui::MainWindow window;
    window.show();
    return app.exec();
}
