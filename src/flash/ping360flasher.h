#pragma once

#include "flasher.h"

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
     * @param parent
     */
    Ping360Flasher();

    /**
     * @brief Destroy the Ping360Flasher object
     *
     */
    ~Ping360Flasher() = default;

    /**
     * @brief Start the flash procedure
     *
     */
    void flash() override;

private:
    int _baudRate = 115200;
    const QList<int> _validBaudRates = {115200};
};
