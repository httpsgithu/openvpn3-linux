//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   dbus-log.cpp
 *
 * @brief  Implementation of the OpenVPN 3 Linux D-Bus logging based interface
 */

#include <gdbuspp/signals/group.hpp>
#include <gdbuspp/signals/subscriptionmgr.hpp>
#include <gdbuspp/signals/target.hpp>

#include "dbus-log.hpp"


//
//  LogSender class implementation
//

LogSender::LogSender(LogWriter *lgwr)
    : DBus::Signals::Group(nullptr, "/", ""),
      Log::EventFilter(6),
      logwr(lgwr)
{
}


LogSender::LogSender(DBus::Connection::Ptr dbuscon,
                     const LogGroup lgroup,
                     const std::string &objpath,
                     const std::string &interf,
                     const bool session_token,
                     LogWriter *lgwr)
    : DBus::Signals::Group(dbuscon, objpath, interf),
      Log::EventFilter(3),
      logwr(lgwr),
      log_group(lgroup)
{
    RegisterSignal("Log",
                   Events::Log::SignalDeclaration(session_token));
}


const LogGroup LogSender::GetLogGroup() const
{
    return log_group;
}


Events::Log LogSender::NewEvent(LogCategory catg, const std::string &msg)
{
    return Events::Log(log_group, catg, msg);
}


void LogSender::Log(const Events::Log &logev, bool no_duplicates)
{
    // Don't log an empty messages or if log level filtering allows it
    // The filtering is done against the LogCategory of the message
    if (logev.empty() || !EventFilter::Allow(logev))
    {
        return;
    }

    // If this log event is the same as the previous one, skip it
    // if no duplicates has been requested
    if (no_duplicates)
    {
        if (!last_logevent.empty() && (logev == last_logevent))
        {
            // This contains the same log message as the previous one
            return;
        }
    }
    last_logevent = logev;

    if (logwr)
    {
        logwr->Write(logev);
    }

    SendGVariant("Log", logev.GetGVariantTuple());
}


void LogSender::Debug(const std::string &msg)
{
    Log(Events::Log(log_group, LogCategory::DEBUG, msg));
}

void LogSender::Debug_wnl(const std::string &msg)
{
    // Variant of Debug() (with newline) which will not filter out newline (\n)
    Log(Events::Log(log_group, LogCategory::DEBUG, msg, false));
}


void LogSender::LogVerb2(const std::string &msg)
{
    Log(Events::Log(log_group, LogCategory::VERB2, msg));
}


void LogSender::LogVerb1(const std::string &msg)
{
    Log(Events::Log(log_group, LogCategory::VERB1, msg));
}


void LogSender::LogInfo(const std::string &msg)
{
    Log(Events::Log(log_group, LogCategory::INFO, msg));
}


void LogSender::LogWarn(const std::string &msg)
{
    Log(Events::Log(log_group, LogCategory::WARN, msg));
}


void LogSender::LogError(const std::string &msg)
{
    Log(Events::Log(log_group, LogCategory::ERROR, msg));
}


void LogSender::LogCritical(const std::string &msg)
{
    // Critical log messages will always be sent
    Log(Events::Log(log_group, LogCategory::CRIT, msg));
}


void LogSender::LogFATAL(const std::string &msg)
{
    // Fatal log messages will always be sent
    Log(Events::Log(log_group, LogCategory::FATAL, msg));
    // FIXME: throw something here, to start shutdown procedures
}


Events::Log LogSender::GetLastLogEvent() const
{
    return last_logevent;
}


LogWriter *LogSender::GetLogWriter()
{
    return logwr;
}
