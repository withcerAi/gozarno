// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "TrafficPolicy.h"
#include <QString>
#include <memory>
#include <functional>
class VpnRoutes;
class AppRouter {
public:
    AppRouter();
    ~AppRouter();
    static bool preflight(const TrafficPolicy& policy, QString& error);
    bool start(const TrafficPolicy& policy, const VpnRoutes& routes, QString& error, std::function<void()> onFailure = {});
    void stop();
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
