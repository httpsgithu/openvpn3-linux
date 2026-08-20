//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//

#pragma once

#include <ctime>
#include <exception>
#include <fstream>
#include <memory>
#include <string>

#include <gdbuspp/connection.hpp>
#include <gdbuspp/signals/group.hpp>
#include <gdbuspp/signals/subscriptionmgr.hpp>

#include "events/status.hpp"
#include "logfilter.hpp"
#include "logwriter.hpp"


class LogSender : public DBus::Signals::Group,
                  public Log::EventFilter
{
  public:
    using Ptr = std::shared_ptr<LogSender>;

    LogSender(LogWriter *lgwr = nullptr);
    LogSender(DBus::Connection::Ptr dbuscon,
              const LogGroup lgroup,
              const std::string &objpath,
              const std::string &interf,
              const bool session_token = false,
              LogWriter *lgwr = nullptr);
    virtual ~LogSender() = default;

    const LogGroup GetLogGroup() const;

    /**
     *  Creates a new Events::Log object with the same LogGroup
     *  as this LogSender object is configured to use.
     *
     * @param catg   LogCategory to tag the log event with
     * @param msg    std::string with the log message
     *
     * @return Events::Log
     */
    Events::Log NewEvent(LogCategory catg, const std::string &msg);

    /**
     *  Log the Events::Log() object
     *
     * This will send the log event both to the D-Bus based log service
     * as well as logging via the configured LogWriter object.
     *
     * If the no_duplicates flag is set to true, if the log event is
     * identical to the previous log event, it will be skipped.
     *
     * @param logev
     * @param no_duplicates
     */
    virtual void Log(const Events::Log &logev, bool no_duplicates = false);


    // Convenience wrappers that construct Events::Log
    // and forward to LogSender::Log()
    virtual void Debug(const std::string &msg);
    virtual void Debug_wnl(const std::string &msg);
    virtual void LogVerb2(const std::string &msg);
    virtual void LogVerb1(const std::string &msg);
    virtual void LogInfo(const std::string &msg);
    virtual void LogWarn(const std::string &msg);
    virtual void LogError(const std::string &msg);
    virtual void LogCritical(const std::string &msg);
    virtual void LogFATAL(const std::string &msg);

    /**
     *  Retrieve the last Events::Log logged/sent
     *
     * @return Events::Log
     */
    Events::Log GetLastLogEvent() const;

    /**
     *  Retrieve all collected log events
     *
     *  When the LogSender object has been constructed without
     *  a D-Bus connection, this LogSender object will collect
     *  all the log events instead.  These events can be retrieved
     *  from this method.
     *
     *  This will only work if the LogSender::LogSender(LogWriter *)
     *  constructor has been used.   If not it will throw a
     *  std::runtime_error exception.
     *
     * @return std::vector<Events::Log>
     * @throws std::runtime_error
     */
    std::vector<Events::Log> GetLogBuffer();


    /**
     *  Retrieve a pointer to the configured LogWriter object
     *
     * @return LogWriter*
     */
    LogWriter *GetLogWriter();


  protected:
    LogWriter *logwr = nullptr;
    LogGroup log_group;


  private:
    bool dbus_enabled;
    Events::Log last_logevent;
    std::vector<Events::Log> log_buffer; //< Only used when dbus_enabled == false
};
