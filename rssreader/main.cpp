// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "rsslisting.h"
#include <QtWidgets>
using namespace Qt::StringLiterals;

//! [0]
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"RSS Reader"_s);
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    RSSListing rsslisting(u"https://www.qt.io/blog/rss.xml"_s);
    rsslisting.show();
    return app.exec();
}
//! [0]

