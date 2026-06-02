#pragma once

#include <string>
#include <vector>
#include <unordered_map>

using Record = std::unordered_map<std::string, std::string>;

struct IDataSource {
    virtual ~IDataSource() = default;
    virtual std::vector<Record> GetData() const = 0;
    virtual std::string GetFilePath() const = 0;
};

struct JsonFileDataSource : public IDataSource {
    std::string filePath;

    JsonFileDataSource(const std::string& path) : filePath(path) {}
    std::vector<Record> GetData() const override;
    std::string GetFilePath() const override { return filePath; }
};

struct CsvFileDataSource : public IDataSource {
    std::string filePath;

    CsvFileDataSource(const std::string& path) : filePath(path) {}
    std::vector<Record> GetData() const override;
    std::string GetFilePath() const override { return filePath; }
};
