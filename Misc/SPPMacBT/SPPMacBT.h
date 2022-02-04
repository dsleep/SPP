#include "SPPCore.h"
#include <functional>
#include <map>

namespace SPP
{
    uint32_t GetWinRTBTWVersion();

    enum class EBTEState
    {
        Connected,
        Disconnected
    };

    struct IBTEWatcher
    {
        virtual void IncomingData(uint8_t*, size_t) = 0;
        virtual void StateChange(EBTEState) = 0;
    };

    class BTEWatcher
    {
    private:
        struct PlatImpl;
        std::unique_ptr<PlatImpl> _impl;

    public:
        BTEWatcher();
        ~BTEWatcher();
        void WatchForData(const std::string& DeviceID, const std::map< std::string, IBTEWatcher* >& CharacterFunMap);
        void WriteData(const std::string& DeviceID, const std::string& WriteID, const void* buf, uint16_t BufferSize);
        void Update();
        void Stop();
    };
}
