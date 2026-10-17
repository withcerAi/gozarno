// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>
// Allowlisted structured events, NOT a redaction pass over arbitrary engine logs.
// Never accepts profile names, endpoints, credentials, file paths or raw errors.
class DiagnosticReport {
public:
    DiagnosticReport();
    void record(const QString& event,const QJsonObject& values={});
    QJsonDocument document() const;
private:
    QElapsedTimer elapsed;
    QJsonArray events;
};
