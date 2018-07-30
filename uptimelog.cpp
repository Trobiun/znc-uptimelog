#include <znc/main.h>
#include <znc/Modules.h>
#include <znc/IRCNetwork.h>
#include <znc/User.h>
#include <znc/Chan.h>
#include <znc/Server.h>
#include <ctime>
#include <math.h>


namespace {
    const char* s_FormatPath = "%Y-%m-%dT%H-%M-%S.log";
    const char* s_FormatTime = "[%H:%M:%S]";
}

class CLogger {
protected:

    CUser* m_pUser;
    CFile m_logFile;
    time_t m_loadTime;

public:

    void PutLog(const CString& sLine) {
        time_t curtime = std::time(nullptr);
        time_t diff = std::difftime(curtime, m_loadTime);
        struct tm* timeinfo = gmtime(&diff);
        char buffer[sizeof ("[00:00:00]")];
        strftime(buffer, sizeof ("[00:00:00]"), s_FormatTime, timeinfo);
        CString sDiff = CString(buffer);
        CString userTime = CUtils::FormatTime(curtime, s_FormatTime, m_pUser->GetTimezone());
        m_logFile.Write(sDiff + userTime + " " + sLine + "\n");
    }

    CLogger(const CString& saveDir, const CString& sLogPath, const CString& sWindow, CUser* user) {
        m_loadTime = time(nullptr);
        m_pUser = user;
        CString sPath = CUtils::FormatTime(m_loadTime, sLogPath, m_pUser->GetTimezone());
        sPath.Replace("$WINDOW", CString(sWindow.Replace_n("/", "-").Replace_n("\\", "-")).AsLower());

        sPath = CDir::CheckPathPrefix(saveDir, sPath);
        if (sPath.empty()) {
            throw std::ios_base::failure("Invalid log path [" + saveDir + sLogPath + "].");
        }
        m_logFile = CFile(sPath);
        CString sLogDir = m_logFile.GetDir();
        struct stat modDirInfo;
        CFile::GetInfo(saveDir, modDirInfo);
        if (!CFile::Exists(sLogDir)) {
            CDir::MakeDir(sLogDir, modDirInfo.st_mode);
        }
        if (!m_logFile.Open(O_WRONLY | O_APPEND | O_CREAT)) {
            throw std::ios_base::failure("Could not open log file [" + sPath + "]: " + strerror(errno));
        }
    }

    ~CLogger() {
        m_logFile.Close();
    }

};

class CUptimeLogMod : public CModule {
protected:
    std::map<CString, CLogger*> m_loggers;
    CString m_sLogPath;
    bool m_bActivated;

    void CreateLogger(const CString& sWindow) {
        try {
            CLogger* addLogger = new CLogger(GetSavePath(), m_sLogPath, sWindow, GetUser());
            m_loggers.insert(std::make_pair(sWindow, addLogger));
        }
        catch (std::ios_base::failure failure) {
            PutModule(failure.what());
        }
    }

    CLogger* GetLogger(const CString& sWindow) {
        try {
            CLogger* pLogger = m_loggers.at(sWindow);
            return pLogger;
        }
        catch (std::out_of_range oor) {
            CreateLogger(sWindow);
            try {
                CLogger* pLogger = m_loggers.at(sWindow);
                return pLogger;
            }
            catch (std::out_of_range oor2) {
                PutModule("Could not create logger.");
            }
        }
        return nullptr;
    }

    void ClearLoggers() {
        std::map<CString, CLogger*>::iterator it;
        for (it = m_loggers.begin(); it != m_loggers.end(); it++) {
            delete it->second;
        }
        m_loggers.clear();
    }

    void StartLog(const CString& sCommand) {
        m_bActivated = true;
        PutModule("Starting log.");

    }

    void StopLog(const CString& sCommand) {
        m_bActivated = false;
        ClearLoggers();
        PutModule("Stopping log.");
    }

public:

    void PutLog(const CString& sLine, const CString sWindow = "Status") {
        if (!m_bActivated) {
            return;
        }
        CLogger* pLogger = GetLogger(sWindow);
        if (nullptr == pLogger) {
            PutModule("Can't get logger.");
        }
        else {
            pLogger->PutLog(sLine);
        }
    }

    void PutLog(const CString& sLine, const CChan& Channel) {
        PutLog(sLine, Channel.GetName());
    }

    void PutLog(const CString& sLine, const CNick& Nick) {
        PutLog(sLine, Nick.GetNick());
    }

    EModRet OnBroadcast(CString& sMessage) override {
        PutLog("Broadcast : " + sMessage);
        return CONTINUE;
    }

    EModRet OnUserNotice(CString& sTarget, CString& sMessage) override {
        CIRCNetwork* pNetwork = GetNetwork();
        if (pNetwork) {
            PutLog("-" + pNetwork->GetCurNick() + "- " + sMessage, sTarget);
        }
        return CONTINUE;
    }

    EModRet OnPrivNotice(CNick& Nick, CString& sMessage) override {
        PutLog("-" + Nick.GetNick() + "- " + sMessage, Nick);
        return CONTINUE;
    }

    EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage) override {
        PutLog("-" + Nick.GetNick() + "- " + sMessage, Channel);
        return CONTINUE;
    }

    EModRet OnUserMsg(CString& sTarget, CString& sMessage) override {
        CIRCNetwork* pNetwork = GetNetwork();
        if (pNetwork) {
            PutLog("<" + pNetwork->GetCurNick() + "> " + sMessage, sTarget);
        }
        return CONTINUE;
    }

    EModRet OnPrivMsg(CNick& Nick, CString& sMessage) override {
        PutLog("<" + Nick.GetNick() + "> " + sMessage, Nick);
        return CONTINUE;
    }

    EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) override {
        PutLog("<" + Nick.GetNick() + "> " + sMessage, Channel);
        return CONTINUE;
    }

    virtual EModRet OnJoining(CChan& Channel) override {
        m_bActivated = true;
        CreateLogger(Channel.GetName());
        return CONTINUE;
    }

    MODCONSTRUCTOR(CUptimeLogMod) {
        AddHelpCommand();
        AddCommand("Start", static_cast<CModCommand::ModCmdFunc> (&CUptimeLogMod::StartLog),
                "", "Start log.");
        AddCommand("Stop", static_cast<CModCommand::ModCmdFunc> (&CUptimeLogMod::StopLog),
                "", "Stop log.");
    }

    virtual bool OnLoad(const CString& sArgs, CString& sMessage) override {
        m_sLogPath = GetSavePath() + "/$WINDOW/" + s_FormatPath;
        return true;
    }

    virtual ~CUptimeLogMod() {
        StopLog("stop");
    }

};

NETWORKMODULEDEFS(CUptimeLogMod, "Module to log conversations with uptime of module");