//# This file is a part of toml++ and is subject to the the terms of the MIT license.
//# Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
//# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT
#pragma once

#include "forward_declarations.hpp"
#include "std_vector.hpp"
#include "header_start.hpp"

TOML_NAMESPACE_START
{
	/// \brief Indicates type of path component, either a key, an index in an array, or invalid
	enum class TOML_CLOSED_ENUM path_component_type : uint8_t
	{
		key			= 0x1,
		array_index = 0x2
	};

	/// \brief Represents a single component of a complete 'TOML-path': either a key or an array index
	class TOML_EXPORTED_CLASS path_component
	{
		/// \cond
		struct storage_t
		{
			static constexpr size_t size =
				(sizeof(size_t) < sizeof(std::string) ? sizeof(std::string) : sizeof(size_t));
			static constexpr size_t align =
				(alignof(size_t) < alignof(std::string) ? alignof(std::string) : alignof(size_t));

			alignas(align) unsigned char bytes[size];
		};
		alignas(storage_t::align) mutable storage_t value_storage_;

		path_component_type type_;

		TOML_PURE_GETTER
		TOML_EXPORTED_STATIC_FUNCTION
		static bool TOML_CALLCONV equal(const path_component&, const path_component&) noexcept;

		template <typename Type>
		TOML_PURE_INLINE_GETTER
		static Type* get_as(storage_t& s) noexcept
		{
			return TOML_LAUNDER(reinterpret_cast<Type*>(s.bytes));
		}

		static void store_key(std::string_view key, storage_t& storage_)
		{
			::new (static_cast<void*>(storage_.bytes)) std::string{ key };
		}

		static void store_index(size_t index, storage_t& storage_) noexcept
		{
			::new (static_cast<void*>(storage_.bytes)) std::size_t{ index };
		}

		void destroy() noexcept
		{
			if (type_ == path_component_type::key)
				get_as<std::string>(value_storage_)->~basic_string();
		}

		TOML_NODISCARD
		size_t& index_ref() noexcept
		{
			TOML_ASSERT_ASSUME(type_ == path_component_type::array_index);
			return *get_as<size_t>(value_storage_);
		}

		TOML_NODISCARD
		std::string& key_ref() noexcept
		{
			TOML_ASSERT_ASSUME(type_ == path_component_type::key);
			return *get_as<std::string>(value_storage_);
		}
		/// \endcond

	  public:
		/// \brief	Default constructor (creates an empty key).
		TOML_NODISCARD_CTOR
		TOML_EXPORTED_MEMBER_FUNCTION
		path_component();

		/// \brief	Constructor for a path component that is an array index
		TOML_NODISCARD_CTOR
		TOML_EXPORTED_MEMBER_FUNCTION
		path_component(size_t index) noexcept;

		/// \brief	Constructor for a path component that is a key string
		TOML_NODISCARD_CTOR
		TOML_EXPORTED_MEMBER_FUNCTION
		path_component(std::string_view key);

#if TOML_ENABLE_WINDOWS_COMPAT

		/// \brief	Constructor for a path component that is a key string
		///
		/// \availability This constructor is only available when #TOML_ENABLE_WINDOWS_COMPAT is enabled.
		TOML_NODISCARD_CTOR
		TOML_EXPORTED_MEMBER_FUNCTION
		path_component(std::wstring_view key);

#endif

		/// \brief	Copy constructor.
		TOML_NODISCARD_CTOR
		TOML_EXPORTED_MEMBER_FUNCTION
		path_component(const path_component& pc);

		/// \brief	Move constructor.
		TOML_NODISCARD_CTOR
		TOML_EXPORTED_MEMBER_FUNCTION
		path_component(path_component&& pc) noexcept;

		/// \brief	Copy-assignment operator.
		TOML_EXPORTED_MEMBER_FUNCTION
		path_component& operator=(const path_component& rhs);

		/// \brief	Move-assignment operator.
		TOML_EXPORTED_MEMBER_FUNCTION
		path_component& operator=(path_component&& rhs) noexcept;

		/// \brief Assigns an array index to this path component.
		TOML_EXPORTED_MEMBER_FUNCTION
		path_component& operator=(size_t new_index) noexcept;

		/// \brief Assigns a path key to this path component.
		TOML_EXPORTED_MEMBER_FUNCTION
		path_component& operator=(std::string_view new_key);

#if TOML_ENABLE_WINDOWS_COMPAT

		/// \brief Assigns a path key to this path component.
		///
		/// \availability This overload is only available when #TOML_ENABLE_WINDOWS_COMPAT is enabled.
		TOML_EXPORTED_MEMBER_FUNCTION
		path_component& operator=(std::wstring_view new_key);

#endif

		/// \brief	Destructor.
		~path_component() noexcept
		{
			destroy();
		}

		/// \name Array index accessors
		/// \warning It is undefined behaviour to call these functions when the path component does not represent an array index.
		/// Check #type() to determine the component's value type.
		/// @{

		/// \brief	Returns the array index (const lvalue overload).
		TOML_PURE_GETTER
		size_t index() const noexcept
		{
			TOML_ASSERT_ASSUME(type_ == path_component_type::array_index);
			return *get_as<const size_t>(value_storage_);
		}

		/// \brief	Returns the array index (const lvalue).
		TOML_PURE_INLINE_GETTER
		explicit operator size_t() const noexcept
		{
			return index();
		}

		/// @}

		/// \name Key accessors
		/// \warning It is undefined behaviour to call these functions when the path component does not represent a key.
		/// Check #type() to determine the component's value type.
		/// @{

		/// \brief	Returns the key string.
		TOML_PURE_GETTER
		const std::string& key() const noexcept
		{
			TOML_ASSERT_ASSUME(type_ == path_component_type::key);
			return *get_as<const std::string>(value_storage_);
		}

		/// \brief	Returns the key string.
		TOML_PURE_INLINE_GETTER
		explicit operator const std::string&() const noexcept
		{
			return key();
		}

		/// @}

		/// \brief Retrieve the type of this path component, either path_component::key or path_component::array_index
		TOML_PURE_INLINE_GETTER
		path_component_type type() const noexcept
		{
			return type_;
		}

		/// \name Equality
		/// @{

		/// \brief	Returns true if two path components represent the same key or array index.
		TOML_PURE_INLINE_GETTER
		friend bool operator==(const path_component& lhs, const path_component& rhs) noexcept
		{
			return equal(lhs, rhs);
		}

		/// \brief	Returns true if two path components do not represent the same key or array index.
		TOML_PURE_INLINE_GETTER
		friend bool operator!=(const path_component& lhs, const path_component& rhs) noexcept
		{
			return !equal(lhs, rhs);
		}

		/// @}
	};

	/// \brief	A TOML path.
	///
	/// \detail This type parses and represents a path to a TOML node. It validates
	///         the syntax of the path but does not ensure that the path refers to
	///         a valid node in any particular TOML document. If parsing fails,
	///         the object will evaluate as 'falsy', and will be empty.
	///
	/// \cpp
	/// toml::path the_path("animals.cats[1]");
	///
	/// // can use with tbl.at_path or operator[]
	/// std::cout << "second cat: " << tbl[the_path] << "\n";
	/// std::cout << "cats: " << tbl.at_path(the_path.parent_path()) << "\n";
	/// \ecpp
	///
	/// \out
	/// second cat: lion
	/// cats: ['tiger', 'lion', 'puma']
	/// \eout
	class TOML_EXPORTED_CLASS path
	{
	  private:
		/// \cond

		std::vector<path_component> components_;

		TOML_EXPORTED_MEMBER_FUNCTION
		void print_to(std::ostream&) const;

		TOML_PURE_GETTER
		TOML_EXPORTED_STATIC_FUNCTION
		static bool TOML_CALLCONV equal(const path&, const path&) noexcept;

		/// \endcond

	  public:
		/// \brief	Default constructor.
		TOML_NODISCARD_CTOR
		path() noexcept = default;

		/// \brief Construct a path by parsing from a string.
		TOML_NODISCARD_CTOR
		TOML_EXPORTED_MEMBER_FUNCTION
		explicit path(std::string_view);

#if TOML_ENABLE_WINDOWS_COMPAT

		/// \brief Construct a path by parsing from a string.
		///
		/// \availability This constructor is only available when #TOML_ENABLE_WINDOWS_COMPAT is enabled.
		TOML_NODISCARD_CTOR
		TOML_EXPORTED_MEMBER_FUNCTION
		explicit path(std::wstring_view);

#endif

		/// \brief	Default destructor.
		~path() noexcept = default;

		/// \brief	Copy constructor.
		TOML_NODISCARD_CTOR
		path(const path&) = default;

		/// \brief	Move constructor.
		TOML_NODISCARD_CTOR
		path(path&&) noexcept = default;

		/// \brief Returns the number of components in the path.
		TOML_PURE_INLINE_GETTER
		size_t size() const noexcept
		{
			return components_.size();
		}

		/// \brief Returns true if the path has one or more components.
		TOML_PURE_INLINE_GETTER
		explicit operator bool() const noexcept
		{
			return !components_.empty();
		}

		/// \brief Whether (true) or not (false) the path is empty
		TOML_PURE_INLINE_GETTER
		bool empty() const noexcept
		{
			return components_.empty();
		}

		/// \brief Fetch a path component by index.
		TOML_PURE_INLINE_GETTER
		path_component& operator[](size_t index) noexcept
		{
			TOML_ASSERT(index < size());
			return components_[index];
		}

		/// \brief Fetch a path component by index (const overload).
		TOML_PURE_INLINE_GETTER
		const path_component& operator[](size_t index) const noexcept
		{
			TOML_ASSERT(index < size());
			return components_[index];
		}

		/// \name Assignment
		/// @{

		/// \brief	Copy-assignment operator.
		path& operator=(const path&) = default;

		/// \brief	Move-assignment operator.
		path& operator=(path&&) noexcept = default;

		/// \brief	Replaces the contents of the path by parsing from a string.
		TOML_EXPORTED_MEMBER_FUNCTION
		path& operator=(std::string_view);

#if TOML_ENABLE_WINDOWS_COMPAT

		/// \brief	Replaces the contents of the path by parsing from a string.
		///
		/// \availability This overload is only available when #TOML_ENABLE_WINDOWS_COMPAT is enabled.
		TOML_EXPORTED_MEMBER_FUNCTION
		path& operator=(std::wstring_view);

#endif

		/// \brief	Replaces the contents of the path with that of another.
		TOML_ALWAYS_INLINE
		path& assign(const path& p)
		{
			return *this = p;
		}

		/// \brief	Replaces the contents of the path with that of another.
		TOML_ALWAYS_INLINE
		path& assign(path&& p) noexcept
		{
			return *this = std::move(p);
		}

		/// \brief	Replaces the contents of the path object by a new path
		TOML_ALWAYS_INLINE
		path& assign(std::string_view str)
		{
			return *this = str;
		}

#if TOML_ENABLE_WINDOWS_COMPAT

		/// \brief	Replaces the contents of the path object by a new path
		///
		/// \availability This overload is only available when #TOML_ENABLE_WINDOWS_COMPAT is enabled.
		TOML_ALWAYS_INLINE
		path& assign(std::wstring_view str)
		{
			return *this = str;
		}

#endif

		/// @}

		/// \name Appending
		/// @{

		/// \brief	Appends another path onto the end of this one.
		TOML_EXPORTED_MEMBER_FUNCTION
		path& operator+=(const path&);

		/// \brief	Appends another path onto the end of this one.
		TOML_EXPORTED_MEMBER_FUNCTION
		path& operator+=(path&&);

		/// \brief	Parses a path and appends it onto the end of this one.
		TOML_EXPORTED_MEMBER_FUNCTION
		path& operator+=(std::string_view);

#if TOML_ENABLE_WINDOWS_COMPAT

		/// \brief	Parses a path and appends it onto the end of this one.
		///
		/// \availability This overload is only available when #TOML_ENABLE_WINDOWS_COMPAT is enabled.
		TOML_EXPORTED_MEMBER_FUNCTION
		path& operator+=(std::wstring_view);

#endif

		/// \brief	Appends another path onto the end of this one.
		TOML_ALWAYS_INLINE
		path& append(const path& p)
		{
			return *this += p;
		}

		/// \brief	Appends another path onto the end of this one.
		TOML_ALWAYS_INLINE
		path& append(path&& p)
		{
			return *this += std::move(p);
		}

		/// \brief	Parses a path and appends it onto the end of this one.
		TOML_ALWAYS_INLINE
		path& append(std::string_view str)
		{
			return *this += str;
		}

#if TOML_ENABLE_WINDOWS_COMPAT

		/// \brief	Parses a path and appends it onto the end of this one.
		///
		/// \availability This overload is only available when #TOML_ENABLE_WINDOWS_COMPAT is enabled.
		TOML_ALWAYS_INLINE
		path& append(std::wstring_view str)
		{
			return *this += str;
		}

#endif

		/// @}

		/// \name Prepending
		/// @{

		/// \brief	Prepends another path onto the beginning of this one.
		TOML_EXPORTED_MEMBER_FUNCTION
		path& prepend(const path&);

		/// \brief	Prepends another path onto the beginning of this one.
		TOML_EXPORTED_MEMBER_FUNCTION
		path& prepend(path&&);

		/// \brief	Parses a path and prepends it onto the beginning of this one.
		TOML_EXPORTED_MEMBER_FUNCTION
		path& prepend(std::string_view);

#if TOML_ENABLE_WINDOWS_COMPAT

		/// \brief	Parses a path and prepends it onto the beginning of this one.
		///
		/// \availability This overload is only available when #TOML_ENABLE_WINDOWS_COMPAT is enabled.
		TOML_EXPORTED_MEMBER_FUNCTION
		path& prepend(std::wstring_view);

#endif

		/// @}

		/// \name Concatenation
		/// @{

		/// \brief  Concatenates two paths.
		TOML_NODISCARD
		friend path operator+(const path& lhs, const path& rhs)
		{
			path result = lhs;
			result += rhs;
			return result;
		}

		/// \brief  Concatenates two paths.
		TOML_NODISCARD
		friend path operator+(const path& lhs, std::string_view rhs)
		{
			path result = lhs;
			result += rhs;
			return result;
		}

		/// \brief  Concatenates two paths.
		TOML_NODISCARD
		friend path operator+(std::string_view lhs, const path& rhs)
		{
			path result = rhs;
			result.prepend(lhs);
			return result;
		}

#if TOML_ENABLE_WINDOWS_COMPAT

		/// \brief  Concatenates two paths.
		///
		/// \availability This overload is only available when #TOML_ENABLE_WINDOWS_COMPAT is enabled.
		TOML_NODISCARD
		friend path operator+(const path& lhs, std::wstring_view rhs)
		{
			path result = lhs;
			result += rhs;
			return result;
		}

		/// \brief  Concatenates two paths.
		///
		/// \availability This overload is only available when #TOML_ENABLE_WINDOWS_COMPAT is enabled.
		TOML_NODISCARD
		friend path operator+(std::wstring_view lhs, const path& rhs)
		{
			path result = rhs;
			result.prepend(lhs);
			return result;
		}

#endif

		/// @}

		/// \name String conversion
		/// @{

		/// \brief	Prints the string representation of a #toml::path out to a stream.
		TOML_ALWAYS_INLINE
		friend std::ostream& operator<<(std::ostream& os, const path& rhs)
		{
			rhs.print_to(os);
			return os;
		}

		/// \brief Returns a string representation of this path.
		TOML_NODISCARD
		TOML_EXPORTED_MEMBER_FUNCTION
		std::string str() const;

		/// \brief Returns a string representation of this path.
		TOML_NODISCARD
		TOML_ALWAYS_INLINE
		explicit operator std::string() const
		{
			return str();
		}

#if TOML_ENABLE_WINDOWS_COMPAT

		/// \brief Returns a string representation of this path.
		///
		/// \availability This overload is only available when #TOML_ENABLE_WINDOWS_COMPAT is enabled.
		TOML_NODISCARD
		TOML_EXPORTED_MEMBER_FUNCTION
		std::wstring wide_str() const;

		/// \brief Returns a string representation of this path.
		///
		/// \availability This overload is only available when #TOML_ENABLE_WINDOWS_COMPAT is enabled.
		TOML_NODISCARD
		TOML_ALWAYS_INLINE
		explicit operator std::wstring() const
		{
			return wide_str();
		}

#endif

		/// @}

		/// \name Equality
		/// @{

		/// \brief Returns whether two paths are the same.
		TOML_PURE_INLINE_GETTER
		friend bool operator==(const path& lhs, const path& rhs) noexcept
		{
			return equal(lhs, rhs);
		}

		/// \brief Returns whether two paths are not the same.
		TOML_PURE_INLINE_GETTER
		friend bool operator!=(const path& lhs, const path& rhs) noexcept
		{
			return !equal(lhs, rhs);
		}

		/// \brief Returns whether two paths are the same.
		TOML_NODISCARD
		TOML_ALWAYS_INLINE
		friend bool operator==(const path& lhs, std::string_view rhs)
		{
			return lhs == path{ rhs };
		}

		/// \brief Returns whether two paths are the same.
		TOML_NODISCARD
		TOML_ALWAYS_INLINE
		friend bool operator==(std::string_view lhs, const path& rhs)
		{
			return rhs == lhs;
		}

		/// \brief Returns whether two paths are not the same.
		TOML_NODISCARD
		TOML_ALWAYS_INLINE
		friend bool operator!=(const path& lhs, std::string_view rhs)
		{
			return lhs != path{ rhs };
		}

		/// \brief Returns whether two paths are not the same.
		TOML_NODISCARD
		TOML_ALWAYS_INLINE
		friend bool operator!=(std::string_view lhs, const path& rhs)
		{
			return rhs != lhs;
		}

#if TOML_ENABLE_WINDOWS_COMPAT

		/// \brief Returns whether two paths are the same.
		///
		/// \availability This overload is only available when #TOML_ENABLE_WINDOWS_COMPAT is enabled.
		TOML_NODISCARD
		TOML_ALWAYS_INLINE
		friend bool operator==(const path& lhs, std::wstring_view rhs)
		{
			return lhs == path{ rhs };
		}

		/// \brief Returns whether two paths are the same.
		///
		/// \availability This overload is only available when #TOML_ENABLE_WINDOWS_COMPAT is enabled.
		TOML_NODISCARD
		TOML_ALWAYS_INLINE
		friend bool operator==(std::wstring_view lhs, const path& rhs)
		{
			return rhs == lhs;
		}

		/// \brief Returns whether two paths are not the same.
		///
		/// \availability This overload is only available when #TOML_ENABLE_WINDOWS_COMPAT is enabled.
		TOML_NODISCARD
		TOML_ALWAYS_INLINE
		friend bool operator!=(const path& lhs, std::wstring_view rhs)
		{
			return lhs != path{ rhs };
		}

		/// \brief Returns whether two paths are not the same.
		///
		/// \availability This overload is only available when #TOML_ENABLE_WINDOWS_COMPAT is enabled.
		TOML_NODISCARD
		TOML_ALWAYS_INLINE
		friend bool operator!=(std::wstring_view lhs, const path& rhs)
		{
			return rhs != lhs;
		}

#endif // TOML_ENABLE_WINDOWS_COMPAT

		/// @}

		/// \name Iteration
		/// @{

		/// An iterator for iterating over the components in the path.
		/// \see #toml::path_component
		using iterator = std::vector<path_component>::iterator;

		/// A const iterator for iterating over the components in the path.
		/// \see #toml::path_component
		using const_iterator = std::vector<path_component>::const_iterator;

		/// \brief  Returns an iterator to the first component in the path.
		/// \see #toml::path_component
		TOML_PURE_INLINE_GETTER
		iterator begin() noexcept
		{
			return components_.begin();
		}

		/// \brief  Returns an iterator to one-past-the-last component in the path.
		/// \see #toml::path_component
		TOML_PURE_INLINE_GETTER
		iterator end() noexcept
		{
			return components_.end();
		}

		/// \brief  Returns a const iterator to the first component in the path.
		/// \see #toml::path_component
		TOML_PURE_INLINE_GETTER
		const_iterator begin() const noexcept
		{
			return components_.begin();
		}

		/// \brief  Returns a const iterator to one-past-the-last component in the path.
		/// \see #toml::path_component
		TOML_PURE_INLINE_GETTER
		const_iterator end() const noexcept
		{
			return components_.end();
		}

		/// \brief  Returns a const iterator to the first component in the path.
		/// \see #toml::path_component
		TOML_PURE_INLINE_GETTER
		const_iterator cbegin() const noexcept
		{
			return components_.begin();
		}

		/// \brief  Returns a const iterator to one-past-the-last component in the path.
		/// \see #toml::path_component
		TOML_PURE_INLINE_GETTER
		const_iterator cend() const noexcept
		{
			return components_.end();
		}

		/// @}

		/// \name Subpaths and Truncation
		/// @{

		/// \brief	Erases the contents of the path.
		TOML_EXPORTED_MEMBER_FUNCTION
		void clear() noexcept;

		/// \brief Removes the number of terminal path components specified by n
		TOML_EXPORTED_MEMBER_FUNCTION
		path& truncate(size_t n);

		/// \brief Returns a toml::path object which has had n terminal path components removed
		TOML_NODISCARD
		TOML_EXPORTED_MEMBER_FUNCTION
		path truncated(size_t n) const;

		/// \brief	Returns a toml::path object representing the path of the parent node
		TOML_NODISCARD
		TOML_EXPORTED_MEMBER_FUNCTION
		path parent() const;

		/// \brief	Returns a toml::path object representing terminal n-parts of a TOML path
		TOML_NODISCARD
		TOML_EXPORTED_MEMBER_FUNCTION
		path leaf(size_t n = 1) const;

		/// \brief	Returns a toml::path object that is a specified subpath of the current path, representing the
		/// range of path components from [start, end).
		TOML_NODISCARD
		TOML_EXPORTED_MEMBER_FUNCTION
		path subpath(const_iterator start, const_iterator end) const;

		/// \brief	Returns a toml::path object that is a specified subpath of the current path, representing the
		/// range of path components with indexes from [start, start + length].
		TOML_NODISCARD
		TOML_EXPORTED_MEMBER_FUNCTION
		path subpath(size_t start, size_t length) const;

		/// @}
	};

	inline namespace literals
	{
		/// \brief	Parses a TOML path from a string literal.
		///
		/// \detail \cpp
		/// using namespace toml::literals;
		///
		/// auto path = "main.settings.devices[2]"_tpath;
		/// std::cout << path.parent_path() << "\n";
		/// \ecpp
		///
		/// \out
		/// main.settings.devices
		/// \eout
		///
		/// \param 	str	The string data.
		/// \param 	len	The string length.
		///
		/// \returns	A #toml::path generated from the string literal.
		TOML_NODISCARD
		TOML_ALWAYS_INLINE
		path operator"" _tpath(const char* str, size_t len)
		{
			return path(std::string_view{ str, len });
		}
	}

	/// \brief Returns a view of the node matching a fully-qualified "TOML path".
	///
	/// \detail \cpp
	/// auto config = toml::parse(R"(
	///
	/// [foo]
	/// bar = [ 0, 1, 2, [ 3 ], { kek = 4 } ]
	///
	/// )"sv);
	///
	/// toml::path path1("foo.bar[2]");
	/// toml::path path2("foo.bar[4].kek");
	/// std::cout << toml::at_path(config, path1) << "\n";
	/// std::cout << toml::at_path(config, path1.parent_path()) << "\n";
	/// std::cout << toml::at_path(config, path2) << "\n";
	/// std::cout << toml::at_path(config, path2.parent_path()) << "\n";
	/// \ecpp
	///
	/// \out
	/// 2
	/// [ 0, 1, 2, [ 3 ], { kek = 4 } ]
	/// 4
	/// { kek  = 4 }
	/// \eout
	///
	///
	/// \note Keys in paths are interpreted literally, so whitespace (or lack thereof) matters:
	/// \cpp
	/// toml::at_path(config, toml::path("foo.bar"))  // same as config["foo"]["bar"]
	/// toml::at_path(config, toml::path("foo. bar")) // same as config["foo"][" bar"]
	/// toml::at_path(config, toml::path("foo..bar")) // same as config["foo"][""]["bar"]
	/// toml::at_path(config, toml::path(".foo.bar")) // same as config[""]["foo"]["bar"]
	/// \ecpp
	/// <br>
	/// Additionally, TOML allows '.' (period) characters to appear in keys if they are quoted strings.
	/// This function makes no allowance for this, instead treating all period characters as sub-table delimiters.
	///
	/// \param root		The root node from which the path will be traversed.
	/// \param path		The "TOML path" to traverse.
	TOML_NODISCARD
	TOML_EXPORTED_FREE_FUNCTION
	node_view<node> TOML_CALLCONV at_path(node & root, const toml::path& path) noexcept;

	/// \brief Returns a const view of the node matching a fully-qualified "TOML path".
	///
	/// \see #toml::at_path(node&, const toml::path& path)
	TOML_NODISCARD
	TOML_EXPORTED_FREE_FUNCTION
	node_view<const node> TOML_CALLCONV at_path(const node& root, const toml::path& path) noexcept;
}
TOML_NAMESPACE_END;

#include "header_end.hpp"
