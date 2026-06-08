#pragma once

namespace leakproof
{
	void table_created() noexcept;
	void array_created() noexcept;
	void value_created() noexcept;
	void table_destroyed() noexcept;
	void array_destroyed() noexcept;
	void value_destroyed() noexcept;
}

#define TOML_LIFETIME_HOOKS	 1
#define TOML_TABLE_CREATED	 ::leakproof::table_created()
#define TOML_TABLE_DESTROYED ::leakproof::table_destroyed()
#define TOML_ARRAY_CREATED	 ::leakproof::array_created()
#define TOML_ARRAY_DESTROYED ::leakproof::array_destroyed()
#define TOML_VALUE_CREATED	 ::leakproof::value_created()
#define TOML_VALUE_DESTROYED ::leakproof::value_destroyed()
