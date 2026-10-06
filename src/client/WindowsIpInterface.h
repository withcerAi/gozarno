// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>

namespace WindowsIpInterface {
inline MIB_IPINTERFACE_ROW withMtu(MIB_IPINTERFACE_ROW row, ULONG mtu)
{
    row.NlMtu = mtu;
    // GetIpInterfaceEntry can return a nonzero IPv4 SitePrefixLength, but
    // SetIpInterfaceEntry requires zero for IPv4 (otherwise error 87).
    // Preserve all other queried settings, including the IPv6 site prefix.
    if (row.Family == AF_INET) row.SitePrefixLength = 0;
    return row;
}
}
#endif
