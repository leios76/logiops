/*
 * Copyright 2019-2020 PixlOne
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <utility>
#include <vector>
#include <map>

#include "Configuration.h"
#include "util/log.h"

using namespace logid;
using namespace libconfig;
using namespace std::chrono;

Configuration::Configuration(const std::string& config_file)
{
    try {
        _config.readFile(config_file.c_str());
    } catch(const FileIOException &e) {
        logPrintf(ERROR, "I/O Error while reading %s: %s", config_file.c_str(),
                e.what());
        throw e;
    } catch(const ParseException &e) {
        logPrintf(ERROR, "Parse error in %s, line %d: %s", e.getFile(),
                e.getLine(), e.getError());
        throw e;
    }

    const Setting &root = _config.getRoot();
    Setting* devices;

    try { devices = &root["devices"]; }
    catch(const SettingNotFoundException &e) {
        logPrintf(WARN, "No devices listed in config file.");
        return;
    }

    _worker_threads = LOGID_DEFAULT_WORKER_COUNT;
    try {
        auto& worker_count = root["workers"];
        if(worker_count.getType() == Setting::TypeInt) {
            _worker_threads = worker_count;
            if(_worker_threads < 0)
                logPrintf(WARN, "Line %d: workers cannot be negative.",
                        worker_count.getSourceLine());
        } else {
            logPrintf(WARN, "Line %d: workers must be an integer.",
                    worker_count.getSourceLine());
        }
    } catch(const SettingNotFoundException& e) {
        // Ignore
    }

    _io_timeout = LOGID_DEFAULT_RAWDEVICE_TIMEOUT;
    try {
        auto& timeout = root["io_timeout"];
        if(timeout.isNumber()) {
            auto t = timeout.getType();
            if(timeout.getType() == Setting::TypeFloat)
                _io_timeout = duration_cast<milliseconds>(
                        duration<double, std::milli>(timeout));
            else
                _io_timeout = milliseconds((int)timeout);
        } else
            logPrintf(WARN, "Line %d: io_timeout must be a number.",
                    timeout.getSourceLine());
    } catch(const SettingNotFoundException& e) {
        // Ignore
    }

    for(int i = 0; i < devices->getLength(); i++) {
        const Setting &device = (*devices)[i];
        std::string name;
        try {
            if(!device.lookupValue("name", name)) {
                logPrintf(WARN, "Line %d: 'name' must be a string, skipping "
                                "device.", device["name"].getSourceLine());
                continue;
            }
        } catch(SettingNotFoundException &e) {
            logPrintf(WARN, "Line %d: Missing 'name' field, skipping device."
                , device.getSourceLine());
            continue;
        }
        _device_paths.insert({name, device.getPath()});
    }
}

libconfig::Setting& Configuration::getSetting(std::string path)
{
    return _config.lookup(path);
}

std::string Configuration::getDevice(std::string name)
{
    auto it = _device_paths.find(name);
    if(it == _device_paths.end())
        throw DeviceNotFound(name);
    else
        return it->second;
}

Configuration::DeviceNotFound::DeviceNotFound(std::string name) :
    _name (std::move(name))
{
}

const char * Configuration::DeviceNotFound::what() const noexcept
{
    return _name.c_str();
}

int Configuration::workerCount() const
{
    return _worker_threads;
}

std::chrono::milliseconds Configuration::ioTimeout() const
{
    return _io_timeout;
}
