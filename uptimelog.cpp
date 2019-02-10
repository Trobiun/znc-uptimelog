#include <znc/main.h>
#include <znc/Modules.h>
#include <znc/IRCNetwork.h>
#include <znc/User.h>
#include <znc/Chan.h>
#include <znc/Server.h>
#include <ctime>
#include <math.h>
#include <curl/curl.h>
#include "lib/json.hpp"

#define CLIENT_ID "kqsi0i4m9qfkmtvnj8lcom05to7hhm"

namespace {
    const char* s_FormatPath = "%Y-%m-%dT%H-%M-%S.log";
    const char* s_FormatTime = "[%H:%M:%S]";
}

class CLogger {
protected:

    CUser* m_pUser;
    CFile m_logFile;
    time_t m_loadTime;

    
    static size_t writeCallBack(void *contents, size_t size, size_t nmemb, void *userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }
    
    time_t GetStartLiveTime(const CString sChannel) {
        CURL *curl;
        CURLcode res;
        struct curl_slist *list = NULL;
        std::string readBuffer;

        curl = curl_easy_init();
        if (curl && !sChannel.empty()) {
            CString url = "https://api.twitch.tv/helix/streams?user_login=" + sChannel;
            curl_easy_setopt(curl, CURLOPT_URL, url.data());
            list = curl_slist_append(list, "Client-ID: " CLIENT_ID);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallBack);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
//                PutModule("curl_easy_perform() failed " + CString(curl_easy_strerror(res)));
            }
            long http_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            curl_easy_cleanup(curl);
            curl_slist_free_all(list);
//            PutModule(CString(http_code));
//            PutModule(readBuffer);
            json::JSON root = json::JSON::Load(readBuffer);
            CString started_at = root.at("data")[0].at("started_at").ToString();
            struct tm tm;
            time_t time_load;
            strptime(started_at.data(), "%FT%TZ", &tm);
//            PutModule("started_at : " + started_at);
            time_load = mktime(&tm);
//            PutModule("lol :" + CString(time_load));
            time_t now = time(nullptr);
            return time_load;
//            double diff = difftime(now, time_load);
//            PutModule("time_1 : " + CUtils::FormatTime(time_1,"%FT%TZ",GetUser()->GetTimezone()));
        }
        return -1;
    }
    
    
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

    CLogger(const CString& saveDir, const CString& sLogPath, const CString& sWindow, CUser* user, bool bAuto) {
        if (bAuto) {
            m_loadTime = GetStartLiveTime(sWindow);
        }
        else {
            m_loadTime = time(nullptr);
        }
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
    bool m_bAuto;

    void CreateLogger(const CString& sWindow) {
        try {
            CLogger* addLogger = new CLogger(GetSavePath(), m_sLogPath, sWindow, GetUser(), m_bAuto);
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
        if (!m_bActivated) {
            m_bActivated = true;
            PutModule("Logging started.");
        }
    }
    
    void StartLogAuto(const CString& sCommand) {
        if (!m_bActivated) {
            m_bActivated = true;
//            CString sChannel = sCommand.Token(1);
//            GetStartLiveTime(sChannel);
        }
    }

    void StopLog(const CString& sCommand) {
        if (m_bActivated) {
            m_bActivated = false;
            ClearLoggers();
            PutModule("Logging stopped.");
        }
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

//    virtual EModRet OnJoining(CChan& Channel) override {
//        m_bActivated = true;
//        CreateLogger(Channel.GetName());
//        return CONTINUE;
//    }

    MODCONSTRUCTOR(CUptimeLogMod) {
        AddHelpCommand();
        AddCommand("Start", static_cast<CModCommand::ModCmdFunc> (&CUptimeLogMod::StartLog),
                "", "Start log.");
        AddCommand("Auto", static_cast<CModCommand::ModCmdFunc> (&CUptimeLogMod::StartLogAuto),
                "", "Start log automatically for twitch.");
        AddCommand("Stop", static_cast<CModCommand::ModCmdFunc> (&CUptimeLogMod::StopLog),
                "", "Stop log.");
    }

    virtual bool OnLoad(const CString& sArgs, CString& sMessage) override {
        m_sLogPath = GetSavePath() + "/$WINDOW/" + s_FormatPath;
        return true;
    }
    
//    static size_t writeCallBack(void *contents, size_t size, size_t nmemb, void *userp) {
//        ((std::string*)userp)->append((char*)contents, size * nmemb);
//        return size * nmemb;
//    }

    virtual ~CUptimeLogMod() {
        StopLog("stop");
    }

};

NETWORKMODULEDEFS(CUptimeLogMod, "Module to log conversations with uptime of module");