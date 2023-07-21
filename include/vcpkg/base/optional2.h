#pragma once

#include <type_traits>

namespace vcpkg
{
    struct NullOpt2
    {
        explicit constexpr NullOpt2(int) { }
    };

    const static constexpr NullOpt2 nullopt2{0};

    struct OptionalDummyType
    {
        constexpr OptionalDummyType() noexcept { }
    };

    template<typename T, bool = std::is_trivially_destructible_v<T>>
    struct OptionalDestructorBase
    {
        union
        {
            OptionalDummyType m_dummy;
            std::remove_cv_t<T> m_value;
        };
        bool m_has_value;

        constexpr OptionalDestructorBase() noexcept : m_dummy(), m_has_value(false) { }
        template<class... Args>
        explicit constexpr OptionalDestructorBase(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
            : m_value(std::forward<Args&&...>(args...)), m_has_value(true)
        {
        }

        void reset() noexcept { m_has_value = false; }
    };

    template<typename T>
    struct OptionalDestructorBase<T, false>
    {
        union
        {
            OptionalDummyType m_dummy;
            std::remove_cv_t<T> m_value;
        };
        bool m_has_value;

        constexpr OptionalDestructorBase() noexcept : m_dummy(), m_has_value(false) { }
        template<class... Args>
        explicit constexpr OptionalDestructorBase(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
            : m_value(std::forward<Args&&...>(args...)), m_has_value(true)
        {
        }

        void reset() noexcept
        {
            if (!m_has_value) return;
            m_value.~T();
            m_has_value = false;
        }

        ~OptionalDestructorBase() noexcept
        {
            if (m_has_value)
            {
                m_value.~T();
            }
        }

        constexpr OptionalDestructorBase(const OptionalDestructorBase&) noexcept = default;
        constexpr OptionalDestructorBase& operator=(const OptionalDestructorBase&) noexcept = default;
        constexpr OptionalDestructorBase(OptionalDestructorBase&&) noexcept = default;
        constexpr OptionalDestructorBase& operator=(OptionalDestructorBase&&) noexcept = default;
    };

    template<typename T>
    struct Optional2 : private OptionalDestructorBase<T>
    {
        using value_type = T;
        constexpr Optional2() noexcept = default;
        constexpr Optional2(NullOpt2) noexcept { }
        
        template<class... Args, std::enable_if_t<std::is_constructible_v<T, Args...>, int> = 0>
            explicit constexpr Optional2(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
            : OptionalDestructorBase<T>(std::forward<Args>(args)...)
        {
        }

        template<class Arg>
        constexpr Optional2(Arg&& rhs) noexcept(
            std::is_nothrow_constructible_v<T, Arg>)
            : OptionalDestructorBase<T>(std::forward<Arg>(rhs))
        {
        }

        constexpr value_type& operator*() & noexcept { return this->m_value; }
        constexpr const value_type& operator*() const& noexcept { return this->m_value; }
        constexpr value_type&& operator*() && noexcept { return std::move(this->m_value); }
        constexpr const value_type&& operator*() const&& noexcept { return std::move(this->m_value); }

        explicit constexpr operator bool() const noexcept { return this->m_has_value; }
        constexpr bool has_value() const noexcept { return this->m_has_value; }

        constexpr value_type& value() & noexcept { return this->m_value; }
        constexpr const value_type& value() const& noexcept { return this->m_value; }
        constexpr value_type&& value() && noexcept { return std::move(this->m_value); }
        constexpr const value_type&& value() const&& noexcept { return std::move(this->m_value); }

        constexpr value_type& get() & noexcept { return this->m_value; }
        constexpr const value_type& get() const& noexcept { return this->m_value; }
        constexpr value_type&& get() && noexcept { return std::move(this->m_value); }
        constexpr const value_type&& get() const&& noexcept { return std::move(this->m_value); }

        template<class... Args>
        constexpr value_type& emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
        {
            this->reset();
            this->m_value(std::forward<Args>(args)...);
            this->m_has_value = true;
            return this->m_value;
        }
    };

    template<class U>
    Optional2<std::remove_cv_t<U>> make_optional2(U&& u)
    {
        return Optional2<std::remove_cv_t<U>>(std::forward<U>(u));
    }
}
