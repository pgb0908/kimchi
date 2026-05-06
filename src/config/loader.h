#pragma once

#include "config/models.h"

#include <filesystem>
#include <string>

namespace kimchi::config {

class ConfigLoader {
public:
    static ConfigStore loadFromDirectory(const std::filesystem::path& configDir);
    static void applyJsonDocument(const std::string& jsonText, ConfigStore& store);

private:
    static void dispatchDocument(const folly::dynamic& doc, ConfigStore& store);
};

} // namespace kimchi::config
