// SPDX-License-Identifier: GPL-2.0-or-later
#include <powermeter/Provider.h>
#include <MqttSettings.h>

namespace PowerMeters {

bool Provider::isDataValid() const
{
    return getLastUpdate() > 0 && ((millis() - getLastUpdate()) < (30 * 1000));
}

float Provider::getPowerTotal() const
{
    auto oPowerTotal = _dataCurrent.get<DataPointLabel::PowerTotal>();

    if (oPowerTotal) {
        return *oPowerTotal;
    }

    auto powerTotal = 0.0;

#define ADD(l) \
    { \
        auto oDataPoint = _dataCurrent.get<DataPointLabel::l>(); \
        if (oDataPoint) { \
            powerTotal += *oDataPoint; \
        } \
    }

    ADD(PowerL1);
    ADD(PowerL2);
    ADD(PowerL3);
#undef ADD

    return powerTotal;
}

void Provider::mqttLoop() const
{
    if (!MqttSettings.getConnected()) { return; }

    if (!isDataValid()) { return; }

    auto constexpr halfOfAllMillis = std::numeric_limits<uint32_t>::max() / 2;
    if ((getLastUpdate() - _lastMqttPublish) > halfOfAllMillis) { return; }

    // based on getPowerTotal() as we can not be sure that the PowerTotal value is set for all providers
    MqttSettings.publish("powermeter/powerTotal", String(getPowerTotal()));

#define PUB(l, t) \
    { \
        auto oDataPoint = _dataCurrent.get<DataPointLabel::l>(); \
        if (oDataPoint) { \
            MqttSettings.publish("powermeter/" t, String(*oDataPoint)); \
        } \
    }

    PUB(PowerL1, "power1");
    PUB(PowerL2, "power2");
    PUB(PowerL3, "power3");
    PUB(VoltageL1, "voltage1");
    PUB(VoltageL2, "voltage2");
    PUB(VoltageL3, "voltage3");
    PUB(CurrentL1, "current1");
    PUB(CurrentL2, "current2");
    PUB(CurrentL3, "current3");
    PUB(Import, "import");
    PUB(Export, "export");
#undef PUB

    _lastMqttPublish = millis();
}

} // namespace PowerMeters
