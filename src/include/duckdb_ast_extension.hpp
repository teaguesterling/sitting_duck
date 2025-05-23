#pragma once

#include "duckdb.hpp"

namespace duckdb {

class DuckDBASTExtension : public Extension {
public:
	void Load(DuckDB &db) override;
	string Name() override;
	string Version() const override;
};

} // namespace duckdb