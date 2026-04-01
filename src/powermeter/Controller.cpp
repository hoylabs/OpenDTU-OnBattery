// SPDX-License-Identifier: GPL-2.0-or-later
#include <powermeter/Controller.h>
#include <Configuration.h>
#include <powermeter/json/http/Provider.h>
#include <powermeter/json/mqtt/Provider.h>
#include <powermeter/modbus/udp/victron/Provider.h>
#include <powermeter/sdm/serial/Provider.h>
#include <powermeter/smahm/udp/Provider.h>
#include <powermeter/sml/http/Provider.h>
#include <powermeter/sml/serial/Provider.h>
#include <LogHelper.h>
#include <algorithm>

PowerMeters::Controller PowerMeter;

namespace PowerMeters {

#undef TAG
static const char* TAG = "powerMeter";
static const char* SUBTAG = "Controller";

static constexpr uint16_t kMaxSampleWindow = 200;
static constexpr uint16_t kMaxTimeWindowSeconds = 120;

void Controller::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&Controller::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.enable();

    updateSettings();
}

void Controller::sanitizeAveragingConfig(PowerMeterAveragingConfig& cfg) const
{
    if (cfg.WindowMode != PowerMeterAveragingConfig::Mode::Samples
            && cfg.WindowMode != PowerMeterAveragingConfig::Mode::Time) {
        cfg.WindowMode = PowerMeterAveragingConfig::Mode::Samples;
    }

    cfg.WindowSize = std::max<uint16_t>(1, cfg.WindowSize);

    auto maxWindow = (cfg.WindowMode == PowerMeterAveragingConfig::Mode::Samples)
        ? kMaxSampleWindow
        : kMaxTimeWindowSeconds;
    cfg.WindowSize = std::min<uint16_t>(cfg.WindowSize, maxWindow);
}

Controller::ChannelState& Controller::channelState(Channel channel)
{
    return _powerStats.at(static_cast<size_t>(channel));
}

Controller::ChannelState const& Controller::channelState(Channel channel) const
{
    return _powerStats.at(static_cast<size_t>(channel));
}

void Controller::resetPowerStats()
{
    _lastProviderUpdate = 0;
    for (auto& state : _powerStats) {
        state.Window.Samples.clear();
        state.Window.Sum = 0.0f;
        state.Stats = {};
    }
}

void Controller::pruneWindow(ChannelWindow& window, uint32_t now) const
{
    if (_averagingCfg.WindowMode == PowerMeterAveragingConfig::Mode::Samples) {
        while (window.Samples.size() > _averagingCfg.WindowSize) {
            window.Sum -= window.Samples.front().Value;
            window.Samples.pop_front();
        }
        return;
    }

    uint32_t maxAgeMs = static_cast<uint32_t>(_averagingCfg.WindowSize) * 1000U;
    while (!window.Samples.empty() && (now - window.Samples.front().Timestamp) > maxAgeMs) {
        window.Sum -= window.Samples.front().Value;
        window.Samples.pop_front();
    }
}

void Controller::updateStats(ChannelState& state)
{
    if (state.Window.Samples.empty()) {
        state.Stats.Average.reset();
        state.Stats.Minimum.reset();
        state.Stats.Maximum.reset();
        return;
    }

    state.Stats.Average = state.Window.Sum / state.Window.Samples.size();
    state.Stats.Last = state.Window.Samples.back().Value;

    auto minMax = std::minmax_element(
            state.Window.Samples.cbegin(),
            state.Window.Samples.cend(),
            [](Sample const& lhs, Sample const& rhs) {
                return lhs.Value < rhs.Value;
            });
    state.Stats.Minimum = minMax.first->Value;
    state.Stats.Maximum = minMax.second->Value;
}

void Controller::addSample(Channel channel, float value, uint32_t timestamp)
{
    auto& state = channelState(channel);
    state.Stats.Raw = value;
    state.Stats.Last = value;

    state.Window.Samples.push_back({ timestamp, value });
    state.Window.Sum += value;

    pruneWindow(state.Window, millis());
    updateStats(state);
}

void Controller::refreshAgingForTimeBasedWindows()
{
    if (_averagingCfg.WindowMode != PowerMeterAveragingConfig::Mode::Time) {
        return;
    }

    auto now = millis();
    for (auto& state : _powerStats) {
        pruneWindow(state.Window, now);
        updateStats(state);
    }
}

void Controller::updatePowerStatsFromProvider()
{
    if (!_upProvider) { return; }

    auto lastUpdate = _upProvider->getLastUpdate();
    if (lastUpdate == 0 || lastUpdate == _lastProviderUpdate) {
        refreshAgingForTimeBasedWindows();
        return;
    }
    _lastProviderUpdate = lastUpdate;

    auto channels = _upProvider->getPowerChannels();
    auto totalRaw = channels.Total;
    if (!totalRaw && (channels.L1 || channels.L2 || channels.L3)) {
        totalRaw = channels.L1.value_or(0.0f)
            + channels.L2.value_or(0.0f)
            + channels.L3.value_or(0.0f);
    }

    if (totalRaw) { addSample(Channel::Total, *totalRaw, lastUpdate); }
    if (channels.L1) { addSample(Channel::L1, *channels.L1, lastUpdate); }
    if (channels.L2) { addSample(Channel::L2, *channels.L2, lastUpdate); }
    if (channels.L3) { addSample(Channel::L3, *channels.L3, lastUpdate); }

    auto const& totalStats = channelState(Channel::Total).Stats;
    if (totalStats.Last && totalStats.Average && totalStats.Minimum && totalStats.Maximum) {
        DTU_LOGD("Total stats min=%.2f avg=%.2f max=%.2f last=%.2f",
                *totalStats.Minimum, *totalStats.Average, *totalStats.Maximum, *totalStats.Last);
    }
}

void Controller::updateSettings()
{
    std::lock_guard<std::mutex> l(_mutex);

    if (_upProvider) { _upProvider.reset(); }

    auto const& pmcfg = Configuration.get().PowerMeter;
    _averagingCfg = pmcfg.Averaging;
    sanitizeAveragingConfig(_averagingCfg);
    resetPowerStats();

    if (!pmcfg.Enabled) { return; }

    switch(static_cast<Provider::Type>(pmcfg.Source)) {
        case Provider::Type::MQTT:
            _upProvider = std::make_unique<::PowerMeters::Json::Mqtt::Provider>(pmcfg.Mqtt);
            break;
        case Provider::Type::SDM1PH:
            _upProvider = std::make_unique<::PowerMeters::Sdm::Serial::Provider>(
                    ::PowerMeters::Sdm::Serial::Provider::Phases::One, pmcfg.SerialSdm);
            break;
        case Provider::Type::SDM3PH:
            _upProvider = std::make_unique<::PowerMeters::Sdm::Serial::Provider>(
                    ::PowerMeters::Sdm::Serial::Provider::Phases::Three, pmcfg.SerialSdm);
            break;
        case Provider::Type::HTTP_JSON:
            _upProvider = std::make_unique<::PowerMeters::Json::Http::Provider>(pmcfg.HttpJson);
            break;
        case Provider::Type::SERIAL_SML:
            _upProvider = std::make_unique<::PowerMeters::Sml::Serial::Provider>();
            break;
        case Provider::Type::SMAHM2:
            _upProvider = std::make_unique<::PowerMeters::SmaHM::Udp::Provider>();
            break;
        case Provider::Type::HTTP_SML:
            _upProvider = std::make_unique<::PowerMeters::Sml::Http::Provider>(pmcfg.HttpSml);
            break;
        case Provider::Type::MODBUS_UDP_VICTRON:
            _upProvider = std::make_unique<::PowerMeters::Modbus::Udp::Victron::Provider>(pmcfg.UdpVictron);
            break;
    }

    if (!_upProvider->init()) {
        _upProvider = nullptr;
    }
}

float Controller::getPowerTotal() const
{
    std::lock_guard<std::mutex> l(_mutex);
    if (!_upProvider) { return 0.0f; }

    auto self = const_cast<Controller*>(this);
    self->refreshAgingForTimeBasedWindows();

    auto const& total = channelState(Channel::Total).Stats;
    if (_averagingCfg.Enabled && total.Average) {
        return *total.Average;
    }

    // Edge-case for planned averaging: after data timeout, an average window may
    // become empty. Fallback to the most recent raw total value first.
    if (total.Raw) {
        return *total.Raw;
    }

    return _upProvider->getPowerTotal();
}

Controller::PowerStats Controller::getPowerStats() const
{
    std::lock_guard<std::mutex> l(_mutex);
    if (!_upProvider) { return {}; }

    auto self = const_cast<Controller*>(this);
    self->refreshAgingForTimeBasedWindows();

    PowerStats result;
    result.Total = channelState(Channel::Total).Stats;
    result.L1 = channelState(Channel::L1).Stats;
    result.L2 = channelState(Channel::L2).Stats;
    result.L3 = channelState(Channel::L3).Stats;
    return result;
}

uint32_t Controller::getLastUpdate() const
{
    std::lock_guard<std::mutex> l(_mutex);
    if (!_upProvider) { return 0; }
    return _upProvider->getLastUpdate();
}

bool Controller::isDataValid() const
{
    std::lock_guard<std::mutex> l(_mutex);
    if (!_upProvider) { return false; }
    return _upProvider->isDataValid();
}

void Controller::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_upProvider) { return; }
    _upProvider->loop();
    updatePowerStatsFromProvider();

    auto const& pmcfg = Configuration.get().PowerMeter;
    // we don't need to republish data received from MQTT
    if (pmcfg.Source == static_cast<uint8_t>(Provider::Type::MQTT)) { return; }
    _upProvider->mqttLoop();
}

} // namespace PowerMeters
