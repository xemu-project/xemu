#pragma once

#include <cassert> // assert
#include <numeric> // accumulate
#include <string> // string
#include <vector> // vector

#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/detail/exceptions.hpp>
#include <nlohmann/detail/value_t.hpp>

namespace nlohmann
{
template<typename BasicJsonType>
class json_pointer
{
    // allow basic_json to access private members
    NLOHMANN_BASIC_JSON_TPL_DECLARATION
    friend class basic_json;

  public:
    /*!
    @brief create JSON pointer

    Create a JSON pointer according to the syntax described in
    [Section 3 of RFC6901](https://tools.ietf.org/html/rfc6901#section-3).

    @param[in] s  string representing the JSON pointer; if omitted, the empty
                  string is assumed which references the whole JSON value

    @throw parse_error.107 if the given JSON pointer @a s is nonempty and does
                           not begin with a slash (`/`); see example below

    @throw parse_error.108 if a tilde (`~`) in the given JSON pointer @a s is
    not followed by `0` (representing `~`) or `1` (representing `/`); see
    example below

    @liveexample{The example shows the construction several valid JSON pointers
    as well as the exceptional behavior.,json_pointer}

    @since version 2.0.0
    */
    explicit json_pointer(const std::string& s = "")
        : reference_tokens(split(s))
    {}

    /*!
    @brief return a string representation of the JSON pointer

    @invariant For each JSON pointer `ptr`, it holds:
    @code {.cpp}
    ptr == json_pointer(ptr.to_string());
    @endcode

    @return a string representation of the JSON pointer

    @liveexample{The example shows the result of `to_string`.,
    json_pointer__to_string}

    @since version 2.0.0
    */
    std::string to_string() const noexcept
    {
        return std::accumulate(reference_tokens.begin(), reference_tokens.end(),
                               std::string{},
                               [](const std::string & a, const std::string & b)
        {
            return a + "/" + escape(b);
        });
    }

    /// @copydoc to_string()
    operator std::string() const
    {
        return to_string();
    }

    /*!
    @param[in] s  reference token to be converted into an array index

    @return integer representation of @a s

    @throw out_of_range.404 if string @a s could not be converted to an integer
    */
    static int array_index(const std::string& s)
    {
        std::size_t processed_chars = 0;
        const int res = std::stoi(s, &processed_chars);

        // check if the string was completely read
        if (JSON_UNLIKELY(processed_chars != s.size()))
        {
            JSON_THROW(detail::out_of_range::create(404, "unresolved reference token '" + s + "'"));
        }

        return res;
    }

  private:
    /*!
    @brief remove and return last reference pointer
    @throw out_of_range.405 if JSON pointer has no parent
    */
    std::string pop_back()
    {
        if (JSON_UNLIKELY(is_root()))
        {
            JSON_THROW(detail::out_of_range::create(405, "JSON pointer has no parent"));
        }

        auto last = reference_tokens.back();
        reference_tokens.pop_back();
        return last;
    }

    /// return whether pointer points to the root document
    bool is_root() const
    {
        return reference_tokens.empty();
    }

    json_pointer top() const
    {
        if (JSON_UNLIKELY(is_root()))
        {
            JSON_THROW(detail::out_of_range::create(405, "JSON pointer has no parent"));
        }

        json_pointer result = *this;
        result.reference_tokens = {reference_tokens[0]};
        return result;
    }

    /*!
    @brief create and return a reference to the pointed to value

    @complexity Linear in the number of reference tokens.

    @throw parse_error.109 if array index is not a number
    @throw type_error.313 if value cannot be unflattened
    */
    BasicJsonType& get_and_create(BasicJsonType& j) const
    {
        using size_type = typename BasicJsonType::size_type;
        auto result = &j;

        // in case no reference tokens exist, return a reference to the JSON value
        // j which will be overwritten by a primitive value
        for (const auto& reference_token : reference_tokens)
        {
            switch (result->m_type)
            {
                case detail::value_t::null:
                {
                    if (reference_token == "0")
                    {
                        // start a new array if reference token is 0
                        result = &result->operator[](0);
                    }
                    else
                    {
                        // start a new object otherwise
                        result = &result->operator[](reference_token);
                    }
                    break;
                }

                case detail::value_t::object:
                {
                    // create an entry in the object
                    result = &result->operator[](reference_token);
                    break;
                }

                case detail::value_t::array:
                {
                    // create an entry in the array
                    JSON_TRY
                    {
                        result = &result->operator[](static_cast<size_type>(array_index(reference_token)));
                    }
                    JSON_CATCH(std::invalid_argument&)
                    {
                        JSON_THROW(detail::parse_error::create(109, 0, "array index '" + reference_token + "' is not a number"));
                    }
                    break;
                }

                /*
                The following code is only reached if there exists a reference
                token _and_ the current value is primitive. In this case, we have
                an error situation, because primitive values may only occur as
                single value; that is, with an empty list of reference tokens.
                */
                default:
                    JSON_THROW(detail::type_error::create(313, "invalid value to unflatten"));
            }
        }

        return *result;
    }

    /*!
    @brief return a reference to the pointed to value

    @note This version does not throw if a value is not present, but tries to
          create nested values instead. For instance, calling this function
          with pointer `"/this/that"` on a null value is equivalent to calling
          `operator[]("this").operator[]("that")` on that value, effectively
          changing the null value to an object.

    @param[in] ptr  a JSON value

    @return reference to the JSON value pointed to by the JSON pointer

    @complexity Linear in the length of the JSON pointer.

    @throw parse_error.106   if an array index begins with '0'
    @throw parse_error.109   if an array index was not a number
    @throw out_of_range.404  if the JSON pointer can not be resolved
    */
    BasicJsonType& get_unchecked(BasicJsonType* ptr) const
    {
        using size_type = typename BasicJsonType::size_type;
        for (const auto& reference_token : reference_tokens)
        {
            // convert null values to arrays or objects before continuing
            if (ptr->m_type == detail::value_t::null)
            {
                // check if reference token is a number
                const bool nums =
                    std::all_of(reference_token.begin(), reference_token.end(),
                                [](const char x)
                {
                    return (x >= '0' and x <= '9');
                });

                // change value to array for numbers or "-" or to object otherwise
                *ptr = (nums or reference_token == "-")
                       ? detail::value_t::array
                       : detail::value_t::object;
            }

            switch (ptr->m_type)
            {
                case detail::value_t::object:
                {
                    // use unchecked object access
                    ptr = &ptr->operator[](reference_token);
                    break;
                }

                case detail::value_t::array:
                {
                    // error condition (cf. RFC 6901, Sect. 4)
                    if (JSON_UNLIKELY(reference_token.size() > 1 and reference_token[0] == '0'))
                    {
                        JSON_THROW(detail::parse_error::create(106, 0,
                                                               "array index '" + reference_token +
                                                               "' must not begin with '0'"));
                    }

                    if (reference_token == "-")
                    {
                        // explicitly treat "-" as index beyond the end
                        ptr = &ptr->operator[](ptr->m_value.array->size());
                    }
                    else
                    {
                        // convert array index to number; unchecked access
                        JSON_TRY
                        {
                            ptr = &ptr->operator[](
                                static_cast<size_type>(array_index(reference_token)));
                        }
                        JSON_CATCH(std::invalid_argument&)
                        {
                            JSON_THROW(detail::parse_error::create(109, 0, "array index '" + reference_token + "' is not a number"));
                        }
                    }
                    break;
                }

                default:
                    JSON_THROW(detail::out_of_range::create(404, "unresolved reference token '" + reference_token + "'"));
            }
        }

        return *ptr;
    }

    /*!
    @throw parse_error.106   if an array index begins with '0'
    @throw parse_error.109   if an array index was not a number
    @throw out_of_range.402  if the array index '-' is used
    @throw out_of_range.404  if the JSON pointer can not be resolved
    */
    BasicJsonType& get_checked(BasicJsonType* ptr) const
    {
        using size_type = typename BasicJsonType::size_type;
        for (const auto& reference_token : reference_tokens)
        {
            switch (ptr->m_type)
            {
                case detail::value_t::object:
                {
                    // note: at performs range check
                    ptr = &ptr->at(reference_token);
                    break;
                }

                case detail::value_t::array:
                {
                    if (JSON_UNLIKELY(reference_token == "-"))
                    {
                        // "-" always fails the range check
                        JSON_THROW(detail::out_of_range::create(402,
                                                                "array index '-' (" + std::to_string(ptr->m_value.array->size()) +
                                                                ") is out of range"));
                    }

                    // error condition (cf. RFC 6901, Sect. 4)
                    if (JSON_UNLIKELY(reference_token.size() > 1 and reference_token[0] == '0'))
                    {
                        JSON_THROW(detail::parse_error::create(106, 0,
                                                               "array index '" + reference_token +
                                                               "' must not begin with '0'"));
                    }

                    // note: at performs range check
                    JSON_TRY
                    {
                        ptr = &ptr->at(static_cast<size_type>(array_index(reference_token)));
                    }
                    JSON_CATCH(std::invalid_argument&)
                    {
                        JSON_THROW(detail::parse_error::create(109, 0, "array index '" + reference_token + "' is not a number"));
                    }
                    break;
                }

                default:
                    JSON_THROW(detail::out_of_range::create(404, "unresolved reference token '" + reference_token + "'"));
            }
        }

        return *ptr;
    }

    /*!
    @brief return a const reference to the pointed to value

    @param[in] ptr  a JSON value

    @return const reference to the JSON value pointed to by the JSON
    pointer

    @throw parse_error.106   if an array index begins with '0'
    @throw parse_error.109   if an array index was not a number
    @throw out_of_range.402  if the array index '-' is used
    @throw out_of_range.404  if the JSON pointer can not be resolved
    */
    const BasicJsonType& get_unchecked(const BasicJsonType* ptr) const
    {
        using size_type = typename BasicJsonType::size_type;
        for (const auto& reference_token : reference_tokens)
        {
            switch (ptr->m_type)
            {
                case detail::value_t::object:
                {
                    // use unchecked object access
                    ptr = &ptr->operator[](reference_token);
                    break;
                }

                case detail::value_t::array:
                {
                    if (JSON_UNLIKELY(reference_token == "-"))
                    {
                        // "-" cannot be used for const access
                        JSON_THROW(detail::out_of_range::create(402,
                                                                "array index '-' (" + std::to_string(ptr->m_value.array->size()) +
                                                                ") is out of range"));
                    }

                    // error condition (cf. RFC 6901, Sect. 4)
                    if (JSON_UNLIKELY(reference_token.size() > 1 and reference_token[0] == '0'))
                    {
                        JSON_THROW(detail::parse_error::create(106, 0,
                                                               "array index '" + reference_token +
                                                               "' must not begin with '0'"));
                    }

                    // use unchecked array access
                    JSON_TRY
                    {
                        ptr = &ptr->operator[](
                            static_cast<size_type>(array_index(reference_token)));
                    }
                    JSON_CATCH(std::invalid_argument&)
                    {
                        JSON_THROW(detail::parse_error::create(109, 0, "array index '" + reference_token + "' is not a number"));
                    }
                    break;
                }

                default:
                    JSON_THROW(detail::out_of_range::create(404, "unresolved reference token '" + reference_token + "'"));
            }
        }

        return *ptr;
    }

    /*!
    @throw parse_error.106   if an array index begins with '0'
    @throw parse_error.109   if an array index was not a number
    @throw out_of_range.402  if the array index '-' is used
    @throw out_of_range.404  if the JSON pointer can not be resolved
    */
    const BasicJsonType& get_checked(const BasicJsonType* ptr) const
    {
        using size_type = typename BasicJsonType::size_type;
        for (const auto& reference_token : reference_tokens)
        {
            switch (ptr->m_type)
            {
                case detail::value_t::object:
                {
                    // note: at performs range check
                    ptr = &ptr->at(reference_token);
                    break;
                }

                case detail::value_t::array:
                {
                    if (JSON_UNLIKELY(reference_token == "-"))
                    {
                        // "-" always fails the range check
                        JSON_THROW(detail::out_of_range::create(402,
                                                                "array index '-' (" + std::to_string(ptr->m_value.array->size()) +
                                                                ") is out of range"));
                    }

                    // error condition (cf. RFC 6901, Sect. 4)
                    if (JSON_UNLIKELY(reference_token.size() > 1 and reference_token[0] == '0'))
                    {
                        JSON_THROW(detail::parse_error::create(106, 0,
                                                               "array index '" + reference_token +
                                                               "' must not begin with '0'"));
                    }

                    // note: at performs range check
                    JSON_TRY
                    {
                        ptr = &ptr->at(static_cast<size_type>(array_index(reference_token)));
                    }
                    JSON_CATCH(std::invalid_argument&)
                    {
                        JSON_THROW(detail::parse_error::create(109, 0, "array index '" + reference_token + "' is not a number"));
                    }
                    break;
                }

                default:
                    JSON_THROW(detail::out_of_range::create(404, "unresolved reference token '" + reference_token + "'"));
            }
        }

        return *ptr;
    }

    /*!
    @brief split the string input to reference tokens

    @note This function is only called by the json_pointer constructor.
          All exceptions below are documented there.

    @throw parse_error.107  if the pointer is not empty or begins with '/'
    @throw parse_error.108  if character '~' is not followed by '0' or '1'
    */
    static std::vector<std::string> split(const std::string& reference_string)
    {
        std::vector<std::string> result;

        // special case: empty reference string -> no reference tokens
        if (reference_string.empty())
        {
            return result;
        }

        // check if nonempty reference string begins with slash
        if (JSON_UNLIKELY(reference_string[0] != '/'))
        {
            JSON_THROW(detail::parse_error::create(107, 1,
                                                   "JSON pointer must be empty or begin with '/' - was: '" +
                                                   reference_string + "'"));
        }

        // extract the reference tokens:
        // - slash: position of the last read slash (or end of string)
        // - start: position after the previous slash
        for (
            // search for the first slash after the first character
            std::size_t slash = reference_string.find_first_of('/', 1),
            // set the beginning of the first reference token
            start = 1;
            // we can stop if start == string::npos+1 = 0
            start != 0;
            // set the beginning of the next reference token
            // (will eventually be 0 if slash == std::string::npos)
            start = slash + 1,
            // find next slash
            slash = reference_string.find_first_of('/', start))
        {
            // use the text between the beginning of the reference token
            // (start) and the last slash (slash).
            auto reference_token = reference_string.substr(start, slash - start);

            // check reference tokens are properly escaped
            for (std::size_t pos = reference_token.find_first_of('~');
                    pos != std::string::npos;
                    pos = reference_token.find_first_of('~', pos + 1))
            {
                assert(reference_token[pos] == '~');

                // ~ must be followed by 0 or 1
                if (JSON_UNLIKELY(pos == reference_token.size() - 1 or
                                  (reference_token[pos + 1] != '0' and
                                   reference_token[pos + 1] != '1')))
                {
                    JSON_THROW(detail::parse_error::create(108, 0, "escape character '~' must be followed with '0' or '1'"));
                }
            }

            // finally, store the reference token
            unescape(reference_token);
            result.push_back(reference_token);
        }

        return result;
    }

    /*!
    @brief replace all occurrences of a substring by another string

    @param[in,out] s  the string to manipulate; changed so that all
                   occurrences of @a f are replaced with @a t
    @param[in]     f  the substring to replace with @a t
    @param[in]     t  the string to replace @a f

    @pre The search string @a f must not be empty. **This precondition is
    enforced with an assertion.**

    @since version 2.0.0
    */
    static void replace_substring(std::string& s, const std::string& f,
                                  const std::string& t)
    {
        assert(not f.empty());
        for (auto pos = s.find(f);                // find first occurrence of f
                pos != std::string::npos;         // make sure f was found
                s.replace(pos, f.size(), t),      // replace with t, and
                pos = s.find(f, pos + t.size()))  // find next occurrence of f
        {}
    }

    /// escape "~"" to "~0" and "/" to "~1"
    static std::string escape(std::string s)
    {
        replace_substring(s, "~", "~0");
        replace_substring(s, "/", "~1");
        return s;
    }

    /// unescape "~1" to tilde and "~0" to slash (order is important!)
    static void unescape(std::string& s)
    {
        replace_substring(s, "~1", "/");
        replace_substring(s, "~0", "~");
    }

    /*!
    @param[in] reference_string  the reference string to the current value
    @param[in] value             the value to consider
    @param[in,out] result        the result object to insert values to

    @note Empty objects or arrays are flattened to `null`.
    */
    static void flatten(const std::string& reference_string,
                        const BasicJsonType& value,
                        BasicJsonType& result)
    {
        switch (value.m_type)
        {
            case detail::value_t::array:
            {
                if (value.m_value.array->empty())
                {
                    // flatten empty array as null
                    result[reference_string] = nullptr;
                }
                else
                {
                    // iterate array and use index as reference string
                    for (std::size_t i = 0; i < value.m_value.array->size(); ++i)
                    {
                        flatten(reference_string + "/" + std::to_string(i),
                                value.m_value.array->operator[](i), result);
                    }
                }
                break;
            }

            case detail::value_t::object:
            {
                if (value.m_value.object->empty())
                {
                    // flatten empty object as null
                    result[reference_string] = nullptr;
                }
                else
                {
                    // iterate object and use keys as reference string
                    for (const auto& element : *value.m_value.object)
                    {
                        flatten(reference_string + "/" + escape(element.first), element.second, result);
                    }
                }
                break;
            }

            default:
            {
                // add primitive value with its reference string
                result[reference_string] = value;
                break;
            }
        }
    }

    /*!
    @param[in] value  flattened JSON

    @return unflattened JSON

    @throw parse_error.109 if array index is not a number
    @throw type_error.314  if value is not an object
    @throw type_error.315  if object values are not primitive
    @throw type_error.313  if value cannot be unflattened
    */
    static BasicJsonType
    unflatten(const BasicJsonType& value)
    {
        if (JSON_UNLIKELY(not value.is_object()))
        {
            JSON_THROW(detail::type_error::create(314, "only objects can be unflattened"));
        }

        BasicJsonType result;

        // iterate the JSON object values
        for (const auto& element : *value.m_value.object)
        {
            if (JSON_UNLIKELY(not element.second.is_primitive()))
            {
                JSON_THROW(detail::type_error::create(315, "values in object must be primitive"));
            }

            // assign value to reference pointed to by JSON pointer; Note that if
            // the JSON pointer is "" (i.e., points to the whole value), function
            // get_and_create returns a reference to result itself. An assignment
            // will then create a primitive value.
            json_pointer(element.first).get_and_create(result) = element.second;
        }

        return result;
    }

    friend bool operator==(json_pointer const& lhs,
                           json_pointer const& rhs) noexcept
    {
        return (lhs.reference_tokens == rhs.reference_tokens);
    }

    friend bool operator!=(json_pointer const& lhs,
                           json_pointer const& rhs) noexcept
    {
        return not (lhs == rhs);
    }

    /// the reference tokens
    std::vector<std::string> reference_tokens;
};
}
