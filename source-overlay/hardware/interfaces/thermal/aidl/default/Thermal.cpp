/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Mainline device implementation of the Android thermal HAL.
 *
 * It publishes the kernel thermal framework directly: every
 * /sys/class/thermal/thermal_zoneN becomes a Temperature (and matching
 * TemperatureThreshold built from its trip points), and every
 * /sys/class/thermal/cooling_deviceN becomes a CoolingDevice.  This is what
 * mainline SoCs such as msm8953 expose; the Qualcomm sysfs names differ from
 * the ones the in-tree QTI HAL hardcodes, so the generic reader is used
 * instead.
 */

#define LOG_TAG "thermal_mainline"

#include "Thermal.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>

#include <dirent.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace aidl::android::hardware::thermal::impl::example {

using ndk::ScopedAStatus;

namespace {

constexpr char kThermalRoot[] = "/sys/class/thermal";
/* ThrottlingSeverity: NONE, LIGHT, MODERATE, SEVERE, CRITICAL, EMERGENCY,
 * SHUTDOWN. */
constexpr size_t kSeverityCount = 7;
constexpr size_t kLightSeverity = 1;
constexpr size_t kShutdownSeverity = 6;

bool interfacesEqual(const std::shared_ptr<::ndk::ICInterface>& left,
                     const std::shared_ptr<::ndk::ICInterface>& right) {
    if (left == nullptr || right == nullptr || !left->isRemote() || !right->isRemote()) {
        return left == right;
    }
    return left->asBinder() == right->asBinder();
}

bool readString(const std::string& path, std::string* out) {
    std::string data;
    if (!::android::base::ReadFileToString(path, &data)) {
        return false;
    }
    *out = ::android::base::Trim(data);
    return true;
}

bool readFloat(const std::string& path, float* out) {
    std::string data;
    if (!readString(path, &data)) {
        return false;
    }
    char* end = nullptr;
    float value = std::strtof(data.c_str(), &end);
    if (end == data.c_str()) {
        return false;
    }
    *out = value;
    return true;
}

bool readLong(const std::string& path, long* out) {
    std::string data;
    if (!readString(path, &data)) {
        return false;
    }
    char* end = nullptr;
    long value = std::strtol(data.c_str(), &end, 10);
    if (end == data.c_str()) {
        return false;
    }
    *out = value;
    return true;
}

std::vector<std::string> listThermalEntries(const char* prefix) {
    std::vector<std::string> names;
    DIR* dir = opendir(kThermalRoot);
    if (dir == nullptr) {
        return names;
    }
    size_t prefixLen = strlen(prefix);
    while (struct dirent* entry = readdir(dir)) {
        if (strncmp(entry->d_name, prefix, prefixLen) == 0) {
            names.emplace_back(entry->d_name);
        }
    }
    closedir(dir);
    std::sort(names.begin(), names.end());
    return names;
}

TemperatureType temperatureTypeFrom(const std::string& name) {
    if (name.compare(0, 3, "cpu") == 0) {
        return TemperatureType::CPU;
    }
    if (name.find("gpu") != std::string::npos) {
        return TemperatureType::GPU;
    }
    if (name.find("battery") != std::string::npos ||
        name.find("batt") != std::string::npos) {
        return TemperatureType::BATTERY;
    }
    if (name.find("skin") != std::string::npos) {
        return TemperatureType::SKIN;
    }
    if (name.find("modem") != std::string::npos) {
        return TemperatureType::MODEM;
    }
    if (name.find("display") != std::string::npos) {
        return TemperatureType::DISPLAY;
    }
    if (name.find("wifi") != std::string::npos) {
        return TemperatureType::WIFI;
    }
    if (name.find("camera") != std::string::npos) {
        return TemperatureType::CAMERA;
    }
    /* remap, quiet, pmic and other unnamed SoC zones. */
    return TemperatureType::SOC;
}

CoolingType coolingTypeFrom(const std::string& name) {
    if (name.find("cpufreq") != std::string::npos ||
        name.find("cpu") != std::string::npos) {
        return CoolingType::CPU;
    }
    if (name.find("gpu") != std::string::npos ||
        name.find("devfreq") != std::string::npos) {
        return CoolingType::GPU;
    }
    if (name.find("battery") != std::string::npos ||
        name.find("charger") != std::string::npos) {
        return CoolingType::BATTERY;
    }
    return CoolingType::COMPONENT;
}

std::vector<float> nanThresholds() {
    return std::vector<float>(kSeverityCount, NAN);
}

}  // namespace

ScopedAStatus Thermal::getCoolingDevices(std::vector<CoolingDevice>* out_devices) {
    LOG(VERBOSE) << __func__;
    std::vector<CoolingDevice> devices;
    for (const std::string& entry : listThermalEntries("cooling_device")) {
        const std::string base = std::string(kThermalRoot) + "/" + entry;
        std::string type;
        long state = 0;
        if (!readString(base + "/type", &type) || !readLong(base + "/cur_state", &state)) {
            continue;
        }
        CoolingDevice device;
        device.type = coolingTypeFrom(type);
        device.name = type;
        device.value = state;
        device.powerLimitMw = 0;
        device.powerMw = 0;
        device.timeWindowMs = 0;
        devices.push_back(device);
    }
    *out_devices = std::move(devices);
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::getCoolingDevicesWithType(CoolingType in_type,
                                                 std::vector<CoolingDevice>* out_devices) {
    LOG(VERBOSE) << __func__ << " CoolingType: " << static_cast<int32_t>(in_type);
    std::vector<CoolingDevice> devices;
    ScopedAStatus status = getCoolingDevices(&devices);
    if (!status.isOk()) {
        return status;
    }
    devices.erase(std::remove_if(devices.begin(), devices.end(),
                                 [&](const CoolingDevice& d) { return d.type != in_type; }),
                  devices.end());
    *out_devices = std::move(devices);
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::getTemperatures(std::vector<Temperature>* out_temperatures) {
    LOG(VERBOSE) << __func__;
    std::vector<Temperature> temperatures;
    for (const std::string& entry : listThermalEntries("thermal_zone")) {
        const std::string base = std::string(kThermalRoot) + "/" + entry;
        std::string type;
        float milli = 0;
        if (!readString(base + "/type", &type) || !readFloat(base + "/temp", &milli)) {
            continue;
        }
        Temperature temperature;
        temperature.type = temperatureTypeFrom(type);
        temperature.name = type;
        temperature.value = milli / 1000.0f;
        temperature.throttlingStatus = ThrottlingSeverity::NONE;
        temperatures.push_back(temperature);
    }
    *out_temperatures = std::move(temperatures);
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::getTemperaturesWithType(TemperatureType in_type,
                                               std::vector<Temperature>* out_temperatures) {
    LOG(VERBOSE) << __func__ << " TemperatureType: " << static_cast<int32_t>(in_type);
    std::vector<Temperature> temperatures;
    ScopedAStatus status = getTemperatures(&temperatures);
    if (!status.isOk()) {
        return status;
    }
    temperatures.erase(std::remove_if(temperatures.begin(), temperatures.end(),
                                      [&](const Temperature& t) { return t.type != in_type; }),
                       temperatures.end());
    *out_temperatures = std::move(temperatures);
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::getTemperatureThresholds(
        std::vector<TemperatureThreshold>* out_temperatureThresholds) {
    LOG(VERBOSE) << __func__;
    std::vector<TemperatureThreshold> thresholds;
    for (const std::string& entry : listThermalEntries("thermal_zone")) {
        const std::string base = std::string(kThermalRoot) + "/" + entry;
        std::string type;
        if (!readString(base + "/type", &type)) {
            continue;
        }

        std::vector<float> hot = nanThresholds();
        for (int i = 0;; i++) {
            const std::string trip = base + "/trip_point_" + std::to_string(i);
            float milli = 0;
            std::string tripType;
            if (!readFloat(trip + "_temp", &milli)) {
                break;
            }
            readString(trip + "_type", &tripType);
            const float celsius = milli / 1000.0f;
            if (tripType == "critical") {
                hot[kShutdownSeverity] = celsius;
            } else if (tripType == "passive" || tripType == "hot") {
                if (std::isnan(hot[kLightSeverity])) {
                    hot[kLightSeverity] = celsius;
                }
            }
        }

        TemperatureThreshold threshold;
        threshold.type = temperatureTypeFrom(type);
        threshold.name = type;
        threshold.hotThrottlingThresholds = std::move(hot);
        threshold.coldThrottlingThresholds = nanThresholds();
        thresholds.push_back(threshold);
    }
    *out_temperatureThresholds = std::move(thresholds);
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::getTemperatureThresholdsWithType(
        TemperatureType in_type,
        std::vector<TemperatureThreshold>* out_temperatureThresholds) {
    LOG(VERBOSE) << __func__ << " TemperatureType: " << static_cast<int32_t>(in_type);
    std::vector<TemperatureThreshold> thresholds;
    ScopedAStatus status = getTemperatureThresholds(&thresholds);
    if (!status.isOk()) {
        return status;
    }
    thresholds.erase(std::remove_if(thresholds.begin(), thresholds.end(),
                                    [&](const TemperatureThreshold& t) {
                                        return t.type != in_type;
                                    }),
                     thresholds.end());
    *out_temperatureThresholds = std::move(thresholds);
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::registerThermalChangedCallback(
        const std::shared_ptr<IThermalChangedCallback>& in_callback) {
    LOG(VERBOSE) << __func__ << " IThermalChangedCallback: " << in_callback;
    if (in_callback == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Invalid nullptr callback");
    }
    {
        std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
        if (std::any_of(thermal_callbacks_.begin(), thermal_callbacks_.end(),
                        [&](const std::shared_ptr<IThermalChangedCallback>& c) {
                            return interfacesEqual(c, in_callback);
                        })) {
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "Callback already registered");
        }
        thermal_callbacks_.push_back(in_callback);
    }
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::registerThermalChangedCallbackWithType(
        const std::shared_ptr<IThermalChangedCallback>& in_callback, TemperatureType in_type) {
    LOG(VERBOSE) << __func__ << " IThermalChangedCallback: " << in_callback
                 << ", TemperatureType: " << static_cast<int32_t>(in_type);
    if (in_callback == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Invalid nullptr callback");
    }
    {
        std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
        if (std::any_of(thermal_callbacks_.begin(), thermal_callbacks_.end(),
                        [&](const std::shared_ptr<IThermalChangedCallback>& c) {
                            return interfacesEqual(c, in_callback);
                        })) {
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "Callback already registered");
        }
        thermal_callbacks_.push_back(in_callback);
    }
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::unregisterThermalChangedCallback(
        const std::shared_ptr<IThermalChangedCallback>& in_callback) {
    LOG(VERBOSE) << __func__ << " IThermalChangedCallback: " << in_callback;
    if (in_callback == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Invalid nullptr callback");
    }
    {
        std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
        bool removed = false;
        thermal_callbacks_.erase(
                std::remove_if(thermal_callbacks_.begin(), thermal_callbacks_.end(),
                               [&](const std::shared_ptr<IThermalChangedCallback>& c) {
                                   if (interfacesEqual(c, in_callback)) {
                                       removed = true;
                                       return true;
                                   }
                                   return false;
                               }),
                thermal_callbacks_.end());
        if (!removed) {
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "Callback wasn't registered");
        }
    }
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::registerCoolingDeviceChangedCallbackWithType(
        const std::shared_ptr<ICoolingDeviceChangedCallback>& in_callback, CoolingType in_type) {
    LOG(VERBOSE) << __func__ << " ICoolingDeviceChangedCallback: " << in_callback
                 << ", CoolingType: " << static_cast<int32_t>(in_type);
    if (in_callback == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Invalid nullptr callback");
    }
    {
        std::lock_guard<std::mutex> _lock(cdev_callback_mutex_);
        if (std::any_of(cdev_callbacks_.begin(), cdev_callbacks_.end(),
                        [&](const std::shared_ptr<ICoolingDeviceChangedCallback>& c) {
                            return interfacesEqual(c, in_callback);
                        })) {
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "Callback already registered");
        }
        cdev_callbacks_.push_back(in_callback);
    }
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::unregisterCoolingDeviceChangedCallback(
        const std::shared_ptr<ICoolingDeviceChangedCallback>& in_callback) {
    LOG(VERBOSE) << __func__ << " ICoolingDeviceChangedCallback: " << in_callback;
    if (in_callback == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Invalid nullptr callback");
    }
    {
        std::lock_guard<std::mutex> _lock(cdev_callback_mutex_);
        bool removed = false;
        cdev_callbacks_.erase(
                std::remove_if(cdev_callbacks_.begin(), cdev_callbacks_.end(),
                               [&](const std::shared_ptr<ICoolingDeviceChangedCallback>& c) {
                                   if (interfacesEqual(c, in_callback)) {
                                       removed = true;
                                       return true;
                                   }
                                   return false;
                               }),
                cdev_callbacks_.end());
        if (!removed) {
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "Callback wasn't registered");
        }
    }
    return ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::forecastSkinTemperature(int32_t, float*) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

}  // namespace aidl::android::hardware::thermal::impl::example
