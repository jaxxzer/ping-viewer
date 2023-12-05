#include "ping360flasher.h"
#include "logger.h"

PING_LOGGING_CATEGORY(PING360FLASH, "ping360.flash")

Ping360Flasher::Ping360Flasher()
    : Flasher()
{
    connect(&_worker, &Ping360FlashWorker::flashProgressChanged, this,
        [this](float flashProgressPct) { emit flashProgress(flashProgressPct); });
}

void Ping360Flasher::flash()
{
    _worker.setFirmwarePath(_firmwareFilePath);
    _worker.setLink(_link);
    _worker.setVerify(_verify);
    _worker.start();
}
