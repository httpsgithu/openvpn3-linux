//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   sessionmgr-events.cpp
 *
 * @brief  Unit tests for SessionManager::Event
 */


#include <iostream>
#include <string>
#include <sstream>

#include <gtest/gtest.h>

#include "sessionmgr/sessionmgr-events.hpp"
#include "sessionmgr/sessionmgr-exceptions.hpp"


using namespace SessionManager;

namespace unittest {

TEST(SessionManagerEvent, init_empty)
{
    Event empty1;
    ASSERT_TRUE(empty1.empty()) << "Not an empty object without init";

    Event empty2{};
    ASSERT_TRUE(empty2.empty()) << "Not an empty object with {} init";
}


TEST(SessionManagerEvent, init_with_values)
{
    Event ev1{"/net/openvpn/v3/test/1", EventType::UNSET, 123};
    ASSERT_FALSE(ev1.empty()) << "Not initialized properly with EventType::UNSET";
    ASSERT_EQ(ev1.path, std::string("/net/openvpn/v3/test/1")) << "Invalid path";
    ASSERT_EQ(ev1.type, EventType::UNSET);
    ASSERT_EQ(ev1.owner, 123);
    ASSERT_EQ(Event::TypeStr(ev1.type), std::string("[UNSET]"));

    std::stringstream chk1;
    chk1 << ev1;
    std::string expect1("[UNSET]; owner: 123, path: /net/openvpn/v3/test/1");
    ASSERT_EQ(chk1.str(), expect1);


    Event ev2{"/net/openvpn/v3/test/2", EventType::SESS_CREATED, 234};
    ASSERT_FALSE(ev2.empty()) << "Not initialized properly with EventType::SESS_CREATED";
    ASSERT_EQ(ev2.path, std::string("/net/openvpn/v3/test/2")) << "Invalid path";
    ASSERT_EQ(ev2.type, EventType::SESS_CREATED);
    ASSERT_EQ(ev2.owner, 234);
    ASSERT_EQ(Event::TypeStr(ev2.type), std::string("Session created"));
    ASSERT_EQ(Event::TypeStr(ev2.type, true), "SESS_CREATED");

    std::stringstream chk2;
    chk2 << ev2;
    std::string expect2("Session created; owner: 234, path: /net/openvpn/v3/test/2");
    ASSERT_EQ(chk2.str(), expect2);

    Event ev3{"/net/openvpn/v3/test/3", EventType::SESS_DESTROYED, 456};
    ASSERT_FALSE(ev3.empty()) << "Not initialized properly with EventType::SESS_DESTROYED";
    ASSERT_EQ(ev3.path, std::string("/net/openvpn/v3/test/3")) << "Invalid path";
    ASSERT_EQ(ev3.type, EventType::SESS_DESTROYED);
    ASSERT_EQ(ev3.owner, 456);
    ASSERT_EQ(Event::TypeStr(ev3.type), std::string("Session destroyed"));
    ASSERT_EQ(Event::TypeStr(ev3.type, true), "SESS_DESTROYED");

    std::stringstream chk3;
    chk3 << ev3;
    std::string expect3("Session destroyed; owner: 456, path: /net/openvpn/v3/test/3");
    ASSERT_EQ(chk3.str(), expect3);
}

TEST(SessionManagerEvent, init_with_gvariant_valid)
{
    GVariantBuilder *b = glib2::Builder::Create("(oqu)");
    glib2::Builder::Add<DBus::Object::Path>(b, "/net/openvpn/v3/test/4");
    glib2::Builder::Add<uint16_t>(b, 1);
    glib2::Builder::Add<uint32_t>(b, 567);
    GVariant *data = glib2::Builder::Finish(b);

    Event ev(data);
    Event chk{"/net/openvpn/v3/test/4", EventType::SESS_CREATED, 567};
    ASSERT_EQ(ev, chk) << "Not an equal result with valid gvariant data";
    g_variant_unref(data);
}


TEST(SessionManagerEvent, init_with_gvariant_invalid)
{
    GVariantBuilder *b = glib2::Builder::Create("(si)");
    glib2::Builder::Add<std::string>(b, "/net/openvpn/v3/test/5");
    glib2::Builder::Add<int32_t>(b, 5);
    GVariant *data = glib2::Builder::Finish(b);

    ASSERT_THROW(Event ev(data), SessionManager::Exception);
    g_variant_unref(data);

    b = glib2::Builder::Create("(oqu)");
    glib2::Builder::Add<DBus::Object::Path>(b, "/net/openvpn/v3/test/6");
    glib2::Builder::Add<uint16_t>(b, 6);
    glib2::Builder::Add<uint32_t>(b, 678);
    data = glib2::Builder::Finish(b);

    ASSERT_THROW(Event ev(data), SessionManager::Exception);
    g_variant_unref(data);
}

TEST(SessionManagerEvent, gvariant_get)
{
    Event ev{"/net/openvpn/v3/test/7", EventType::SESS_DESTROYED, 789};
    GVariant *chk = ev.GetGVariant();
    std::string dump_check = glib2::Utils::DumpToString(chk);
    g_variant_unref(chk);

    std::string expect = "(objectpath '/net/openvpn/v3/test/7', uint16 2, uint32 789)";
    ASSERT_EQ(dump_check, expect);
}


} // namespace unittest
