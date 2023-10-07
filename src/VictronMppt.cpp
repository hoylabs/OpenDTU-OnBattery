// SPDX-License-Identifier: GPL-2.0-or-later
#include "VictronMppt.h"
#include "Configuration.h"
#include "PinMapping.h"
#include "MessageOutput.h"

VictronMpptClass VictronMppt;

void VictronMpptClass::init()
{
    std::lock_guard<std::mutex> lock(_mutex);

    _controllers.clear();

    CONFIG_T& config = Configuration.get();
    if (!config.Vedirect_Enabled) { return; }

    const PinMapping_t& pin = PinMapping.get();
    int8_t rx = pin.victron_rx;
    int8_t tx = pin.victron_tx;

    MessageOutput.printf("[VictronMppt] rx = %d, tx = %d\r\n", rx, tx);

    if (rx < 0) {
        MessageOutput.println(F("[VictronMppt] invalid pin config"));
        return;
    }

    auto upController = std::make_unique<VeDirectMpptController>();
    upController->init(rx, tx, &MessageOutput, config.Vedirect_VerboseLogging);
    _controllers.push_back(std::move(upController));
}

void VictronMpptClass::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);

    for (auto const& upController : _controllers) {
        upController->loop();
    }
}

bool VictronMpptClass::isDataValid() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    for (auto const& upController : _controllers) {
        if (!upController->isDataValid()) { return false; }
    }

    return !_controllers.empty();
}

uint32_t VictronMpptClass::getLastUpdate() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_controllers.empty()) { return 0; }

    auto iter = _controllers.cbegin();
    uint32_t lastUpdate = (*iter)->getLastUpdate();
    ++iter;

    while (iter != _controllers.end()) {
        // TODO(schlimmchen): this breaks when millis() wraps around. this
        // function would be much simpler if it returned data age instead
        // (return max of all millis() - getLastUpdate()).
        lastUpdate = std::min(lastUpdate, (*iter)->getLastUpdate());
        ++iter;
    }

    return lastUpdate;
}

VeDirectMpptController::veMpptStruct const& VictronMpptClass::getData(size_t idx) const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_controllers.empty() || idx >= _controllers.size()) {
        MessageOutput.printf("ERROR: MPPT controller index %d is out of bounds (%d controllers)\r\n",
                idx, _controllers.size());
        static VeDirectMpptController::veMpptStruct dummy;
        return dummy;
    }

    // TODO(schlimmchen): this returns a reference to a struct that is part
    // of a class instance, whose lifetime is managed by a unique_ptr and may
    // disappear. the VeDirectMpptController should manage the struct in a
    // shared_ptr and only allow accessing it via a shared_ptr copy, so we can
    // return a shared_ptr to that structure here.
    return _controllers[idx]->veFrame;
}

String VictronMpptClass::getPidAsString(size_t idx) const
{
    return VeDirectMpptController::getPidAsString(getData(idx).PID);
}

String VictronMpptClass::getCsAsString(size_t idx) const
{
    return VeDirectMpptController::getCsAsString(getData(idx).CS);
}

String VictronMpptClass::getErrAsString(size_t idx) const
{
    return VeDirectMpptController::getErrAsString(getData(idx).ERR);
}

String VictronMpptClass::getOrAsString(size_t idx) const
{
    return VeDirectMpptController::getOrAsString(getData(idx).OR);
}

String VictronMpptClass::getMpptAsString(size_t idx) const
{
    return VeDirectMpptController::getMpptAsString(getData(idx).MPPT);
}
