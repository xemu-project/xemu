//# This file is a part of toml++ and is subject to the the terms of the MIT license.
//# Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
//# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT
#pragma once

//# {{
#include "preprocessor.hpp"
#if !TOML_IMPLEMENTATION
#error This is an implementation-only header.
#endif
//# }}

#include "path.hpp"
#include "at_path.hpp"
#include "print_to_stream.hpp"
TOML_DISABLE_WARNINGS;
#if TOML_INT_CHARCONV
#include <charconv>
#endif
#include <sstream>
TOML_ENABLE_WARNINGS;
#include "header_start.hpp"

//#=====================================================================================================================
//# toml::path_component
//#=====================================================================================================================

TOML_NAMESPACE_START
{
	TOML_EXTERNAL_LINKAGE
	path_component::path_component() //
		: type_{ path_component_type::key }
	{
		store_key("", value_storage_);
	}

	TOML_EXTERNAL_LINKAGE
	path_component::path_component(size_t index) noexcept //
		: type_(path_component_type::array_index)
	{
		store_index(index, value_storage_);
	}

	TOML_EXTERNAL_LINKAGE
	path_component::path_component(std::string_view key) //
		: type_(path_component_type::key)
	{
		store_key(key, value_storage_);
	}

#if TOML_ENABLE_WINDOWS_COMPAT

	TOML_EXTERNAL_LINKAGE
	path_component::path_component(std::wstring_view key) //
		: path_component(impl::narrow(key))
	{}

#endif

	TOML_EXTERNAL_LINKAGE
	path_component::path_component(const path_component& pc) //
		: type_{ pc.type_ }
	{
		if (type_ == path_component_type::array_index)
			store_index(pc.index(), value_storage_);
		else
			store_key(pc.key(), value_storage_);
	}

	TOML_EXTERNAL_LINKAGE
	path_component::path_component(path_component && pc) noexcept //
		: type_{ pc.type_ }
	{
		if (type_ == path_component_type::array_index)
			store_index(pc.index_ref(), value_storage_);
		else
			store_key(std::move(pc.key_ref()), value_storage_);
	}

	TOML_EXTERNAL_LINKAGE
	path_component& path_component::operator=(const path_component& rhs)
	{
		if (type_ != rhs.type_)
		{
			destroy();

			type_ = rhs.type_;
			if (type_ == path_component_type::array_index)
				store_index(rhs.index(), value_storage_);
			else
				store_key(rhs.key(), value_storage_);
		}
		else
		{
			if (type_ == path_component_type::array_index)
				index_ref() = rhs.index();
			else
				key_ref() = rhs.key();
		}
		return *this;
	}

	TOML_EXTERNAL_LINKAGE
	path_component& path_component::operator=(path_component&& rhs) noexcept
	{
		if (type_ != rhs.type_)
		{
			destroy();

			type_ = rhs.type_;
			if (type_ == path_component_type::array_index)
				store_index(rhs.index(), value_storage_);
			else
				store_key(std::move(rhs.key_ref()), value_storage_);
		}
		else
		{
			if (type_ == path_component_type::array_index)
				index_ref() = rhs.index();
			else
				key_ref() = std::move(rhs.key_ref());
		}
		return *this;
	}

	TOML_PURE_GETTER
	TOML_EXTERNAL_LINKAGE
	bool TOML_CALLCONV path_component::equal(const path_component& lhs, const path_component& rhs) noexcept
	{
		// Different comparison depending on contents
		if (lhs.type_ != rhs.type_)
			return false;

		if (lhs.type_ == path_component_type::array_index)
			return lhs.index() == rhs.index();
		else // path_component_type::key
			return lhs.key() == rhs.key();
	}

	TOML_EXTERNAL_LINKAGE
	path_component& path_component::operator=(size_t new_index) noexcept
	{
		// If currently a key, string will need to be destroyed regardless
		destroy();

		type_ = path_component_type::array_index;
		store_index(new_index, value_storage_);

		return *this;
	}

	TOML_EXTERNAL_LINKAGE
	path_component& path_component::operator=(std::string_view new_key)
	{
		if (type_ == path_component_type::key)
			key_ref() = new_key;
		else
		{
			type_ = path_component_type::key;
			store_key(new_key, value_storage_);
		}

		return *this;
	}

#if TOML_ENABLE_WINDOWS_COMPAT

	TOML_EXTERNAL_LINKAGE
	path_component& path_component::operator=(std::wstring_view new_key)
	{
		if (type_ == path_component_type::key)
			key_ref() = impl::narrow(new_key);
		else
		{
			type_ = path_component_type::key;
			store_key(impl::narrow(new_key), value_storage_);
		}

		return *this;
	}

#endif
}
TOML_NAMESPACE_END;

//#=====================================================================================================================
//# toml::path
//#=====================================================================================================================

TOML_ANON_NAMESPACE_START
{
	TOML_INTERNAL_LINKAGE
	bool parse_path_into(std::string_view path_str, std::vector<path_component> & components)
	{
		using components_type = std::remove_reference_t<decltype(components)>;

		const auto original_size = components.size();

		static constexpr auto on_key = [](void* data, std::string_view key) -> bool
		{
			auto& comps = *static_cast<components_type*>(data);
			comps.emplace_back(key);
			return true;
		};

		static constexpr auto on_index = [](void* data, size_t index) -> bool
		{
			auto& comps = *static_cast<components_type*>(data);
			comps.emplace_back(index);
			return true;
		};

		if (!impl::parse_path(path_str, &components, on_key, on_index))
		{
			components.resize(original_size);
			return false;
		}

		return true;
	}
}
TOML_ANON_NAMESPACE_END;

TOML_NAMESPACE_START
{
	TOML_EXTERNAL_LINKAGE
	void path::print_to(std::ostream & os) const
	{
		bool root = true;
		for (const auto& component : components_)
		{
			if (component.type() == path_component_type::key) // key
			{
				if (!root)
					impl::print_to_stream(os, '.');
				impl::print_to_stream(os, component.key());
			}
			else if (component.type() == path_component_type::array_index) // array
			{
				impl::print_to_stream(os, '[');
				impl::print_to_stream(os, component.index());
				impl::print_to_stream(os, ']');
			}
			root = false;
		}
	}

	TOML_PURE_GETTER
	TOML_EXTERNAL_LINKAGE
	bool TOML_CALLCONV path::equal(const path& lhs, const path& rhs) noexcept
	{
		return lhs.components_ == rhs.components_;
	}

	//#=== constructors =================================================

	TOML_EXTERNAL_LINKAGE
	path::path(std::string_view str) //
	{
		TOML_ANON_NAMESPACE::parse_path_into(str, components_);
	}

#if TOML_ENABLE_WINDOWS_COMPAT

	TOML_EXTERNAL_LINKAGE
	path::path(std::wstring_view str) //
		: path(impl::narrow(str))
	{}

#endif

	//#=== assignment =================================================

	TOML_EXTERNAL_LINKAGE
	path& path::operator=(std::string_view rhs)
	{
		components_.clear();
		TOML_ANON_NAMESPACE::parse_path_into(rhs, components_);
		return *this;
	}

#if TOML_ENABLE_WINDOWS_COMPAT

	TOML_EXTERNAL_LINKAGE
	path& path::operator=(std::wstring_view rhs)
	{
		return assign(impl::narrow(rhs));
	}

#endif

	//#=== appending =================================================

	TOML_EXTERNAL_LINKAGE
	path& path::operator+=(const path& rhs)
	{
		components_.insert(components_.cend(), rhs.begin(), rhs.end());
		return *this;
	}

	TOML_EXTERNAL_LINKAGE
	path& path::operator+=(path&& rhs)
	{
		components_.insert(components_.end(),
						   std::make_move_iterator(rhs.components_.begin()),
						   std::make_move_iterator(rhs.components_.end()));
		return *this;
	}

	TOML_EXTERNAL_LINKAGE
	path& path::operator+=(std::string_view str)
	{
		TOML_ANON_NAMESPACE::parse_path_into(str, components_);
		return *this;
	}

#if TOML_ENABLE_WINDOWS_COMPAT

	TOML_EXTERNAL_LINKAGE
	path& path::operator+=(std::wstring_view str)
	{
		return *this += impl::narrow(str);
	}

#endif

	//#=== prepending =================================================

	TOML_EXTERNAL_LINKAGE
	path& path::prepend(const path& source)
	{
		components_.insert(components_.begin(), source.components_.begin(), source.components_.end());
		return *this;
	}

	TOML_EXTERNAL_LINKAGE
	path& path::prepend(path && source)
	{
		components_.insert(components_.begin(),
						   std::make_move_iterator(source.components_.begin()),
						   std::make_move_iterator(source.components_.end()));
		return *this;
	}

	TOML_EXTERNAL_LINKAGE
	path& path::prepend(std::string_view source)
	{
		return prepend(path{ source });
	}

#if TOML_ENABLE_WINDOWS_COMPAT

	TOML_EXTERNAL_LINKAGE
	path& path::prepend(std::wstring_view source)
	{
		return prepend(impl::narrow(source));
	}

#endif

	//#=== string conversion =================================================

	TOML_EXTERNAL_LINKAGE
	std::string path::str() const
	{
		if (empty())
			return "";

		std::ostringstream ss;
		print_to(ss);
		return std::move(ss).str();
	}

#if TOML_ENABLE_WINDOWS_COMPAT

	TOML_EXTERNAL_LINKAGE
	std::wstring path::wide_str() const
	{
		return impl::widen(str());
	}

#endif

	//#=== equality and comparison =================================================

	TOML_EXTERNAL_LINKAGE
	void path::clear() noexcept
	{
		components_.clear();
	}

	TOML_EXTERNAL_LINKAGE
	path& path::truncate(size_t n)
	{
		n = n > components_.size() ? components_.size() : n;

		auto it_end = components_.end();
		components_.erase(it_end - static_cast<int>(n), it_end);

		return *this;
	}

	TOML_EXTERNAL_LINKAGE
	path path::truncated(size_t n) const
	{
		path truncated_path{};

		n = n > components_.size() ? components_.size() : n;

		// Copy all components except one
		// Need at least two path components to have a parent, since if there is
		// only one path component, the parent is the root/null path ""
		truncated_path.components_.insert(truncated_path.components_.begin(),
										  components_.begin(),
										  components_.end() - static_cast<int>(n));

		return truncated_path;
	}

	TOML_EXTERNAL_LINKAGE
	path path::parent() const
	{
		return truncated(1);
	}

	TOML_EXTERNAL_LINKAGE
	path path::leaf(size_t n) const
	{
		path leaf_path{};

		n = n > components_.size() ? components_.size() : n;

		if (n > 0)
		{
			leaf_path.components_.insert(leaf_path.components_.begin(),
										 components_.end() - static_cast<int>(n),
										 components_.end());
		}

		return leaf_path;
	}

	TOML_EXTERNAL_LINKAGE
	path path::subpath(std::vector<path_component>::const_iterator start,
					   std::vector<path_component>::const_iterator end) const
	{
		if (start >= end)
			return {};

		path subpath;
		subpath.components_.insert(subpath.components_.begin(), start, end);
		return subpath;
	}

	TOML_EXTERNAL_LINKAGE
	path path::subpath(size_t start, size_t length) const
	{
		return subpath(begin() + static_cast<int>(start), begin() + static_cast<int>(start + length));
	}
}
TOML_NAMESPACE_END;

//#=====================================================================================================================
//# at_path() overloads for toml::path
//#=====================================================================================================================

TOML_NAMESPACE_START
{
	TOML_EXTERNAL_LINKAGE
	node_view<node> TOML_CALLCONV at_path(node & root, const toml::path& path) noexcept
	{
		// early-exit sanity-checks
		if (root.is_value())
			return {};
		if (auto tbl = root.as_table(); tbl && tbl->empty())
			return {};
		if (auto arr = root.as_array(); arr && arr->empty())
			return {};

		node* current = &root;

		for (const auto& component : path)
		{
			auto type = component.type();
			if (type == path_component_type::array_index)
			{
				const auto current_array = current->as<array>();
				if (!current_array)
					return {}; // not an array, using array index doesn't work

				current = current_array->get(component.index());
			}
			else if (type == path_component_type::key)
			{
				const auto current_table = current->as<table>();
				if (!current_table)
					return {};

				current = current_table->get(component.key());
			}
			else
			{
				// Error: invalid component
				return {};
			}

			if (!current)
				return {}; // not found
		}

		return node_view{ current };
	}

	TOML_EXTERNAL_LINKAGE
	node_view<const node> TOML_CALLCONV at_path(const node& root, const toml::path& path) noexcept
	{
		return node_view<const node>{ at_path(const_cast<node&>(root), path).node() };
	}
}
TOML_NAMESPACE_END;

#include "header_end.hpp"
