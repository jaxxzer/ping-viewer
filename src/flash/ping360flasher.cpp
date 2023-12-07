#include "ping360flasher.h"
#include "logger.h"

PING_LOGGING_CATEGORY(PING360FLASH, "ping360.flash")

Ping360Flasher::Ping360Flasher(QObject* parent)
    : Flasher(parent, {115200})
{
    connect(&_worker, &Ping360FlashWorker::flashProgressChanged, this,
        [this](float flashProgressPct) { emit flashProgress(flashProgressPct); });
    connect(&_worker, &Ping360FlashWorker::stateChanged, this,
        [this](Flasher::States newState) { setState(newState); });
    connect(&_worker, &Ping360FlashWorker::messageChanged, this,
        [this](QString message) { 
            setMessage(message); 
        qCCritical(PING360FLASH) << message; });
}

void Ping360Flasher::flash()
{
    _worker.setFirmwarePath(_firmwareFilePath);
    _worker.setLink(_link);
    _worker.setVerify(_verify);
    _worker.start();
}
