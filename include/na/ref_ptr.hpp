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

#if (__cpp_exceptions)
#include <exception>
#endif

#include <functional>
#include <mutex>
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

namespace detail
{
inline std::mutex referable_after_free_handler_mutex;

inline std::function<void()> referable_after_free_handler = []() {
#if (__cpp_exceptions)
    throw referable_after_free_exception{};
#else
    std::terminate();
#endif // __cpp_exceptions
};

} // namespace detail

// Customization point for the referable after free handler
// By default a referable_after_free exception is thrown if exceptions are enabled or std::terminate() is called if
// exceptions are disabled.

/// @brief Set a custom referable after free handler
/// @param handler  New referable after free handler
inline void set_referable_after_free_handler(const std::function<void()> &handler)
{
    const auto lock = std::scoped_lock{detail::referable_after_free_handler_mutex};
    detail::referable_after_free_handler = handler;
}

/// @brief Get referable after free handler
/// @return Current referable after free handler
inline std::function<void()> get_referable_after_free_handler()
{
    const auto lock = std::scoped_lock{detail::referable_after_free_handler_mutex};
    return detail::referable_after_free_handler;
}

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

/// @brief referable<type> type boxes a value so that safe references can be made to the contained value using
/// ref_ptr.
///
/// There are two advantages of using referable over raw references,
/// 1. The intention is clear and visible that the contained value is going to be referred to by other parts of the
/// program.
/// 2. All references are runtime checked to ensure that the referred object is not destroyed before the references
/// pointing at it.
///
/// @tparam type The contained value type
template <typename type> class referable
{
  public:
    /// @brief Constructs a referable object by direct initializing the value by forwarding arguments.
    /// @tparam ...arguments Argument types
    /// @param ...args Arguments for the value constructor
    template <typename... arguments>
    referable(arguments &&...args)
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{0},
#endif
          value{std::forward<arguments>(args)...}
    {
    }

    /// @brief Destroys the referable object, raises an error if there are any ref_ptr objects still referring the
    /// object.
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

    /// @brief Constructs a referable object by copying the value from another referable object.
    /// @tparam other_type The value type of the other referable object
    /// @param other The other referable object
    template <typename other_type> referable(const referable<other_type> &other) : ref_count(0), value(other.value)
    {
    }

    /// @brief Constructs a referable object by moving the value from another referable object.
    /// @tparam other_type The value type of the other referable object
    /// @param other The other referable object
    template <typename other_type>
    referable(referable<other_type> &&other) : ref_count(0), value(std::move(other.value))
    {
    }

    /// @brief Assigns the value from another referable object.
    /// @tparam other_type The value type of the other referable object
    /// @param other The other referable object
    /// @return A reference to this referable object
    template <typename other_type> referable &operator=(const referable<other_type> &other)
    {
        value = other.value;
        return *this;
    }

    /// @brief Assigns the value from another referable object by moving the value.
    /// @tparam other_type The value type of the other referable object
    /// @param other The other referable object
    /// @return A reference to this referable object
    template <typename other_type> referable &operator=(referable<other_type> &&other)
    {
        value = std::move(other.value);
        return *this;
    }

    /// @brief Accesses the value.
    /// @return A pointer to the value
    constexpr const type *operator->() const noexcept
    {
        return &value;
    }

    /// @brief Accesses the value.
    /// @return A pointer to the value
    constexpr type *operator->() noexcept
    {
        return &value;
    }

    /// @brief Accesses the value.
    /// @return A reference to the value
    constexpr const type &operator*() const & noexcept
    {
        return value;
    }

    /// @brief Accesses the value.
    /// @return A reference to the value
    constexpr type &operator*() & noexcept
    {
        return value;
    }

    /// @brief Accesses the value.
    /// @return A reference to the value
    constexpr const type &&operator*() const && noexcept
    {
        return value;
    }

    /// @brief Accesses the value.
    /// @return A reference to the value
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

/// @brief enable_ref_from_this<type> allow creating ref_ptr aware value types.
///
/// Publicly deriving from enable_ref_from_this<type> makes the value type a referable so that ref_ptr<type> can be
/// constructed by passing a reference to the value.
/// Moreover, from within the value type, ref_from_this() can be called to create a ref_ptr<type> to the value.
///
/// @tparam type The value type of the derived class
template <class type> class enable_ref_from_this
{
  public:
    /// @brief Copy constructor.
    /// @param other The other enable_ref_from_this object
    enable_ref_from_this([[maybe_unused]] const enable_ref_from_this &other) : ref_count{0}
    {
    }

    /// @brief Move constructor.
    /// @param other The other enable_ref_from_this object
    enable_ref_from_this([[maybe_unused]] enable_ref_from_this &&other) : ref_count{0}
    {
    }

    /// @brief Assignment operator.
    /// @param other The other enable_ref_from_this object
    enable_ref_from_this &operator=([[maybe_unused]] const enable_ref_from_this &)
    {
    }

    /// @brief Move assignment operator.
    /// @param other The other enable_ref_from_this object
    enable_ref_from_this &operator=([[maybe_unused]] enable_ref_from_this &&)
    {
    }

    /// @brief Destroys the enable_ref_from_this object, raises an error if there are any ref_ptr objects still
    /// referring the object.
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

    /// @brief Creates a ref_ptr<type> to the value.
    /// @return A ref_ptr<type> pointing the value.
    ref_ptr<type> ref_from_this()
    {
        return {*this};
    }

    /// @brief Creates a ref_ptr<type> to the value.
    /// @return A ref_ptr<type> pointing the value.
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

/// @brief ref_ptr<type> is a smart pointer that can be used to safely point to a value owned by another object.
///
/// A ref_ptr<type> can be constructed to point to an object boxed in a referable<type> object or an object of type that
/// is derived from enable_ref_from_this<type>.
///
/// Moreover, a ref_ptr<type> can also point to a sub object of such an object.
///
/// ref_ptr<type> has three different implementations.
/// 1. Counted implementation: ref_ptr<type> counts the number of references at runtime. This variation is the default
/// in the release mode.
/// 2. Tracked implementation: ref_ptr<type> keeps track of all the ref_ptrs alive so that referable after free does
/// list all the references for easy debugging. This variation is the default in the debug mode.
/// 3. Uncounted implementation: ref_ptr<type> does not count or keep track of references. This variation has zero
/// overhead compared to a raw pointer or reference. If the program can be validated to have correct RAII in the debug
/// mode then this implementation can be enabled in the release mode to achieve optimal performance. Recommended to be
/// used only if last bit of performance is important or the performance gain achieved by disabling the reference
/// counting can be justified.
///
/// Use following macros to select the implementation before including the header file:
/// 1. na_ref_ptr_counted: Counted implementation
/// 2. na_ref_ptr_tracked: Tracked implementation
/// 3. na_ref_ptr_uncounted: Uncounted implementation
///
/// @tparam type The type of the value pointed to by the ref_ptr.
template <typename type> class ref_ptr
{
  public:
    /// @brief Constructs an empty ref_ptr.
    ref_ptr() : ref_count{nullptr}, value{nullptr}
    {
    }

    /// @brief Constructs a ref_ptr pointing to a referable<type> object.
    /// @tparam ref_type The value type of the referable object.
    /// @param ref The referable object.
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

    /// @brief Constructs a ref_ptr pointing to a referable<type> object.
    /// @tparam ref_type The value type of the referable object.
    /// @param ref The referable object.
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

    /// @brief Constructs a ref_ptr pointing to a sub object of a referable<type> object.
    /// @tparam ref_type The value type of the referable object.
    /// @param ref The referable object.
    /// @param mem_var_ptr The member pointer to the sub object.
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

    /// @brief Constructs a ref_ptr pointing to a sub object of a referable<type> object.
    /// @tparam ref_type The value type of the referable object.
    /// @param ref The referable object.
    /// @param mem_var_ptr The member pointer to the sub object.
    template <typename ref_type, typename value_type>
    ref_ptr(const referable<ref_type> &ref, value_type ref_type::*mem_var_ptr)
        : ref_count{&ref.ref_count}, value{&(ref.value.*mem_var_ptr)}
    {
        add_ref();
    }

    /// @brief Deleted constructor from a temporary referable object.
    /// @tparam ref_type The value type of the referable object.
    /// @param ref The temporary referable object.
    template <typename ref_type> ref_ptr(referable<ref_type> &&ref) = delete;

    /// @brief Constructs a ref_ptr pointing to an object of type that is derived from enable_ref_from_this.
    /// @tparam ref_type The value type of enable_ref_from_this.
    /// @param ref The enable_ref_from_this object.
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

    /// @brief Constructs a ref_ptr pointing to an object of type that is derived from enable_ref_from_this.
    /// @tparam ref_type The value type of enable_ref_from_this.
    /// @param ref The enable_ref_from_this object.
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

    /// @brief Constructs a ref_ptr pointing to a sub object of type that is derived from enable_ref_from_this.
    /// @tparam ref_type The value type of enable_ref_from_this.
    /// @tparam value_type The value type of the sub object.
    /// @param ref The enable_ref_from_this object.
    /// @param mem_var_ptr The member pointer to the sub object.
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

    /// @brief Constructs a ref_ptr pointing to a sub object of type that is derived from enable_ref_from_this.
    /// @tparam ref_type The value type of enable_ref_from_this.
    /// @tparam value_type The value type of the sub object.
    /// @param ref The enable_ref_from_this object.
    /// @param mem_var_ptr The member pointer to the sub object.
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

    /// @brief Deleted constructor from a temporary enable_ref_from_this object.
    /// @tparam ref_type The value type of enable_ref_from_this.
    /// @param ref the temporary enable_ref_from_this object.
    template <typename ref_type> ref_ptr(enable_ref_from_this<ref_type> &&ref) = delete;

    /// @brief Constructs a ref_ptr from another ref_ptr.
    /// @tparam other_type The value type of the tother ref_ptr.
    /// @param other The other ref_ptr object.
    template <typename other_type>
    ref_ptr(const ref_ptr<other_type> &other)
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{other.ref_count},
#endif
          value{other.value}
    {
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
        if (ref_count != nullptr)
        {
            add_ref();
        }
#endif
    }

    /// @brief Constructs a ref_ptr from another ref_ptr.
    /// @tparam other_type The value type of the tother ref_ptr.
    /// @param other The other ref_ptr object.
    template <typename other_type> ref_ptr(ref_ptr<other_type> &&other) : ref_count{other.ref_count}, value{other.value}
    {
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
        other.ref_count = nullptr;
#endif
        other.value = nullptr;
    }

    /// @brief Assigns the value from another ref_ptr.
    /// @tparam other_type The value type of the other ref_ptr
    /// @param other The other ref_ptr object.
    /// @return A reference to this ref_ptr.
    template <typename other_type> ref_ptr &operator=(const ref_ptr<other_type> &other)
    {
        if (this == &other)
        {
            return *this;
        }

#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
        this->ref_count = other.ref_count;
        if (ref_count != nullptr)
        {
            add_ref();
        }
#endif

        this->value = other.value;

        return *this;
    }

    /// @brief Assigns the value from another ref_ptr.
    /// @tparam other_type The value type of the other ref_ptr
    /// @param other The other ref_ptr object.
    /// @return A reference to this ref_ptr.
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

    /// @brief Remove this reference to the object and destroys the ref_ptr.
    ~ref_ptr()
    {
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
        if (ref_count != nullptr)
        {
            remove_ref();
        }
#endif
    }

    /// @brief Remove this reference to the pointed object.
    void reset() noexcept
    {
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
        if (ref_count != nullptr)
        {
            remove_ref();
            ref_count = nullptr;
        }
#endif
        value = nullptr;
    }

    /// @brief Tests whether the ref_ptr is pointing to a valid object.
    operator bool() const noexcept
    {
        return value != nullptr;
    }

    /// @brief Accesses the value pointed by the ref_ptr.
    /// @return A pointer to the value pointed by the ref_ptr.
    constexpr const type *operator->() const noexcept
    {
        return value;
    }

    /// @brief Accesses the value pointed by the ref_ptr.
    /// @return A pointer to the value pointed by the ref_ptr.
    constexpr type *operator->() noexcept
    {
        return value;
    }

    /// @brief Accesses the value pointed by the ref_ptr.
    /// @return A reference to the value pointed by the ref_ptr.
    constexpr const type &operator*() const & noexcept
    {
        return *value;
    }

    /// @brief Accesses the value pointed by the ref_ptr.
    /// @return A reference to the value pointed by the ref_ptr.
    constexpr type &operator*() & noexcept
    {
        return *value;
    }

    /// @brief Accesses the value pointed by the ref_ptr.
    /// @return A reference to the value pointed by the ref_ptr.
    constexpr const type &&operator*() const && noexcept
    {
        return *value;
    }

    /// @brief Accesses the value pointed by the ref_ptr.
    /// @return A reference to the value pointed by the ref_ptr.
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
