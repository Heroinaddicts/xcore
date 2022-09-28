#include "Engine.h"
#include "MultiSys.h"
#include "Logic/Logic.h"
#include "SafeTime.h"
#include "SafeString.h"
#include "SafeSystem.h"
#include <map>

#ifdef WIN32
#include "Net/windows/Net.h"
#endif //WIN32


static std::map<std::string, std::string> static_parameter_map;

XEngine::iLogic* g_logic = nullptr;
XEngine::iNet* g_net = nullptr;

namespace XEngine {
    Engine* Engine::GetInstance() {
        static Engine engine;
        return &engine;
    }
    const char* Engine::GetLaunchParameter(const std::string& name) {
        auto itor = static_parameter_map.find(name);
        if (itor != static_parameter_map.end()) {
            return itor->second.c_str();
        }

        return nullptr;
    }

    Api::iNetApi* Engine::GetNetApi() {
        return g_net;
    }

    void Engine::LogAsync(const std::string& log) {
#ifdef _DEBUG
        printf("%s\n", log.c_str());
#endif //_DEBUG
    }

    void Engine::LogSync(const std::string& log) {
#ifdef _DEBUG
        printf("%s\n", log.c_str());
#endif //_DEBUG
    }

    void Engine::Shutdown() {

    }

    bool Engine::isShutdown() {
        return false;
    }
}

bool AnalysisLaunchParameters(const int argc, const char** args, const char** env) {
    for (int i = 1; i < argc; ++i) {
        if (strncmp(args[i], "--", 2) == 0) {
            const char* start = args[i] + 2;
            const char* equal = strstr(start, "=");
            if (equal != nullptr) {
                std::string name(start, equal);
                std::string val(equal + 1);
                static_parameter_map[name] = val;
            }
            else if (strlen(args[i]) > 2) {
                static_parameter_map[args[i] + 2] = "";
            }
        }
        else {
            return false;
        }
    }

    return true;
}

int main(int argc, const char** args, const char** env) {
    if (!AnalysisLaunchParameters(argc, args, env)) {
        XASSERT(false, "AnalysisLaunchParameters error");
        return 0;
    }


    XEngine::Engine* engine = XEngine::Engine::GetInstance();

    int fixedTimeStep = 33333;
    const char* fixedTimeStepStr = engine->GetLaunchParameter("fixedTimeStep");
    if (fixedTimeStepStr) {
        fixedTimeStep = SafeString::StringToFloat(fixedTimeStepStr) * 1000;
    }

    {
        g_logic = XEngine::Logic::GetInstance();
        g_net = XEngine::Net::GetInstance();
    }


    { // Initialize
        g_net->Initialize(engine);
        g_logic->Initialize(engine);
    }

    { // Launche
        g_net->Launch(engine);
        g_logic->Launch(engine);
    }

    unsigned_int64 tick = SafeTime::GetMicroSecond();
    while (!engine->isShutdown()) {
        g_net->EarlyUpdate(engine);
        g_logic->EarlyUpdate(engine);

        g_net->Update(engine);
        g_logic->Update(engine);

        g_net->LaterUpdate(engine);
        g_logic->LaterUpdate(engine);

        unsigned_int64 tick2 = SafeTime::GetMicroSecond();
        if (tick2 - tick >= fixedTimeStep) {
            tick += fixedTimeStep;
        }
    }

    { // Release
        g_logic->Release(engine);
        g_net->Release(engine);
    }

    return 0;
}
