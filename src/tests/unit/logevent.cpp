//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   logevent.cpp
 *
 * @brief  Unit test for struct LogEvent
 */

#include <iostream>
#include <string>
#include <sstream>

#include <gtest/gtest.h>

#include "build-config.h"
#include "events/status.hpp"
#include "events/log.hpp"


namespace unittest {

std::string test_empty(const Events::Log &ev, const bool expect)
{
    bool r = ev.empty();
    if (expect != r)
    {
        return std::string("test_empty():  ")
               + "ev.empty() = " + (r ? "true" : "false")
               + " [expected: " + (expect ? "true" : "false") + "]";
    }

    r = (LogGroup::UNDEFINED == ev.group
         && LogCategory::UNDEFINED == ev.category
         && ev.empty(true));
    if (expect != r)
    {
        return std::string("test_empty() - Member check:  ")
               + "(" + std::to_string((unsigned)ev.group) + ", "
               + std::to_string((unsigned)ev.category) + "', "
               + "'" + ev.GetMessage() + "', "
               + "message.size=" + std::to_string(ev.GetMessage().size()) + ") ..."
               + " is " + (r ? "EMPTY" : "NON-EMPTY")
               + " [expected: " + (expect ? "EMPTY" : "NON-EMPTY") + "]";
    }
    return "";
};


TEST(LogEvent, init_empty)
{
    Events::Log empty;
    std::string res = test_empty(empty, true);
    ASSERT_TRUE(res.empty()) << res;
    ASSERT_EQ(empty.format, Events::Log::Format::AUTO);
}


TEST(LogEvent, init_with_value_1)
{
    Events::Log ev(LogGroup::LOGGER, LogCategory::DEBUG, "Test LogEvent");
    std::string res = test_empty(ev, false);
    ASSERT_TRUE(res.empty()) << res;
    ASSERT_EQ(ev.format, Events::Log::Format::NORMAL);
}


TEST(LogEvent, reset)
{
    Events::Log ev(LogGroup::LOGGER, LogCategory::DEBUG, "Test LogEvent");
    ev.reset();
    std::string res = test_empty(ev, true);
    ASSERT_TRUE(res.empty()) << res;
    ASSERT_EQ(ev.format, Events::Log::Format::AUTO);
}


TEST(LogEvent, init_with_session_token)
{
    Events::Log ev(LogGroup::LOGGER, LogCategory::INFO, "session_token_value", "Log message");
    std::string res = test_empty(ev, false);
    ASSERT_TRUE(res.empty()) << res;
    ASSERT_EQ(ev.format, Events::Log::Format::SESSION_TOKEN);
}


TEST(LogEvent, reset_with_session_token)
{
    Events::Log ev(LogGroup::LOGGER, LogCategory::INFO, "session_token_value", "Log message");
    ev.reset();
    std::string res = test_empty(ev, true);
    ASSERT_TRUE(res.empty()) << res;
    ASSERT_EQ(ev.format, Events::Log::Format::AUTO);
}


TEST(LogEvent, log_group_str)
{
    for (uint8_t g = 0; g <= LogGroupCount; g++)
    {
        Events::Log ev((LogGroup)g, LogCategory::UNDEFINED, "irrelevant message");
        std::string expstr((g < LogGroupCount ? LogGroup_str[g] : "[group:10]"));
        EXPECT_STREQ(ev.GetLogGroupStr().c_str(), expstr.c_str());
    }
}


TEST(LogEvent, log_category_str)
{
    for (uint8_t c = 0; c < 10; c++)
    {
        Events::Log ev(LogGroup::UNDEFINED, (LogCategory)c, "irrelevant message");
        std::string expstr((c < 9 ? LogCategory_str[c] : "[category:9]"));
        EXPECT_STREQ(ev.GetLogCategoryStr().c_str(), expstr.c_str());
    }
}


TEST(LogEvent, str_keepnl)
{
    std::string msg;
    for (uint8_t i = 0; i < 10; i++)
    {
        msg += fmt::format("Test Line {}\n", i + 1);
    }
    auto ev_keepnl = Events::Log(LogGroup::LOGGER, LogCategory::DEBUG, msg).KeepNL();

    std::string chk_indent = "Logger DEBUG: Test Line 1\n    Test Line 2\n    Test Line 3\n"
                             "    Test Line 4\n    Test Line 5\n    Test Line 6\n    Test Line 7\n"
                             "    Test Line 8\n    Test Line 9\n    Test Line 10";
    EXPECT_STREQ(ev_keepnl.str(4).c_str(), chk_indent.c_str()) << "FAILED: Event::Log::str(4)";

    // The std::string operator() uses 4 space indents by default
    std::string test_str = ev_keepnl;
    EXPECT_STREQ(test_str.c_str(), chk_indent.c_str()) << "FAILED: std::string Event::Log::operator()";

    std::string chk = "Logger DEBUG: Test Line 1\nTest Line 2\nTest Line 3\n"
                      "Test Line 4\nTest Line 5\nTest Line 6\nTest Line 7\n"
                      "Test Line 8\nTest Line 9\nTest Line 10";
    EXPECT_STREQ(ev_keepnl.str().c_str(), chk.c_str()) << "FAILED: Event::Log::str()";
}


TEST(LogEvent, str_no_keepnl)
{
    std::string msg;
    for (uint8_t i = 0; i < 10; i++)
    {
        msg += fmt::format("Test Line {}\n", i + 1);
    }
    auto ev_nonl = Events::Log(LogGroup::LOGGER, LogCategory::DEBUG, msg);

    std::string chk = "Logger DEBUG: Test Line 1Test Line 2Test Line 3"
                      "Test Line 4Test Line 5Test Line 6Test Line 7"
                      "Test Line 8Test Line 9Test Line 10";
    EXPECT_STREQ(ev_nonl.str(4).c_str(), chk.c_str()) << "FAILED: Event::Log::str(4)";
    EXPECT_STREQ(ev_nonl.str().c_str(), chk.c_str()) << "FAILED: Event::Log::str()";

    // The std::string operator() uses 4 space indents by default
    std::string test_str = ev_nonl;
    EXPECT_STREQ(test_str.c_str(), chk.c_str()) << "FAILED: std::string Event::Log::operator()";
}


TEST(LogEvent, message_keepnl)
{
    std::string msg;
    for (uint8_t i = 0; i < 10; i++)
    {
        msg += fmt::format("Test Line {}\n", i + 1);
    }
    auto ev_keepnl = Events::Log(LogGroup::LOGGER, LogCategory::DEBUG, msg).KeepNL();

    std::string chk_indent = "Test Line 1\n    Test Line 2\n    Test Line 3\n"
                             "    Test Line 4\n    Test Line 5\n    Test Line 6\n    Test Line 7\n"
                             "    Test Line 8\n    Test Line 9\n    Test Line 10";
    EXPECT_STREQ(ev_keepnl.GetMessage(4).c_str(), chk_indent.c_str()) << "FAILED: Event::Log::message(4)";

    std::string chk = "Test Line 1\nTest Line 2\nTest Line 3\n"
                      "Test Line 4\nTest Line 5\nTest Line 6\nTest Line 7\n"
                      "Test Line 8\nTest Line 9\nTest Line 10";
    EXPECT_STREQ(ev_keepnl.GetMessage().c_str(), chk.c_str()) << "FAILED: Event::Log::message()";
}


TEST(LogEvent, message_no_keepnl)
{
    std::string msg;
    for (uint8_t i = 0; i < 10; i++)
    {
        msg += fmt::format("Test Line {}\n", i + 1);
    }
    auto ev_nonl = Events::Log(LogGroup::LOGGER, LogCategory::DEBUG, msg);

    std::string chk = "Test Line 1Test Line 2Test Line 3"
                      "Test Line 4Test Line 5Test Line 6Test Line 7"
                      "Test Line 8Test Line 9Test Line 10";
    EXPECT_STREQ(ev_nonl.GetMessage(4).c_str(), chk.c_str()) << "FAILED: Event::Log::message(4)";
    EXPECT_STREQ(ev_nonl.GetMessage().c_str(), chk.c_str()) << "FAILED: Event::Log::message()";
}



TEST(LogEvent, parse_gvariant_tuple_invalid)
{
    GVariantBuilder *params = glib2::Builder::Create("(uuis)");
    glib2::Builder::Add(params, StatusMajor::CONFIG);
    glib2::Builder::Add(params, StatusMinor::CFG_OK);
    glib2::Builder::Add<int32_t>(params, 1234);
    glib2::Builder::Add<std::string>(params, "Invalid data");
    GVariant *data = glib2::Builder::Finish(params);
    ASSERT_THROW(auto parsed = Events::ParseLog(data), LogException);
    if (nullptr != data)
    {
        g_variant_unref(data);
    }
}


TEST(LogEvent, parse_gvariant_dict)
{
    GVariantDict *dict = glib2::Dict::Create();
    glib2::Dict::Add(dict, "log_group", LogGroup::LOGGER);
    glib2::Dict::Add(dict, "log_category", LogCategory::DEBUG);
    glib2::Dict::Add<std::string>(dict, "log_message", "Test log message");
    GVariant *data = glib2::Dict::Finish(dict);

    auto parsed = Events::ParseLog(data);
    g_variant_unref(data);

    ASSERT_EQ(parsed.group, LogGroup::LOGGER);
    ASSERT_EQ(parsed.category, LogCategory::DEBUG);
    ASSERT_EQ(parsed.GetMessage(), "Test log message");
    ASSERT_EQ(parsed.format, Events::Log::Format::NORMAL);
}


TEST(LogEvent, parse_gvariant_tuple)
{
    GVariantBuilder *params = glib2::Builder::Create("(uus)");
    glib2::Builder::Add(params, LogGroup::BACKENDPROC);
    glib2::Builder::Add(params, LogCategory::INFO);
    glib2::Builder::Add<std::string>(params, "Parse testing again");
    GVariant *data = glib2::Builder::Finish(params);
    auto parsed = Events::ParseLog(data);
    g_variant_unref(data);

    ASSERT_EQ(parsed.group, LogGroup::BACKENDPROC);
    ASSERT_EQ(parsed.category, LogCategory::INFO);
    ASSERT_EQ(parsed.GetMessage(), "Parse testing again");
    ASSERT_EQ(parsed.format, Events::Log::Format::NORMAL);
}


TEST(LogEvent, GetVariantTuple)
{
    Events::Log reverse(LogGroup::BACKENDSTART, LogCategory::WARN, "Yet another test");
    GVariant *revparse = reverse.GetGVariantTuple();

    auto grp = glib2::Value::Extract<LogGroup>(revparse, 0);
    auto ctg = glib2::Value::Extract<LogCategory>(revparse, 1);
    auto msg = glib2::Value::Extract<std::string>(revparse, 2);

    ASSERT_EQ(reverse.group, grp);
    ASSERT_EQ(reverse.category, ctg);
    ASSERT_EQ(reverse.GetMessage(), msg);
    g_variant_unref(revparse);
}


TEST(LogEvent, GetVariantDict)
{
    Events::Log dicttest(LogGroup::CLIENT, LogCategory::ERROR, "Moar testing is needed");
    GVariant *revparse = dicttest.GetGVariantDict();

    // Reuse the parser in LogEvent.  As that has already passed the
    // test, expect this to work too.
    auto cmp = Events::ParseLog(revparse);
    g_variant_unref(revparse);

    ASSERT_EQ(cmp.group, dicttest.group);
    ASSERT_EQ(cmp.category, dicttest.category);
    ASSERT_EQ(cmp.GetMessage(), dicttest.GetMessage());
}


TEST(LogEvent, parse_gvariant_dict_session_token)
{
    GVariantDict *dict = glib2::Dict::Create();
    glib2::Dict::Add(dict, "log_group", LogGroup::LOGGER);
    glib2::Dict::Add(dict, "log_category", LogCategory::DEBUG);
    glib2::Dict::Add<std::string>(dict, "log_session_token", "session_token_value");
    glib2::Dict::Add<std::string>(dict, "log_message", "Test log message");
    GVariant *data = glib2::Dict::Finish(dict);
    auto parsed = Events::ParseLog(data);
    g_variant_unref(data);

    ASSERT_EQ(parsed.group, LogGroup::LOGGER);
    ASSERT_EQ(parsed.category, LogCategory::DEBUG);
    ASSERT_EQ(parsed.session_token, "session_token_value");
    ASSERT_EQ(parsed.GetMessage(), "Test log message");
    ASSERT_EQ(parsed.format, Events::Log::Format::SESSION_TOKEN);
}


TEST(LogEvent, parse_gvariant_tuple_session_token)
{
    GVariantBuilder *params = glib2::Builder::Create("(uuss)");
    glib2::Builder::Add(params, LogGroup::BACKENDPROC);
    glib2::Builder::Add(params, LogCategory::INFO);
    glib2::Builder::Add<std::string>(params, "session_token_val");
    glib2::Builder::Add<std::string>(params, "Parse testing again");
    GVariant *data = glib2::Builder::Finish(params);
    auto parsed = Events::ParseLog(data);
    g_variant_unref(data);

    ASSERT_EQ(parsed.group, LogGroup::BACKENDPROC);
    ASSERT_EQ(parsed.category, LogCategory::INFO);
    ASSERT_EQ(parsed.session_token, "session_token_val");
    ASSERT_EQ(parsed.GetMessage(), "Parse testing again");
    ASSERT_EQ(parsed.format, Events::Log::Format::SESSION_TOKEN);
}


TEST(LogEvent, GetVariantTuple_session_token)
{
    Events::Log reverse(LogGroup::BACKENDSTART, LogCategory::WARN, "YetAnotherSessionToken", "Yet another test");
    GVariant *revparse = reverse.GetGVariantTuple();

    auto grp = glib2::Value::Extract<LogGroup>(revparse, 0);
    auto ctg = glib2::Value::Extract<LogCategory>(revparse, 1);
    auto sesstok = glib2::Value::Extract<std::string>(revparse, 2);
    auto msg = glib2::Value::Extract<std::string>(revparse, 3);

    ASSERT_EQ(grp, reverse.group);
    ASSERT_EQ(ctg, reverse.category);
    ASSERT_EQ(sesstok, reverse.session_token);
    ASSERT_EQ(msg, reverse.GetMessage());
    g_variant_unref(revparse);
}


TEST(LogEvent, GetVariantDict_session_token)
{
    Events::Log dicttest(LogGroup::CLIENT, LogCategory::ERROR, "MoarSessionTokens", "Moar testing is needed");
    GVariant *revparse = dicttest.GetGVariantDict();

    // Reuse the parser in LogEvent.  As that has already passed the
    // test, expect this to work too.
    auto cmp = Events::ParseLog(revparse);
    g_variant_unref(revparse);

    ASSERT_EQ(cmp.group, dicttest.group);
    ASSERT_EQ(cmp.category, dicttest.category);
    ASSERT_EQ(cmp.session_token, dicttest.session_token);
    ASSERT_EQ(cmp.GetMessage(), dicttest.GetMessage());
    ASSERT_EQ(cmp.format, dicttest.format);
}


std::string test_compare(const Events::Log &lhs, const Events::Log &rhs, const bool expect)
{
    bool r = (lhs.group == rhs.group
              && lhs.category == rhs.category
              && lhs.session_token == rhs.session_token
              && lhs.GetMessage() == rhs.GetMessage());
    if (r != expect)
    {
        std::stringstream err;
        err << "LogEvent compare check FAIL: "
            << "{" << lhs << "} == {" << rhs << "} returned "
            << (r ? "true" : "false")
            << " - expected: "
            << (expect ? "true" : "false");
        return err.str();
    }

    r = (lhs.group != rhs.group
         || lhs.category != rhs.category
         || lhs.session_token != rhs.session_token
         || lhs.GetMessage() != rhs.GetMessage());
    if (r == expect)
    {
        std::stringstream err;
        err << "Negative LogEvent compare check FAIL: "
            << "{" << lhs << "} == {" << rhs << "} returned "
            << (r ? "true" : "false")
            << " - expected: "
            << (expect ? "true" : "false");
        return err.str();
    }
    return "";
}


TEST(LogEvent, compare_eq)
{
    Events::Log ev(LogGroup::SESSIONMGR, LogCategory::FATAL, "var1");
    Events::Log cmp(LogGroup::SESSIONMGR, LogCategory::FATAL, "var1");
    std::string res = test_compare(ev, cmp, true);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(LogEvent, operator_eq)
{
    Events::Log ev(LogGroup::SESSIONMGR, LogCategory::FATAL, "var1");
    Events::Log cmp(LogGroup::SESSIONMGR, LogCategory::FATAL, "var1");
    ASSERT_TRUE(ev == cmp);
}


TEST(LogEvent, compare_neq_group)
{
    Events::Log ev(LogGroup::BACKENDSTART, LogCategory::DEBUG, "var1");
    Events::Log cmp(LogGroup::BACKENDPROC, LogCategory::DEBUG, "var1");
    std::string res = test_compare(ev, cmp, false);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(LogEvent, operator_neq_group)
{
    Events::Log ev(LogGroup::BACKENDSTART, LogCategory::DEBUG, "var1");
    Events::Log cmp(LogGroup::BACKENDPROC, LogCategory::DEBUG, "var1");
    ASSERT_TRUE(ev != cmp);
}


TEST(LogEvent, compare_neq_category)
{
    Events::Log ev(LogGroup::CONFIGMGR, LogCategory::ERROR, "var4");
    Events::Log cmp(LogGroup::CONFIGMGR, LogCategory::WARN, "var4");
    std::string res = test_compare(ev, cmp, false);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(LogEvent, operator_neq_category)
{
    Events::Log ev(LogGroup::CONFIGMGR, LogCategory::ERROR, "var4");
    Events::Log cmp(LogGroup::CONFIGMGR, LogCategory::WARN, "var4");
    ASSERT_TRUE(ev != cmp);
}


TEST(LogEvent, compare_neq_message)
{
    Events::Log ev(LogGroup::CONFIGMGR, LogCategory::ERROR, "var4");
    Events::Log cmp(LogGroup::CONFIGMGR, LogCategory::ERROR, "different");
    std::string res = test_compare(ev, cmp, false);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(LogEvent, operator_neq_message)
{
    Events::Log ev(LogGroup::CONFIGMGR, LogCategory::ERROR, "var4");
    Events::Log cmp(LogGroup::CONFIGMGR, LogCategory::ERROR, "different");
    ASSERT_TRUE(ev != cmp);
}


TEST(LogEvent, group_category_string_parser)
{
    for (uint8_t g = 0; g < LogGroupCount; g++)
    {
        for (uint8_t c = 0; c < 9; c++)
        {
            std::string msg1 = "Message without session token";
            auto ev1 = Events::ParseLog(LogGroup_str[g], LogCategory_str[c], msg1);
            Events::Log chk_ev1((LogGroup)g, (LogCategory)c, msg1);
            std::string res1 = test_compare(ev1, chk_ev1, true);
            EXPECT_TRUE(res1.empty()) << res1;

            std::string msg2 = "Message with session token";
            std::string sesstok = "SESSION_TOKEN";
            auto ev2 = Events::ParseLog(LogGroup_str[g], LogCategory_str[c], sesstok, msg2);
            Events::Log chk_ev2((LogGroup)g, (LogCategory)c, sesstok, msg2);
            std::string res2 = test_compare(ev2, chk_ev2, true);
            EXPECT_TRUE(res2.empty()) << res2;
        }
    }
}


TEST(LogEvent, stringstream)
{
    Events::Log logev(LogGroup::LOGGER, LogCategory::DEBUG, "Debug message");
    std::stringstream chk;
    chk << logev;
    std::string expect("Logger DEBUG: Debug message");
    ASSERT_EQ(chk.str(), expect);
}


TEST(LogEvent, multiline_stream)
{
    std::string msg1 = "Log line 1\nLog line 2\nLog Line 3";
    Events::Log ev1(LogGroup::LOGGER, LogCategory::DEBUG, msg1);
    ev1.KeepNL();
    std::stringstream ev1_chk;
    // Tests Events::Log::operator<<()
    ev1_chk << ev1;

    // Check formatting with LogPrefix and no indenting of NL
    std::string msg1prfx = fmt::format("{}{}",
                                       LogPrefix(LogGroup::LOGGER, LogCategory::DEBUG),
                                       msg1);
    EXPECT_EQ(ev1_chk.str(), msg1prfx) << "operator<<() (.KeepNL())";


    Events::Log ev2(LogGroup::LOGGER, LogCategory::DEBUG, msg1);
    std::stringstream ev2_chk;
    // Tests Events::Log::operator<<(), without .KeepNL()
    ev2_chk << ev2;

    // Check formatting with LogPrefix and no indenting of NL
    std::string msg2prfx = fmt::format("{}Log line 1Log line 2Log Line 3",
                                       LogPrefix(LogGroup::LOGGER, LogCategory::DEBUG));
    EXPECT_EQ(ev2_chk.str(), msg2prfx) << "operator<<() (no KeepNL())";
}


TEST(LogEvent, multiline_str)
{
    // Tests for Events::Log::str()
    // Check formatting without LogPrefix and no indenting of NL
    std::string msg1 = "Log line 1\nLog line 2\nLog Line 3";
    Events::Log ev1(LogGroup::LOGGER, LogCategory::DEBUG, msg1);
    ev1.KeepNL();
    EXPECT_EQ(ev1.str(0, false), msg1) << "ev1.str(0, false)";

    // Check formatting with LogPrefix and no indenting of NL
    std::string msg1prfx = fmt::format("{}{}",
                                       LogPrefix(LogGroup::LOGGER, LogCategory::DEBUG),
                                       msg1);
    // Events::Log::str() without arguments does not indent multiple lines
    EXPECT_EQ(ev1.str(), msg1prfx) << "ev1.str() [A]";

    // Events::Log::str(indent, prefix_flag) - 5 space indent and no prefix
    std::string msg1ind5 = "Log line 1\n     Log line 2\n     Log Line 3";
    EXPECT_EQ(ev1.str(5, false), msg1ind5) << "ev1.str(5, false)";

    // Check formatting filtering out newlines
    // Same as above, but the resulting output is a single line
    std::string msg_nonl = fmt::format("{}Log line 1Log line 2Log Line 3",
                                       LogPrefix(LogGroup::LOGGER, LogCategory::DEBUG));
    Events::Log ev_nonl(LogGroup::LOGGER, LogCategory::DEBUG, msg1);
    EXPECT_EQ(ev_nonl.str(), msg_nonl) << "ev_nonl.str()";
}

TEST(LogEvent, multiline_operator_stdstring)
{
    std::string msg1 = "Log line 1\nLog line 2\nLog Line 3";
    Events::Log ev1(LogGroup::LOGGER, LogCategory::DEBUG, msg1);
    ev1.KeepNL();
    // Tests std::string Events::Log::operator()
    std::string ev1_chk = ev1;

    // Check formatting with LogPrefix - operator() indents with 4 spaces
    std::string msg1prfx = fmt::format("{}Log line 1\n    Log line 2\n    Log Line 3",
                                       LogPrefix(LogGroup::LOGGER, LogCategory::DEBUG));
    EXPECT_EQ(ev1_chk, msg1prfx) << "std::string operator() (.KeepNL())";


    Events::Log ev2(LogGroup::LOGGER, LogCategory::DEBUG, msg1);
    // Tests std::string Events::Log::operator(), without .KeepNL()
    std::string ev2_chk = ev2;

    // Check formatting with LogPrefix and no indenting of NL
    std::string msg2prfx = fmt::format("{}Log line 1Log line 2Log Line 3",
                                       LogPrefix(LogGroup::LOGGER, LogCategory::DEBUG));
    EXPECT_EQ(ev2_chk, msg2prfx) << "std::string operator() (no KeepNL())";
}

TEST(LogEvent, stringstream_grp_ctg_limits)
{
    for (uint8_t g = 0; g <= LogGroupCount; g++)
    {
        for (uint8_t c = 0; c < 10; c++)
        {
            Events::Log ev((LogGroup)g, (LogCategory)c, "some message string");
            std::stringstream msg;
            msg << ev;

            // Construct expected string manually
            std::stringstream chk;
            if (LogGroup::UNDEFINED != (LogGroup)g)
            {
                chk << (g < LogGroupCount ? LogGroup_str[g] : "[group:10]");
            }
            if (LogCategory::UNDEFINED != (LogCategory)c)
            {
                if (LogGroup::UNDEFINED != (LogGroup)g)
                {
                    chk << " ";
                }
                chk << (c < 9 ? LogCategory_str[c] : "[category:9]");
            }
            if (LogGroup::UNDEFINED != (LogGroup)g
                || LogCategory::UNDEFINED != (LogCategory)c)
            {
                chk << ": ";
            }
            chk << "some message string";
            EXPECT_EQ(ev.str(), chk.str());
        }
    }
}


TEST(LogEvent, message_filter)
{
    // Checks that messages containing characters < 0x20 are filtered out
    std::string below_0x20{"This is a \1 test \x20with \b various \x0A\n\rcontrol "
                           "\x19\x0E\x1b[31m characters"};
    std::string expected_below_0x20_wo_nl{"This is a  test  with  various control [31m characters"};
    Events::Log ev_b0x20(LogGroup::LOGGER, LogCategory::DEBUG, below_0x20);
    EXPECT_EQ(ev_b0x20.str(0, false), expected_below_0x20_wo_nl);

    std::string expected_below_0x20_with_nl{"This is a  test  with  various \n\ncontrol [31m characters"};
    auto ev_b0x20_w_nl = Events::Log(LogGroup::LOGGER, LogCategory::DEBUG, below_0x20).KeepNL();
    EXPECT_EQ(ev_b0x20_w_nl.str(0, false), expected_below_0x20_with_nl);

    std::string with_nl("This is line 1\nThis is line 2\nThis is line 3\n\n\n");
    // Trailing \n is always removed
    std::string expected_with_nl{"This is line 1\nThis is line 2\nThis is line 3"};
    auto ev_with_nl = Events::Log(LogGroup::LOGGER, LogCategory::DEBUG, with_nl).KeepNL();
    EXPECT_EQ(ev_with_nl.str(0, false), expected_with_nl);

    std::string expected_wo_nl{"This is line 1This is line 2This is line 3"};
    auto ev_wo_nl = Events::Log(LogGroup::LOGGER, LogCategory::DEBUG, with_nl);
    EXPECT_EQ(ev_wo_nl.str(0, false), expected_wo_nl);
}

} // namespace unittest
