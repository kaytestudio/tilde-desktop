HEADERS += rsslisting.h
SOURCES += main.cpp rsslisting.cpp
QT += network widgets
VERSION = 1.0.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
requires(qtConfig(treewidget))

# install
target.path = $$[QT_INSTALL_EXAMPLES]/network/rsslisting
INSTALLS += target
