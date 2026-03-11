//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2020 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 *  @file   src/tests/unit/cached-config-tests.cpp
 *
 *  @brief  Unit tests for the
 */

#include "build-config.h"

#include <sstream>
#include <gtest/gtest.h>
#include <gdbuspp/glib2/utils.hpp>

#include "log/core-dbus-logger.hpp"
#include <openvpn/netconf/linux/gwnetlink.hpp>
#include "netcfg/cached-net-config.hpp"

using namespace NetCfg;

namespace unittest {

TEST(CachedNetworkConfig, create_empty)
{
    // This should not cause any crash nor leak any memory
    auto empty = CachedNetworkConfig::Create();

    std::stringstream cfg_chk;
    cfg_chk << "{'remote': <@mv nothing>, "
            << "'gateway': <@mv nothing>, "
            << "'vpn_addresses': <@mv nothing>, "
            << "'routes': <{'include': <@a{sv} {}>, 'exclude': <@a{sv} {}>}>}";
    GVariant *net_config = empty->GetNetworkConfig();
    std::string cfg_str = glib2::Utils::DumpToString(net_config, false);
    EXPECT_STREQ(cfg_str.c_str(), cfg_chk.str().c_str());
}


TEST(CachedNetworkConfig, add_remote)
{
    // Disable CoreLog by enabling a dummy-logger
    // The log events there are not interesting for us.
    LogSender::Ptr log = std::make_shared<LogSender>();
    CoreLog::Connect(log);

    auto cfg = CachedNetworkConfig::Create();

    const std::string dev = "test_dev";
    const std::string remote_host = "10.9.0.1";
    openvpn::LinuxGW46Netlink gw(dev, remote_host);

    std::stringstream cfg_chk;
    cfg_chk << "{'remote': <'" << remote_host << "'>, "
            << "'gateway': <{'IPv4': <'" << gw.v4.addr().to_string() << "'>}>, "
            << "'vpn_addresses': <@mv nothing>, "
            << "'routes': <{'include': <@a{sv} {}>, 'exclude': <@a{sv} {}>}>}";

    cfg->AddRemote(dev, remote_host);
    GVariant *net_config = cfg->GetNetworkConfig();
    std::string cfg_str = glib2::Utils::DumpToString(net_config, false);
    EXPECT_STREQ(cfg_str.c_str(), cfg_chk.str().c_str());
}


TEST(CachedNetworkConfig, add_vpn_address)
{
    auto cfg = CachedNetworkConfig::Create();

    VPNAddress addr4 = {"10.3.0.0", 24, "10.8.0.1", false};
    cfg->AddVPNaddress(addr4, "10.0.0.1", "3fff:dead:cafe::1");
    VPNAddress addr6 = {"3fff:feed:beef::2345", 64, "3fff:feed:beef::1", true};
    cfg->AddVPNaddress(addr6, "10.0.0.1", "3fff:dead:cafe::1");

    std::stringstream cfg_chk;
    cfg_chk << "{'remote': <@mv nothing>, "
            << "'gateway': <@mv nothing>, "
            << "'vpn_addresses': <[('10.3.0.0', uint32 24, '10.0.0.1'), ('3fff:feed:beef::2345', 64, '3fff:dead:cafe::1')]>, "
            << "'routes': <{'include': <@a{sv} {}>, 'exclude': <@a{sv} {}>}>}";


    GVariant *net_config = cfg->GetNetworkConfig();
    std::string cfg_str = glib2::Utils::DumpToString(net_config, false);
    EXPECT_STREQ(cfg_str.c_str(), cfg_chk.str().c_str());
}


TEST(CachedNetworkConfig, add_vpn_route)
{
    auto cfg = CachedNetworkConfig::Create();

    std::vector<Network> routes = {
        {"10.4.0.0", 23, -1, false, false},
        {"10.8.0.0", 20, 33, false, false},
        {"10.6.0.0", 25, -1, false, true},
        {"10.23.0.0", 25, 44, false, true},
        {"3fff:1234:aaaa::", 64, -1, true, false},
        {"3fff:1234:bbbb::", 48, 10, true, false},
        {"3fff:456:c0001::", 48, -1, true, true},
        {"3fff:4567:fff1::", 128, 55, true, true},
    };

    for (const auto &rout : routes)
    {
        cfg->AddVPNroute(rout, "192.168.55.1", "3fff::1");
    }

    std::stringstream cfg_chk;
    cfg_chk << "{'remote': <@mv nothing>, "
            << "'gateway': <@mv nothing>, "
            << "'vpn_addresses': <@mv nothing>, "
            << "'routes': <"
            << "{'include': <{'IPv4': <[('10.4.0.0', uint32 23, int16 -1), ('10.8.0.0', 20, 33)]>, '"
            << "IPv6': <[('3fff:1234:aaaa::', uint32 64, int16 -1), ('3fff:1234:bbbb::', 48, 10)]>}>, "
            << "'exclude': <{'IPv4': <[('10.6.0.0', uint32 25, int16 -1), ('10.23.0.0', 25, 44)]>, "
            << "'IPv6': <[('3fff:456:c0001::', uint32 48, int16 -1), ('3fff:4567:fff1::', 128, 55)]>}>}>}";

        GVariant *net_config = cfg->GetNetworkConfig();
    std::string cfg_str = glib2::Utils::DumpToString(net_config, false);
    EXPECT_STREQ(cfg_str.c_str(), cfg_chk.str().c_str());
}

TEST(CachedNetworkConfig, full_config_unconsumed)
{
    auto cfg = CachedNetworkConfig::Create();

    cfg->AddRemote("test_dev", "10.9.0.1");

    VPNAddress addr4 = {"10.3.0.0", 24, "10.8.0.1", false};
    cfg->AddVPNaddress(addr4, "10.0.0.1", "3fff:dead:cafe::1");
    VPNAddress addr6 = {"3fff:feed:beef::2345", 64, "3fff:feed:beef::1", true};
    cfg->AddVPNaddress(addr6, "10.0.0.1", "3fff:dead:cafe::1");

    std::vector<Network> routes = {
        {"10.4.0.0", 23, -1, false, false},
        {"10.8.0.0", 20, 33, false, false},
        {"10.6.0.0", 25, -1, false, true},
        {"10.23.0.0", 25, 44, false, true},
        {"3fff:1234:aaaa::", 64, -1, true, false},
        {"3fff:1234:bbbb::", 48, 10, true, false},
        {"3fff:456:c0001::", 48, -1, true, true},
        {"3fff:4567:fff1::", 128, 55, true, true},
    };

    for (const auto &rout : routes)
    {
        cfg->AddVPNroute(rout, "192.168.55.1", "3fff::1");
    }

    // This test does not really provide any test output.  It is to ensure
    // it doesn't crash if the collected data is not consumed in any way
    // and running via valgrind will ensure there are no memory leaks
    EXPECT_TRUE(true);
}


} // namespace unittest