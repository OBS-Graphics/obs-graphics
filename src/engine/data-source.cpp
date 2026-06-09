#include "data-source.h"

#include "json.hpp"
#include <fstream>

using json = nlohmann::json;

std::vector<Record> JsonFileDataSource::GetData() const
{
    std::vector<Record> records;

    std::ifstream f(filePath);
    if (!f.is_open()) {
        throw std::runtime_error("Failed to open data source file: " + filePath);
    }

    json j;
    f >> j;

    if (!j.is_array()) {
        throw std::runtime_error("Expected JSON array in data source file: " + filePath);
    }

    for (const auto& item : j) {
        if (!item.is_object())
            continue;

        Record record;
        for (auto it = item.begin(); it != item.end(); ++it) {
            if (it.value().is_string()) {
                record[it.key()] = it.value().get<std::string>();
            } else {
                record[it.key()] = it.value().dump();
            }
        }
        records.push_back(std::move(record));
    }

    return records;
}

std::vector<Record> CsvFileDataSource::GetData() const
{
    std::vector<Record> records;

    std::ifstream f(filePath);
    if (!f.is_open()) {
        throw std::runtime_error("Failed to open data source file: " + filePath);
    }

    std::string line;
    std::vector<std::string> headers;

    // Read header line
    if (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string cell;
        while (std::getline(ss, cell, ',')) {
            headers.push_back(cell);
        }
    } else {
        throw std::runtime_error("CSV file is empty: " + filePath);
    }

    // Read data lines
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string cell;
        Record record;
        size_t idx = 0;
        while (std::getline(ss, cell, ',')) {
            if (idx < headers.size()) {
                record[headers[idx]] = cell;
            }
            idx++;
        }
        records.push_back(std::move(record));
    }

    return records;
}
