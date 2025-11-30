//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   netcfg-changeeevent.cpp
 *
 * @brief  Unit test for struct NetCfgChangeEvent
 */

#include <iostream>
#include <string>
#include <sstream>
#include <gdbuspp/glib2/utils.hpp>

#include <gtest/gtest.h>

#include "netcfg/netcfg-changeevent.hpp"

namespace unittest {

std::string test_empty(const NetCfgChangeEvent &ev, const bool expect)
{
    bool r = ev.empty();

    if (expect != r)
    {
        return std::string("test_empty():  ")
               + "ev.empty() = " + (r ? "true" : "false")
               + " [expected: " + (expect ? "true" : "false") + "]";
    }

    r = (NetCfgChangeType::UNSET == ev.type
         && ev.device.empty()
         && ev.details.empty());
    if (expect != r)
    {
        return std::string("test_empty() - Member check:  ")
               + "(" + std::to_string((unsigned)ev.type) + ", "
               + "'" + ev.device + "', "
               + "details.size=" + std::to_string(ev.details.size()) + ") ..."
               + " is " + (r ? "EMPTY" : "NON-EMPTY")
               + " [expected: " + (expect ? "EMPTY" : "NON-EMPTY") + "]";
    }
    return "";
};


TEST(NetCfgChangeEvent, init_empty)
{
    NetCfgChangeEvent empty;
    std::string res = test_empty(empty, true);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(NetCfgChangeEvent, init_with_values)
{
    NetCfgChangeEvent populated1(NetCfgChangeType::DEVICE_ADDED,
                                 "test-dev",
                                 {{"some_key", "Some detail"}});

    std::string res = test_empty(populated1, false);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(NetCfgChangeEvent, reset)
{
    NetCfgChangeEvent populated1(NetCfgChangeType::DEVICE_ADDED,
                                 "test-dev",
                                 {{"some_key", "Some detail"}});
    populated1.reset();
    std::string res = test_empty(populated1, true);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(NetCfgChangeEvent, stringstream)
{
    // TODO: Simple compat-check - can be removed in v28+
    NetCfgChangeEvent event(NetCfgChangeType::IPADDR_ADDED,
                            "testdev",
                            {{"ip_address", "2001:db8:a050::1"}, {"prefix", "64"}, {"prefix_size", "64"}});
    std::stringstream chk;
    chk << event;
    std::string expect("Device testdev - IP Address Added: ip_address='2001:db8:a050::1', prefix='64', prefix_size='64'");

    EXPECT_TRUE(chk.str() == expect);

    NetCfgChangeEvent event2(NetCfgChangeType::IPADDR_ADDED,
                             "testdev",
                             {{"ip_address", "2001:db8:a050::1"}, {"prefix_size", "64"}});
    std::stringstream chk2;
    chk2 << event2;
    std::string expect2("Device testdev - IP Address Added: ip_address='2001:db8:a050::1', prefix_size='64'");

    EXPECT_TRUE(chk2.str() == expect2);
}


TEST(NetCfgChangeEvent, gvariant_GetVariant)
{
    NetCfgChangeEvent g_state(NetCfgChangeType::ROUTE_ADDED,
                              "tun22",
                              {{"ip_address", "2001:db8:a050::1"}, {"prefix_size", "64"}});
    GVariant *chk = g_state.GetGVariant();
    std::string dump_check = glib2::Utils::DumpToString(chk);
    g_variant_unref(chk);

    std::string expect = "(uint32 16, 'tun22', {'ip_address': '2001:db8:a050::1', 'prefix_size': '64'})";
    ASSERT_EQ(dump_check, expect);
}


TEST(NetCfgChangeEvent, gvariant_get)
{
    NetCfgChangeEvent g_state(NetCfgChangeType::ROUTE_ADDED,
                              "tun22",
                              {{"ip_address", "2001:db8:a050::1"}, {"prefix_size", "64"}});
    GVariant *chk = g_state.GetGVariant();

    auto type = glib2::Value::Extract<NetCfgChangeType>(chk, 0);
    auto dev_s = glib2::Value::Extract<std::string>(chk, 1);
    GVariant *details = glib2::Value::ExtractChild(chk, 2);
    g_variant_unref(chk);

    NetCfgChangeDetails det_s;
    auto parse_details = [&det_s](GVariant *record)
    {
        auto key = glib2::Value::Extract<std::string>(record, 0);
        auto value = glib2::Value::Extract<std::string>(record, 1);
        det_s[key] = value;
    };
    glib2::Value::IterateArray(details, parse_details);
    g_variant_unref(details);

    ASSERT_EQ(type, g_state.type);
    ASSERT_EQ(dev_s, g_state.device);
    ASSERT_EQ(det_s, g_state.details);
}


TEST(NetCfgChangeEvent, parse_gvariant_valid_data)
{
    NetCfgChangeEvent event(NetCfgChangeType::ROUTE_ADDED,
                            "tun33",
                            {{"ip_address", "2001:db8:a050::1"}, {"prefix_size", "64"}});

    //
    // Build up an GVariant object similar to what we would retrieve
    // in the various GLib2/GDBus calls when receiving this kind of event
    //
    // All values below here must match the values as provided in event
    //
    GVariantBuilder *b = glib2::Builder::Create("(usa{ss})");
    glib2::Builder::Add(b, event.type);
    glib2::Builder::Add(b, event.device);

    // Add the details - key/value dictionary
    glib2::Builder::OpenChild(b, "a{ss}");

    // Add IP address / prefix
    glib2::Builder::AddKeyValue<std::string, std::string>(b, "ip_address", event.details["ip_address"]);
    glib2::Builder::AddKeyValue<std::string, std::string>(b, "prefix_size", event.details["prefix_size"]);

    // Details ready
    glib2::Builder::CloseChild(b);

    // Retrieve the proper GVariant object
    GVariant *chk = glib2::Builder::Finish(b);

    NetCfgChangeEvent parsed(chk);
    g_variant_unref(chk);
    ASSERT_EQ(parsed, event);
}


TEST(NetCfgChangeEvent, parse_gvariant_invalid_data)
{
    GVariantBuilder *params = glib2::Builder::Create("(uus)");
    glib2::Builder::Add<uint32_t>(params, 123);
    glib2::Builder::Add<uint32_t>(params, 456);
    glib2::Builder::Add<std::string>(params, "Invalid data");
    GVariant *invalid = glib2::Builder::Finish(params);
    ASSERT_THROW(NetCfgChangeEvent invalid_event(invalid),
                 NetCfgException);
    g_variant_unref(invalid);
}

std::string test_compare(const NetCfgChangeEvent &lhs,
                         const NetCfgChangeEvent &rhs,
                         const bool expect)
{
    bool r = (lhs.type == rhs.type
              && lhs.device == rhs.device
              && lhs.details == rhs.details);
    if (r != expect)
    {
        std::stringstream err;
        err << "NetCfgChangeEvent compare check FAIL: "
            << "{" << lhs << "} == {" << rhs << "} returned "
            << (r ? "true" : "false")
            << " - expected: "
            << (expect ? "true" : "false");
        return err.str();
    }

    r = (lhs.type != rhs.type
         || lhs.device != rhs.device
         || lhs.details != rhs.details);
    if (r == expect)
    {
        std::stringstream err;
        err << "Negative NetCfgChangeEvent compare check FAIL: "
            << "{" << lhs << "} == {" << rhs << "} returned "
            << (r ? "true" : "false")
            << " - expected: "
            << (expect ? "true" : "false");
        return err.str();
    }
    return "";
}


TEST(NetCfgChangeEvent, compare_eq)
{
    NetCfgChangeEvent s1(NetCfgChangeType::ROUTE_ADDED,
                         "tun22",
                         {{"ip_address", "2001:db8:a050::1"}, {"prefix_size", "64"}});
    NetCfgChangeEvent s2(NetCfgChangeType::ROUTE_ADDED,
                         "tun22",
                         {{"ip_address", "2001:db8:a050::1"}, {"prefix_size", "64"}});
    std::string res = test_compare(s1, s2, true);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(NetCfgChangeEvent, operator_eq)
{
    NetCfgChangeEvent s1(NetCfgChangeType::ROUTE_ADDED,
                         "tun22",
                         {{"ip_address", "2001:db8:a050::1"}, {"prefix_size", "64"}});
    NetCfgChangeEvent s2(NetCfgChangeType::ROUTE_ADDED,
                         "tun22",
                         {{"ip_address", "2001:db8:a050::1"}, {"prefix_size", "64"}});
    ASSERT_TRUE(s1 == s2);
}

TEST(NetCfgChangeEvent, compare_neq)
{
    NetCfgChangeEvent s1(NetCfgChangeType::ROUTE_ADDED,
                         "tun22",
                         {{"ip_address", "2001:db8:a050::1"}, {"prefix_size", "64"}});
    NetCfgChangeEvent s2(NetCfgChangeType::DNS_SERVER_REMOVED,
                         "tun11",
                         {{"dns_server", "127.0.0.1"}});
    std::string res = test_compare(s1, s2, false);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(NetCfgChangeEvent, operator_neq)
{
    NetCfgChangeEvent s1(NetCfgChangeType::ROUTE_ADDED,
                         "tun22",
                         {{"ip_address", "2001:db8:a050::1"}, {"prefix_size", "64"}});
    NetCfgChangeEvent s2(NetCfgChangeType::DNS_SERVER_REMOVED,
                         "tun11",
                         {{"dns_server", "127.0.0.1"}});
    ASSERT_TRUE(s1 != s2);
}


TEST(NetCfgChangeEvent, FilterMaskList)
{
    uint16_t mask = (1 << 16) - 1;
    std::vector<std::string> all_set = NetCfgChangeEvent::FilterMaskList(mask, true);
    ASSERT_EQ(all_set.size(), 16) << "Unexpected size of return vector";

    mask = 2 | 16 | 2048 | 8192;
    std::vector<std::string> selection = NetCfgChangeEvent::FilterMaskList(mask, true);
    ASSERT_EQ(selection.size(), 4) << "Expected only 4 elements to be returned";

    mask = 0;
    std::vector<std::string> none = NetCfgChangeEvent::FilterMaskList(mask, true);
    ASSERT_EQ(none.size(), 0) << "Expected no elements to be returned";
}


} // namespace unittest
