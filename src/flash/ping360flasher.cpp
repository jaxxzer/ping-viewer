#include "ping360flasher.h"
#include "logger.h"

PING_LOGGING_CATEGORY(PING360FLASH, "ping360.flash")

Ping360Flasher::Ping360Flasher()
    : Flasher()
{
    connect(&_worker, &Ping360FlashWorker::flashProgressChanged, this,
        [this](float flashProgressPct) { emit flashProgress(flashProgressPct); });
    connect(&_worker, &Ping360FlashWorker::stateChanged, this,
        [this](Flasher::States newState) { qCCritical(PING360FLASH) << newState; emit stateChanged(newState);
        qCCritical(PING360FLASH) << newState; });
}

void Ping360Flasher::flash()
{
    _worker.setFirmwarePath(_firmwareFilePath);
    _worker.setLink(_link);
    _worker.setVerify(_verify);
    _worker.start();
}
