#pragma once

#include "duckdb.hpp"

namespace duckdb {

class SittingDuckExtension : public Extension {
public:
	void Load(DuckDB &db) override;
	string Name() override;
	string Version() const override;
};

} // namespace duckdb