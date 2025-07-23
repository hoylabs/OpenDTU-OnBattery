// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023-2024 Thomas Basler and others
 */
#include "HMS_Abstract.h"
#include "Hoymiles.h"
#include "HoymilesRadio_CMT.h"
#include "commands/ChannelChangeCommand.h"

HMS_Abstract::HMS_Abstract(HoymilesRadio* radio, const uint64_t serial)
    : HM_Abstract(radio, serial)
{
}

bool HMS_Abstract::sendChangeChannelRequest()
{
    if (!(getEnableCommands() || getEnablePolling())) {
        return false;
    }

    auto cmdChannel = _radio->prepareCommand<ChannelChangeCommand>(this);
    cmdChannel->setCountryMode(Hoymiles.getRadioCmt()->getCountryMode());
    cmdChannel->setChannel(Hoymiles.getRadioCmt()->getChannelFromFrequency(Hoymiles.getRadioCmt()->getInverterTargetFrequency()));
    _radio->enqueCommand(cmdChannel);

    return true;
}

bool HMS_Abstract::supportsPowerDistributionLogic()
{
    // This feature was added in inverter firmware version 01.01.12 and
    // will limit the AC output instead of limiting the DC inputs.
    // Also supports firmware 2.0.4 (encoded as 20004U) and above
    return DevInfo()->getFwBuildVersion() >= 10112U;
}

bool HMS_Abstract::sendActivePowerControlRequest(float limit, const PowerLimitControlType type)
{
    // Use the base implementation to send the power control command
    bool success = HM_Abstract::sendActivePowerControlRequest(limit, type);
    
    if (success && DevInfo()->containsValidData() && DevInfo()->getFwBuildVersion() >= 20004U) {
        // HMS firmware 2.0.4+ may not update status automatically after power control commands.
        // Send a system config request to refresh the inverter status and get current limits.
        sendSystemConfigParaRequest();
    }
    
    return success;
}
