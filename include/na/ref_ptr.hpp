// Copyright (c) 2023 Namal Bambarasinghe
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#ifndef NA_REF_PTR_HPP
#define NA_REF_PTR_HPP

#if !defined(na_ref_ptr_uncounted) && !defined(na_ref_ptr_counted) && !defined(na_ref_ptr_tracked)

// ref_ptr implementation is not defined, define the default below

#ifdef NDEBUB
#define na_ref_ptr_counted // defaults to counted in the release build
#else
#define na_ref_ptr_tracked // defaults to tracked in the debug build
#endif

#endif

#if defined(na_ref_ptr_counted)
#include <atomic>
#endif

#if defined(na_ref_ptr_tracked)
#include <mutex>
#endif

#if (__cpp_exceptions)
#include <exception>
#endif

#include <functional>
#include <string>

namespace na
{

#if (__cpp_exceptions)

/// @brief Referable after free exception is thrown if the referred object is destroyed with any ref_ptr objects still
/// referring the destroyed object.
struct referable_after_free_exception : std::exception
{
    referable_after_free_exception(const std::string &msg = "referable after free") : msg(msg)
    {
    }

    char const *what() const override
    {
        return msg.c_str();
    }

  private:
    std::string msg;
};

#endif

// Customization point for the referable after free handler
// By default a referable_after_free exception is thrown if exceptions are enabled or std::terminate() is called if
// exceptions are disabled.

/// @brief Set a custom referable after free handler
/// @param handler  New referable after free handler
void set_referable_after_free_handler(const std::function<void()> &handler);

/// @brief Get referable after free handler
/// @return Current referable after free handler
const std::function<void()> &get_referable_after_free_handler();

struct referable_after_free_handler
{
  private:
    friend void set_referable_after_free_handler(const std::function<void()> &handler)
    {
        referable_after_free_handler::handler = handler;
    }

    friend const std::function<void()> &get_referable_after_free_handler()
    {
        return referable_after_free_handler::handler;
    }

    inline static std::function<void()> handler = []() {
#if (__cpp_exceptions)
        throw referable_after_free_exception{};
#else
        std::terminate();
#endif // __cpp_exceptions
    };
};

namespace detail
{
#if defined(na_ref_ptr_uncounted)
namespace uncounted
#elif defined(na_ref_ptr_counted)
namespace counted
#else
namespace tracked
#endif
{

#if defined(na_ref_ptr_tracked)

struct ref_list_node
{
    ref_list_node *prev = nullptr;
    ref_list_node *next = nullptr;
};

class ref_counter
{
  public:
    explicit ref_counter(size_t count) : ref_count{count}
    {
    }

    void add_ref(ref_list_node *node) noexcept
    {
        std::scoped_lock lock{mutex};

        ++ref_count;
        node->next = head.next;
        node->prev = &head;
    }

    void remove_ref(ref_list_node *node) noexcept
    {
        std::scoped_lock lock{mutex};

        --ref_count;

        node->prev->next = node->next;

        if (node->next != nullptr)
        {
            node->next->prev = node->prev;
        }
    }

    std::size_t get_ref() const
    {
        return ref_count;
    }

  private:
    std::mutex mutex;
    std::size_t ref_count = 0;
    ref_list_node head;
};

#endif

template <typename type> class referable
{
  public:
    template <typename... arguments>
    referable(arguments &&...args)
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{0},
#endif
          value{std::forward<arguments>(args)...}
    {
    }

    ~referable()
    {
#if defined(na_ref_ptr_counted)
        if (ref_count != 0)
        {
            get_referable_after_free_handler()();
        }
#elif defined(na_ref_ptr_tracked)
        if (ref_count.get_ref() != 0)
        {
            get_referable_after_free_handler()();
        }
#endif
    }

    template <typename other_type> referable(const referable<other_type> &other) : ref_count(0), value(other.value)
    {
    }

    template <typename other_type>
    referable(referable<other_type> &&other) : ref_count(0), value(std::move(other.value))
    {
    }

    template <typename other_type> referable &operator=(const referable<other_type> &other)
    {
        value = other.value;
    }

    template <typename other_type> referable &operator=(referable<other_type> &&other)
    {
        value = std::move(other.value);
    }

    constexpr const type *operator->() const noexcept
    {
        return &value;
    }

    constexpr type *operator->() noexcept
    {
        return &value;
    }

    constexpr const type &operator*() const & noexcept
    {
        return value;
    }

    constexpr type &operator*() & noexcept
    {
        return value;
    }

    constexpr const type &&operator*() const && noexcept
    {
        return value;
    }

    constexpr type &&operator*() && noexcept
    {
        return value;
    }

  private:
    template <typename ref_type> friend class ref_ptr;

#if defined(na_ref_ptr_counted)
    mutable std::atomic_size_t ref_count;
#elif defined(na_ref_ptr_tracked)
    mutable ref_counter ref_count;
#endif // na_ref_ptr_counted

    type value;
};

template <class type> class enable_ref_from_this
{
  public:
    enable_ref_from_this(const enable_ref_from_this &) : ref_count{0}
    {
    }

    enable_ref_from_this(enable_ref_from_this &&) : ref_count{0}
    {
    }

    enable_ref_from_this &operator=(const enable_ref_from_this &)
    {
    }

    enable_ref_from_this &operator=(enable_ref_from_this &&)
    {
    }

    ~enable_ref_from_this()
    {
#if defined(na_ref_ptr_counted)
        if (ref_count != 0)
        {
            get_referable_after_free_handler()();
        }
#elif defined(na_ref_ptr_tracked)
        if (ref_count.get_ref() != 0)
        {
            get_referable_after_free_handler()();
        }
#endif
    }

    ref_ptr<type> ref_from_this()
    {
        return {*this};
    }

    ref_ptr<const type> ref_from_this() const
    {
        return {*this};
    }

  protected:
    enable_ref_from_this()
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
        : ref_count{0}
#endif
          {};

  private:
    template <typename ref_type> friend class ref_ptr;

#if defined(na_ref_ptr_counted)
    mutable std::atomic_size_t ref_count = 0;
#elif defined(na_ref_ptr_tracked)
    mutable ref_counter ref_count;
#endif // na_ref_ptr_counted or na_ref_ptr_tracked
};

template <typename type> class ref_ptr
{
  public:
    ref_ptr() : ref_count{nullptr}, value{nullptr}
    {
    }

    template <typename ref_type>
    ref_ptr(referable<ref_type> &ref)
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{&ref.ref_count},
#endif
          value{&ref.value}
    {
        add_ref();
    }

    template <typename ref_type>
    ref_ptr(const referable<ref_type> &ref)
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{&ref.ref_count},
#endif
          value{&ref.value}
    {
        add_ref();
    }

    template <typename ref_type, typename value_type>
    ref_ptr(referable<ref_type> &ref, value_type ref_type::*mem_var_ptr)
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{&ref.ref_count},
#endif
          value{&(ref.value.*mem_var_ptr)}
    {
        add_ref();
    }

    template <typename ref_type, typename value_type>
    ref_ptr(const referable<ref_type> &ref, value_type ref_type::*mem_var_ptr)
        : ref_count{&ref.ref_count}, value{&(ref.value.*mem_var_ptr)}
    {
        add_ref();
    }

    template <typename ref_type>
    ref_ptr(referable<ref_type> &&ref) = delete; // should not be allowed to construct from a temporary referable

    // constructors from enable_ref_from_this
    template <typename ref_type>
    ref_ptr(enable_ref_from_this<ref_type> &ref)
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{&ref.ref_count},
#endif
          value{&static_cast<ref_type &>(ref)}
    {
        static_assert(std::is_base_of_v<enable_ref_from_this<ref_type>, ref_type>);
        add_ref();
    }

    template <typename ref_type>
    ref_ptr(const enable_ref_from_this<ref_type> &ref)
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{&ref.ref_count},
#endif
          value{&static_cast<const ref_type &>(ref)}
    {
        static_assert(std::is_base_of_v<enable_ref_from_this<ref_type>, ref_type>);
        add_ref();
    }

    template <typename ref_type, typename value_type>
    ref_ptr(enable_ref_from_this<ref_type> &ref, value_type ref_type::*mem_var_ptr)
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{&ref.ref_count},
#endif
          value{&(ref.value->*mem_var_ptr)}
    {
        add_ref();
    }

    template <typename ref_type, typename value_type>
    ref_ptr(const enable_ref_from_this<ref_type> &ref, value_type ref_type::*mem_var_ptr)
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{&ref.ref_count},
#endif
          value{&(ref.value->*mem_var_ptr)}
    {
        add_ref();
    }

    template <typename ref_type>
    ref_ptr(enable_ref_from_this<ref_type> &&ref) =
        delete; // should not be allowed to construct from a temporary enable_ref_from_this

    template <typename other_type>
    ref_ptr(const ref_ptr<other_type> &other) : ref_count{other.ref_count}, value{other.value}
    {
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
        if (ref_count != nullptr)
        {
            add_ref();
        }
#endif
    }

    template <typename other_type> ref_ptr(ref_ptr<other_type> &&other) : ref_count{other.ref_count}, value{other.value}
    {
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
        other.ref_count = nullptr;
#endif
        other.value = nullptr;
    }

    template <typename other_type> ref_ptr &operator=(const ref_ptr<other_type> &other)
    {
        if (this == &other)
        {
            return *this;
        }

#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
        this->ref_count = other.ref_count;
#endif

        this->value = other.value;

        return *this;
    }

    template <typename other_type> ref_ptr &operator=(ref_ptr<other_type> &&other)
    {
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
        this->ref_count = other.ref_count;
        other.ref_count = nullptr;
#endif

        this->value = other.value;
        other.value = nullptr;

        return *this;
    }

    ~ref_ptr()
    {
        remove_ref();
    }

    void reset() noexcept
    {
        remove_ref();

#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
        ref_count = nullptr;
#endif
        value = nullptr;
    }

    operator bool() const noexcept
    {
        return value != nullptr;
    }

    constexpr const type *operator->() const noexcept
    {
        return value;
    }

    constexpr type *operator->() noexcept
    {
        return value;
    }

    constexpr const type &operator*() const & noexcept
    {
        return *value;
    }

    constexpr type &operator*() & noexcept
    {
        return *value;
    }

    constexpr const type &&operator*() const && noexcept
    {
        return *value;
    }

    constexpr type &&operator*() && noexcept
    {
        return *value;
    }

  private:
    void add_ref()
    {
#if defined(na_ref_ptr_counted)
        ++(*ref_count);
#elif defined(na_ref_ptr_tracked)
        ref_count->add_ref(&list_node);
#endif // na_ref_ptr_counted or na_ref_ptr_tracked
    }

    void remove_ref()
    {
#if defined(na_ref_ptr_counted)
        --(*ref_count);
#elif defined(na_ref_ptr_tracked)
        ref_count->remove_ref(&list_node);
#endif // na_ref_ptr_counted or na_ref_ptr_tracked
    }

#if defined(na_ref_ptr_counted)
    std::atomic_size_t *ref_count;
#elif defined(na_ref_ptr_tracked)
    ref_counter *ref_count;
    ref_list_node list_node;
#endif // na_ref_ptr_counted or na_ref_ptr_tracked

    type *value;
};

} // namespace uncounted or counted or tracked

} // namespace detail

#if defined(na_ref_ptr_uncounted)

template <typename type> using referable = detail::uncounted::referable<type>;
template <typename type> using enable_ref_from_this = detail::uncounted::enable_ref_from_this<type>;
template <typename type> using ref_ptr = detail::uncounted::ref_ptr<type>;

#elif defined(na_ref_ptr_counted)

template <typename type> using referable = detail::counted::referable<type>;
template <typename type> using enable_ref_from_this = detail::counted::enable_ref_from_this<type>;
template <typename type> using ref_ptr = detail::counted::ref_ptr<type>;

#elif defined(na_ref_ptr_tracked)

template <typename type> using referable = detail::tracked::referable<type>;
template <typename type> using enable_ref_from_this = detail::tracked::enable_ref_from_this<type>;
template <typename type> using ref_ptr = detail::tracked::ref_ptr<type>;

#endif

} // namespace na

#endif // NA_REF_PTR_HPP
