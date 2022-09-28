#ifndef __Net_h__
#define __Net_h__

#include "Header.h"

#include <set>

namespace XEngine {
    class Tcper;

    class Net : public iNet {
    public:
        virtual ~Net() {}

        static iNet* GetInstance();

        // ͨ�� iEngineComponent �̳�
        virtual bool Initialize(Api::iEngine* const engine) override;
        virtual bool Launch(Api::iEngine* const engine) override;
        virtual void Release(Api::iEngine* const engine) override;
        virtual void EarlyUpdate(Api::iEngine* const engine) override;
        virtual void Update(Api::iEngine* const engine) override;
        virtual void LaterUpdate(Api::iEngine* const engine) override;

        // ͨ�� iNet �̳�
        virtual bool LaunchTcpSession(Api::iTcpSession* session, const char* host, const int port, int max_ss, int max_rs) override;
        virtual bool LaunchTcpServer(Api::iTcpServer* server, const char* ip, const int port, int max_ss, int max_rs) override;

    public:
        inline void TcpNeedSend(Tcper* pipe) {
            _tcp_need_send.insert(pipe);
        }

        inline void TcpNoNeedSend(Tcper* pipe) {
            _tcp_need_send.erase(pipe);
        }

    private:
        Net() {}

        std::set<Tcper*> _tcp_need_send;
    };
}

#endif //__Net_h__
