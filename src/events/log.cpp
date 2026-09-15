//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   events/log.cpp
 *
 * @brief  [enter description of what this file contains]
 */

#include <algorithm>
#include <iomanip>
#include <ranges>
#include <string>
#include <string_view>
#include <gio/gio.h>
#include <gdbuspp/glib2/utils.hpp>
#include <gdbuspp/signals/group.hpp>

#include "common/string-utils.hpp"
#include "log.hpp"


namespace Events {

DBus::Signals::SignalArgList Log::SignalDeclaration(bool with_session_token) noexcept
{
    if (with_session_token)
    {
        return {{"group", glib2::DataType::DBus<LogGroup>()},
                {"level", glib2::DataType::DBus<LogCategory>()},
                {"session_token", glib2::DataType::DBus<std::string>()},
                {"message", glib2::DataType::DBus<std::string>()}};
    }
    else
    {
        return {{"group", glib2::DataType::DBus<LogGroup>()},
                {"level", glib2::DataType::DBus<LogCategory>()},
                {"message", glib2::DataType::DBus<std::string>()}};
    }
}


Log::Log()
{
    reset();
}


Log::Log(LogGroup grp,
         LogCategory ctg,
         const std::string &msg)
    : group(grp), category(ctg),
      message_(msg)
{
    format = Format::NORMAL;
}


Log::Log(LogGroup grp,
         LogCategory ctg,
         const std::string &session_token,
         const std::string &msg)
    : group(grp), category(ctg),
      session_token(session_token),
      message_(msg)
{
    format = Format::SESSION_TOKEN;
}


Log::Log(LogGroup grp,
         LogCategory ctg,
         const char *session_token,
         const char *msg)
    : group(grp), category(ctg),
      session_token(session_token),
      message_(msg)
{
    format = Format::SESSION_TOKEN;
}


Log::Log(const Log &logev, const std::string &session_token)
    : group(logev.group), category(logev.category),
      session_token(session_token),
      message_(logev.message_)
{
    format = Format::SESSION_TOKEN;
    keep_nl_ = logev.keep_nl_;
    indent_ = logev.indent_;
}


void Log::RemoveToken()
{
    format = Format::NORMAL;
    session_token.clear();
}


Log &Log::KeepNL() noexcept
{
    keep_nl_ = true;
    return *this;
}


std::string Log::GetMessage() const
{
    if (indent_ == 0)
    {
        return filter_ctrl_chars(message_, !keep_nl_);
    }

    std::string raw_message = filter_ctrl_chars(message_, !keep_nl_);
    auto lines = raw_message
                 | std::views::split('\n')
                 | std::views::transform([](auto &&r)
                                         {
                                             return std::string_view(r.begin(), r.end());
                                         });

    std::string ret;
    bool first = true;
    for (std::string_view line : lines)
    {
        if (!first)
        {
            ret += '\n';
            ret.append(indent_, ' ');
        }
        first = false;
        ret += line;
    }
    return ret;
}


void Log::SetDBusSender(DBus::Signals::Target::Ptr sndr)
{
    if (!sender)
    {
        sender = sndr;
    }
}


Log &Log::AddLogTag(LogTag::Ptr tag) noexcept
{
    logtag = tag;
    return *this;
}


LogTag::Ptr Log::GetLogTag() const noexcept
{
    return logtag;
}


GVariant *Log::GetGVariantTuple() const
{

    bool session_token_type = Format::SESSION_TOKEN == format
                              || (Format::AUTO == format && !session_token.empty());

    GVariantBuilder *bld = glib2::Builder::Create(session_token_type ? "(uuss)" : "(uus)");

    glib2::Builder::Add(bld, group);
    glib2::Builder::Add(bld, category);
    if (session_token_type)
    {
        glib2::Builder::Add(bld, session_token);
    }
    glib2::Builder::Add(bld, GetMessage());

    return glib2::Builder::Finish(bld);
}


GVariant *Log::GetGVariantDict() const
{
    GVariantDict *dict = glib2::Dict::Create();

    glib2::Dict::Add(dict, "log_group", glib2::Value::Create(group));
    glib2::Dict::Add(dict, "log_category", glib2::Value::Create(category));
    if (!session_token.empty() || Format::SESSION_TOKEN == format)
    {
        glib2::Dict::Add(dict, "log_session_token", glib2::Value::Create(session_token));
    }
    glib2::Dict::Add(dict, "log_message", glib2::Value::Create(GetMessage()));

    return glib2::Dict::Finish(dict);
}


std::string Log::GetLogGroupStr() const
{
    if ((uint8_t)group >= LogGroupCount)
    {
        return std::string("[group:"
                           + std::to_string(static_cast<uint8_t>(group))
                           + "]");
    }
    return LogGroup_str[(uint8_t)group];
}


std::string Log::GetLogCategoryStr() const
{
    if ((uint8_t)category > 8)
    {
        return std::string("[category:" + std::to_string((uint8_t)category) + "]");
    }
    return LogCategory_str[(uint8_t)category];
}


void Log::reset()
{
    group = LogGroup::UNDEFINED;
    category = LogCategory::UNDEFINED;
    session_token.clear();
    message_.clear();
    format = Format::AUTO;
    keep_nl_ = false;
}


bool Log::empty(bool only_message) const
{
    if (only_message)
    {
        return message_.empty();
    }

    return (LogGroup::UNDEFINED == group)
           && (LogCategory::UNDEFINED == category)
           && session_token.empty()
           && message_.empty();
}


Log &Log::SetIndent(uint8_t spaces)
{
    indent_ = spaces;
    return *this;
}


std::string Log::str(bool prefix) const
{
    return fmt::format("{}{}",
                       (prefix ? LogPrefix(group, category) : ""),
                       GetMessage());
}


bool Log::operator==(const Log &compare) const
{
    if (session_token.empty())
    {
        return ((compare.group == group)
                && (compare.category == category)
                && (0 == compare.message_.compare(message_)));
    }
    else
    {
        return ((compare.group == group)
                && (compare.category == category)
                && (0 == compare.session_token.compare(session_token))
                && (0 == compare.message_.compare(message_)));
    }
}


bool Log::operator!=(const Log &compare) const
{
    return !(this->operator==(compare));
}


Events::Log::operator std::string() const
{
    return str(4);
}


namespace {
/**
 *  Parses group and category strings to the appropriate LogGroup
 *  and LogGroup enum values.  This method sets the group and category members
 *  directly and does not return any values.  String values not found will
 *  result in the UNDEFINED value being set.
 *
 * @param grp_s std::string containing the human readable LogGroup string
 * @param ctg_s std::string containing the human readable LogCategory string
 */
static std::pair<LogGroup, LogCategory> parse_group_category(const std::string &grp_s, const std::string &ctg_s)
{
    auto grp_item = std::find(LogGroup_str.begin(), LogGroup_str.end(), grp_s);
    LogGroup group = (grp_item != LogGroup_str.end()
                          ? static_cast<LogGroup>(std::distance(LogGroup_str.begin(), grp_item))
                          : LogGroup::UNDEFINED);
    auto catg_item = std::find(LogCategory_str.begin(), LogCategory_str.end(), ctg_s);
    LogCategory category = (catg_item != LogCategory_str.end()
                                ? static_cast<LogCategory>(std::distance(LogCategory_str.begin(), catg_item))
                                : LogCategory::UNDEFINED);
    return std::make_pair(group, category);
}

/**
 *  Parses a GVariant object containing a Log signal.  The input
 *  GVariant needs to be of 'a{sv}' which is a named dictionary.  It
 *  must contain the following key values to be valid:
 *
 *     - (u) log_group          Translated into LogGroup
 *     - (u) log_category       Translated into LogCategory
 *     - (s) log_session_token  An optional session token string
 *     - (s) log_message        A string with the log message
 *
 * @param logevent  Pointer to the GVariant object containig the
 *                  log event
 *
 * @return Events::Log object of the parsed GVariant object
 */
Log parse_dict(GVariant *logevent)
{
    auto group = glib2::Dict::Lookup<LogGroup>(logevent, "log_group");
    auto category = glib2::Dict::Lookup<LogCategory>(logevent, "log_category");
    auto message = glib2::Dict::Lookup<std::string>(logevent,
                                                    "log_message");
    try
    {
        auto session_token = glib2::Dict::Lookup<std::string>(logevent,
                                                              "log_session_token");
        return Log(group, category, session_token, message).KeepNL();
    }
    catch (const glib2::Utils::Exception &)
    {
        // This is fine; the log_session_token may not be available
        // and then we treat this event as a "normal" LogEvent without
        // the sessoin token value
    }
    return Log(group, category, message).KeepNL();
}


/**
 *  Parses a tuple oriented GVariant object matching the data type
 *  for a LogEvent object.  The data type must be (uus) if the
 *  GVariant object is does not carry a session token value; otherwise
 *  it must be (uuss) if it does.
 *
 * @param logevent            Pointer to the GVariant object containig the
 *                            log event
 * @param with_session_token  Boolean flag indicating if the logevent
 *                            GVariant object is expected to contain a
 *                            session token.
 *
 * @return Events::Log object of the parsed GVariant object
 */
Log parse_tuple(GVariant *logevent, bool with_session_token)
{
    if (!with_session_token)
    {
        glib2::Utils::checkParams(__func__, logevent, "(uus)", 3);
        auto group = glib2::Value::Extract<LogGroup>(logevent, 0);
        auto category = glib2::Value::Extract<LogCategory>(logevent, 1);
        auto message = glib2::Value::Extract<std::string>(logevent, 2);
        return Log(group, category, message).KeepNL();
    }
    else
    {
        glib2::Utils::checkParams(__func__, logevent, "(uuss)", 4);
        auto group = glib2::Value::Extract<LogGroup>(logevent, 0);
        auto category = glib2::Value::Extract<LogCategory>(logevent, 1);
        auto session_token = glib2::Value::Extract<std::string>(logevent, 2);
        auto message = glib2::Value::Extract<std::string>(logevent, 3);
        return Log(group, category, session_token, message).KeepNL();
    }
}



} // Anonymous namespace


Log ParseLog(const std::string &grp_s,
             const std::string &ctg_s,
             const std::string &sess_token,
             const std::string &msg)
{
    auto [grp, ctg] = parse_group_category(grp_s, ctg_s);
    return sess_token.empty()
               ? Log(grp, ctg, msg)
               : Log(grp, ctg, sess_token, msg);
}


Log ParseLog(const std::string &grp_s,
             const std::string &ctg_s,
             const std::string &msg)
{
    return ParseLog(grp_s, ctg_s, {}, msg);
}


Log ParseLog(GVariant *logev, DBus::Signals::Target::Ptr sndr)
{
    if (nullptr != logev)
    {
        std::string g_type = glib2::DataType::Extract(logev);
        Log event;
        if ("a{sv}" == g_type)
        {
            event = parse_dict(logev);
        }
        else if ("(uus)" == g_type)
        {
            event = parse_tuple(logev, false);
        }
        else if ("(uuss)" == g_type)
        {
            event = parse_tuple(logev, true);
        }
        else
        {
            throw LogException("LogEvent: Invalid LogEvent data type");
        }

        if (sndr)
        {
            event.SetDBusSender(sndr);
        }
        return event;
    }
    throw LogException("LogEvent: Invalid LogEvent GVariant data");
}


} // namespace Events
