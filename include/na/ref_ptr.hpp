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
#include <source_location>
#endif

#include <functional>
#include <mutex>
#include <string>

namespace na
{

using referable_after_free_handler = std::function<void(const std::string &)>;

namespace detail
{
inline std::mutex referable_after_free_handler_mutex;

inline referable_after_free_handler referable_after_free_handler_instance = [](const std::string &msg) {
    std::fputs(msg.c_str(), stderr);
    __debugbreak();
    std::terminate();
};

} // namespace detail

// Customization point for the referable after free handler
// By default a referable_after_free exception is thrown if exceptions are enabled or std::terminate() is called if
// exceptions are disabled.

/// @brief Set a custom referable after free handler
/// @param handler  New referable after free handler
inline void set_referable_after_free_handler(const referable_after_free_handler &handler)
{
    const auto lock = std::scoped_lock{detail::referable_after_free_handler_mutex};
    detail::referable_after_free_handler_instance = handler;
}

/// @brief Get referable after free handler
/// @return Current referable after free handler
inline referable_after_free_handler get_referable_after_free_handler()
{
    const auto lock = std::scoped_lock{detail::referable_after_free_handler_mutex};
    return detail::referable_after_free_handler_instance;
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
    ref_list_node(const std::source_location &loc)
    {
        location = loc;
    }

    ref_list_node *prev = nullptr;
    ref_list_node *next = nullptr;
    std::source_location location;
};

class ref_counter
{
  public:
    explicit ref_counter(size_t count, const std::source_location loc) : ref_count{count}, head(loc)
    {
    }

    void add_ref(ref_list_node *node) noexcept
    {
        std::scoped_lock lock{mutex};

        ++ref_count;
        node->next = head.next;
        node->prev = &head;
        head.next = node;
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

        node->prev = nullptr;
        node->next = nullptr;
    }

    std::size_t get_ref() const
    {
        return ref_count;
    }

    std::string get_referable_after_free_message() const
    {
        using namespace std::string_literals;

        std::string ret = "Referable after free detected.\n"
                          "The referable was destroyed while there were still references to it.\n"
                          "The number of references is " +
                          std::to_string(ref_count) +
                          ".\n"
                          "The referable destroyed:\n" +
                          "  " + head.location.file_name() + ":" + std::to_string(head.location.line()) + "\n" +
                          "Active references:" + "\n";

        ref_list_node *node = head.next;
        while (node != nullptr)
        {
            ret += "  "s + node->location.file_name() + ":" + std::to_string(node->location.line()) + "\n";
            node = node->next;
        }

        return ret;
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
    /// @brief Constructs a referable object by copying the value.
    /// @param val The value to copy into the referable object
    /// @param loc The source location of the referable object
    referable(const type &val
#ifdef na_ref_ptr_tracked
              ,
              const std::source_location &loc = std::source_location::current()
#endif
                  )
        :
#if defined(na_ref_ptr_counted)
          ref_count{0},
#elif defined(na_ref_ptr_tracked)
          ref_count(0, loc),
#endif
          value{val}
    {
    }

    /// @brief Constructs a referable object by moving the value.
    /// @param val The value to move into the referable object.
    /// @param loc The source location of the referable object.
    referable(type &&val
#ifdef na_ref_ptr_tracked
              ,
              const std::source_location &loc = std::source_location::current()
#endif
                  )
        :
#if defined(na_ref_ptr_counted)
          ref_count{0},
#elif defined(na_ref_ptr_tracked)
          ref_count(0, loc),
#endif
          value{std::move(val)}
    {
    }

    /// @brief Destroys the referable object, raises an error if there are any ref_ptr objects still referring the
    /// object.
    ~referable()
    {
#if defined(na_ref_ptr_counted)
        if (ref_count != 0)
        {
            get_referable_after_free_handler()("Referable after free detected");
        }
#elif defined(na_ref_ptr_tracked)
        if (ref_count.get_ref() != 0)
        {
            get_referable_after_free_handler()(ref_count.get_referable_after_free_message());
        }
#endif
    }

    /// @brief Constructs a referable object by copying the value from another referable object.
    /// @tparam other_type The value type of the other referable object
    /// @param other The other referable object
    template <typename other_type>
    referable(const referable<other_type> &other
#if defined(na_ref_ptr_tracked)
              ,
              const std::source_location &loc = std::source_location::current()
#endif
                  )
        :
#if defined(na_ref_ptr_counted)
          ref_count(0),
#elif defined(na_ref_ptr_tracked)
          ref_count(0, loc),
#endif
          value(other.value)
    {
    }

    /// @brief Constructs a referable object by moving the value from another referable object.
    /// @tparam other_type The value type of the other referable object
    /// @param other The other referable object
    template <typename other_type>
    referable(referable<other_type> &&other
#if defined(na_ref_ptr_tracked)
              ,
              const std::source_location &loc = std::source_location::current()
#endif
                  )
        :
#if defined(na_ref_ptr_counted)
          ref_count(0),
#elif defined(na_ref_ptr_tracked)
          ref_count(0, loc),
#endif
          value(std::move(other.value))
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
    enable_ref_from_this([[maybe_unused]] const enable_ref_from_this &other
#if defined(na_ref_ptr_tracked)
                         ,
                         const std::source_location &loc = std::source_location::current()
#endif
                             )
#if defined(na_ref_ptr_counted)
        : ref_count{0}
#elif defined(na_ref_ptr_tracked)
        : ref_count(0, loc)
#endif
    {
    }

    /// @brief Move constructor.
    /// @param other The other enable_ref_from_this object
    enable_ref_from_this([[maybe_unused]] enable_ref_from_this &&other
#if defined(na_ref_ptr_tracked)
                         ,
                         const std::source_location &loc = std::source_location::current()
#endif
                             )
#if defined(na_ref_ptr_counted)
        : ref_count{0}
#elif defined(na_ref_ptr_tracked)
        : ref_count(0, loc)
#endif
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
            get_referable_after_free_handler()("Referable after free detected");
        }
#elif defined(na_ref_ptr_tracked)
        if (ref_count.get_ref() != 0)
        {
            get_referable_after_free_handler()(ref_count.get_referable_after_free_message());
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
#if defined(na_ref_ptr_counted)
        : ref_count{0}
#elif defined(na_ref_ptr_tracked)
        : ref_count(0, std::source_location::current())
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
    ref_ptr(
#ifdef na_ref_ptr_tracked
        const std::source_location &loc = std::source_location::current()
#endif
            )
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{nullptr},
#endif
#if defined(na_ref_ptr_tracked)
          list_node{loc},
#endif
          value{nullptr}
    {
    }

    /// @brief Constructs a ref_ptr pointing to a referable<type> object.
    /// @tparam ref_type The value type of the referable object.
    /// @param ref The referable object.
    template <typename ref_type>
    ref_ptr(referable<ref_type> &ref
#ifdef na_ref_ptr_tracked
            ,
            const std::source_location &loc = std::source_location::current()
#endif
                )
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{&ref.ref_count},
#endif
#if defined(na_ref_ptr_tracked)
          list_node{loc},
#endif
          value{&ref.value}
    {
        add_ref();
    }

    /// @brief Constructs a ref_ptr pointing to a referable<type> object.
    /// @tparam ref_type The value type of the referable object.
    /// @param ref The referable object.
    template <typename ref_type>
    ref_ptr(const referable<ref_type> &ref
#ifdef na_ref_ptr_tracked
            ,
            const std::source_location &loc = std::source_location::current()
#endif
                )
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{&ref.ref_count},
#endif
#if defined(na_ref_ptr_tracked)
          list_node{loc},
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
    ref_ptr(referable<ref_type> &ref, value_type ref_type::*mem_var_ptr
#ifdef na_ref_ptr_tracked
            ,
            const std::source_location &loc = std::source_location::current()
#endif
                )
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{&ref.ref_count},
#endif
#if defined(na_ref_ptr_tracked)
          list_node{loc},
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
    ref_ptr(const referable<ref_type> &ref, value_type ref_type::*mem_var_ptr
#ifdef na_ref_ptr_tracked
            ,
            const std::source_location &loc = std::source_location::current()
#endif
                )
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{&ref.ref_count},
#endif
#if defined(na_ref_ptr_tracked)
          list_node{loc},
#endif
          value{&(ref.value.*mem_var_ptr)}
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
    ref_ptr(enable_ref_from_this<ref_type> &ref
#ifdef na_ref_ptr_tracked
            ,
            const std::source_location &loc = std::source_location::current()
#endif
                )
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{&ref.ref_count},
#endif
#if defined(na_ref_ptr_tracked)
          list_node{loc},
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
    ref_ptr(const enable_ref_from_this<ref_type> &ref
#ifdef na_ref_ptr_tracked
            ,
            const std::source_location &loc = std::source_location::current()
#endif
                )
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{&ref.ref_count},
#endif
#if defined(na_ref_ptr_tracked)
          list_node{loc},
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
    ref_ptr(enable_ref_from_this<ref_type> &ref, value_type ref_type::*mem_var_ptr
#ifdef na_ref_ptr_tracked
            ,
            const std::source_location &loc = std::source_location::current()
#endif
                )
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{&ref.ref_count},
#endif
#if defined(na_ref_ptr_tracked)
          list_node{loc},
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
    ref_ptr(const enable_ref_from_this<ref_type> &ref, value_type ref_type::*mem_var_ptr
#ifdef na_ref_ptr_tracked
            ,
            const std::source_location &loc = std::source_location::current()
#endif
                )
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{&ref.ref_count},
#endif
#if defined(na_ref_ptr_tracked)
          list_node{loc},
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
    ref_ptr(const ref_ptr<other_type> &other
#ifdef na_ref_ptr_tracked
            ,
            const std::source_location &loc = std::source_location::current()
#endif
                )
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{other.ref_count},
#endif
#if defined(na_ref_ptr_tracked)
          list_node{loc},
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

    /// @brief Constructs a ref_ptr to a sub object of another ref_ptr.
    /// @tparam other_type The value type of the tother ref_ptr.
    /// @tparam value_type The value type of the sub object.
    /// @param other The other ref_ptr object.
    /// @param mem_var_ptr The member pointer to the sub object.
    template <typename other_type, typename value_type>
    ref_ptr(const ref_ptr<other_type> &other, value_type other_type::*mem_var_ptr
#ifdef na_ref_ptr_tracked
            ,
            const std::source_location &loc = std::source_location::current()
#endif
                )
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{other.ref_count},
#endif
#if defined(na_ref_ptr_tracked)
          list_node{loc},
#endif
          value{&(other.value->*mem_var_ptr)}
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
    template <typename other_type>
    ref_ptr(ref_ptr<other_type> &&other
#ifdef na_ref_ptr_tracked
            ,
            const std::source_location &loc = std::source_location::current()
#endif
                )
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{other.ref_count},
#endif
#if defined(na_ref_ptr_tracked)
          list_node{loc},
#endif
          value{other.value}
    {
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
        other.ref_count = nullptr;
#endif
        other.value = nullptr;
    }

    /// @brief Constructs a ref_ptr to a sub object of another ref_ptr.
    /// @tparam other_type The value type of the tother ref_ptr.
    /// @tparam value_type The value type of the sub object.
    /// @param other The other ref_ptr object.
    /// @param mem_var_ptr The member pointer to the sub object.
    template <typename other_type, typename value_type>
    ref_ptr(ref_ptr<other_type> &&other, value_type other_type::*mem_var_ptr
#ifdef na_ref_ptr_tracked
            ,
            const std::source_location &loc = std::source_location::current()
#endif
                )
        :
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
          ref_count{other.ref_count},
#endif
#if defined(na_ref_ptr_tracked)
          list_node{loc},
#endif
          value{&(other.value->*mem_var_ptr)}
    {
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
        other.ref_count = nullptr;
#endif
        other.value = nullptr;
    }

    /// @brief Assigns the value from another ref_ptr.
    /// @param other The other ref_ptr object.
    /// @return A reference to this ref_ptr.
    ref_ptr &operator=(const ref_ptr &other)
    {
        if (this == &other)
        {
            return *this;
        }

#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
        if (this->ref_count != nullptr)
        {
            remove_ref();
        }

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
    template <typename other_type> ref_ptr &operator=(const ref_ptr<other_type> &other)
    {
        if (this == &other)
        {
            return *this;
        }

#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
        if (this->ref_count != nullptr)
        {
            remove_ref();
        }

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
    /// @param other The other ref_ptr object.
    /// @return A reference to this ref_ptr.
    ref_ptr &operator=(ref_ptr &&other)
    {
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
        if (this->ref_count != nullptr)
        {
            this->remove_ref();
        }

        this->ref_count = other.ref_count;

#if defined(na_ref_ptr_tracked)
        if (other.ref_count != nullptr)
        {
            other.remove_ref();
        }

        if (this->ref_count != nullptr)
        {
            this->add_ref();
        }
#endif

        other.ref_count = nullptr;
#endif

        this->value = other.value;
        other.value = nullptr;

        return *this;
    }

    /// @brief Assigns the value from another ref_ptr.
    /// @tparam other_type The value type of the other ref_ptr
    /// @param other The other ref_ptr object.
    /// @return A reference to this ref_ptr.
    template <typename other_type> ref_ptr &operator=(ref_ptr<other_type> &&other)
    {
#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
        if (this->ref_count != nullptr)
        {
            this->remove_ref();
        }

        this->ref_count = other.ref_count;

#if defined(na_ref_ptr_tracked)
        if (other.ref_count != nullptr)
        {
            other.remove_ref();
        }

        if (this->ref_count != nullptr)
        {
            this->add_ref();
        }
#endif

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

    template <typename other_type> friend class ref_ptr;

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
