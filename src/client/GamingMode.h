// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QString>
#include <memory>
class GamingMode {
public:
    GamingMode();
    ~GamingMode();
    bool enable(const QString& tunnelName, unsigned mtu, QString& error);
    bool disable(QString* error = nullptr);
    bool active() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
