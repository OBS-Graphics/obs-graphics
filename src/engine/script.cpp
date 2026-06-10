#include "script.h"

#include <iostream>

ScriptDataSource::ScriptDataSource(const std::string& path)
    : scriptFilePath(path)
{
    L.open_libraries(
        sol::lib::base,
        sol::lib::bit32,
        sol::lib::coroutine,
        sol::lib::io,
        sol::lib::math,
        sol::lib::os,
        sol::lib::package,
        sol::lib::string,
        sol::lib::table,
        sol::lib::utf8
    );
    L.safe_script_file(path);
    m_getData = L["_get_data"];
}

std::vector<Record> ScriptDataSource::GetData() const
{
    if (!m_getData) return {};

    auto result = m_getData();
    if (!result.valid()) {
        sol::error err = result;
        std::cerr << err.what() << std::endl;
        return {};
    }

    if (result.get_type() != sol::type::table) {
        sol::error err = result;
        std::cerr << "Data must be a table (list of objects)" << std::endl;
        return {};
    }

    sol::table table = result;
    std::vector<Record> records;
    records.reserve(table.size());

    for (auto& [i, rowValue] : table) {
        sol::table row = rowValue.as<sol::table>();
        Record rec{};
        for (auto& [k, v] : row) {
            std::string key = k.as<std::string>();
            switch (v.get_type()) {
                case sol::type::number:
                    rec[key] = v.is<int>()
                        ? std::to_string(v.as<int>())
                        : std::to_string(v.as<double>());
                    break;
                case sol::type::string:
                    rec[key] = v.as<std::string>();
                    break;
                case sol::type::boolean:
                    rec[key] = v.as<bool>() ? "true" : "false";
                    break;
                default:
                    break;
            }
        }
        records.push_back(std::move(rec));
    }

    return records;
}
