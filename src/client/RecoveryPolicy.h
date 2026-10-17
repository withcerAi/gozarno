// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QtGlobal>

// A successful probe resets failures; recovery remains paced even when every
// destination is unreachable. No idle-traffic or single-endpoint heuristics.
struct RecoveryPolicy {
    int failures = 0;
    int attempts = 0;
    qint64 nextAttempt = 0;
    void reset() { failures = attempts = 0; nextAttempt = 0; }
    bool result(bool healthy, qint64 now, bool automatic) {
        if (healthy) { reset(); return false; }
        ++failures;
        if (!automatic || failures < 3 || now < nextAttempt) return false;
        const int delay = qMin(300, 30 * (1 << qMin(attempts, 4)));
        ++attempts;
        nextAttempt = now + qint64(delay) * 1000;
        return true;
    }
};
