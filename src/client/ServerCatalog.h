// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QJsonDocument>
#include <QString>
#include <QStringList>
class ServerCatalog {
public:
    static bool validName(const QString& name);
    static bool validGateway(const QString& text, QString* normalized = nullptr);
    static QJsonDocument exportProfiles(); // Never exports credentials, cookies, keys or certificates.
    static bool importProfiles(const QJsonDocument& document, QStringList& names, QString& error);
    static bool duplicate(const QString& source, const QString& target, QString& error);
};
