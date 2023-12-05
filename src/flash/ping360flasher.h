#pragma once

#include "flasher.h"
#include "ping360flashworker.h"

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(PING360FLASH)

/**
 * @brief Flasher implementation for Ping360 sensor device
 *
 */
class Ping360Flasher : public Flasher {
    Q_OBJECT
public:
    /**
     * @brief Construct a new Ping360Flasher object
     *
     */
    Ping360Flasher();

    /**
     * @brief Destroy the Ping360Flasher object
     *
     */
    ~Ping360Flasher() = default;

    void flash() override final;

private:
    Ping360FlashWorker _worker;
};
