# na::ref_ptr\<type\> A C++ smart pointer for creating safe references.

na::ref_ptr\<type\> is a C++ smart pointer that allows creating safe references to a non-owned object.

The referred object must be boxed using na::referable\<type\> or must be derived from na::enabled_ref_from_this\<type\>.

The value stored inside a na::referable can be unboxed using operator* or operator->.

Any number of safe references (na::ref_ptr\<type\>) can be created to refer to the referred object.

na::ref_ptr\<type\> can also be created to refer to sub objects of the referred object using pointers to member variables.

New 

```cpp
// primitive type boxed in a referable
na::referable<int> r = {1};
int r1 = *r;
na::ref_ptr<int> rp = r;
int r2 = *rp;
assert(r1 == r2);

// user defined type boxed in a referable
struct test
{
    int a;
    float b;
};

na::referable<test> t = {2, 5.0f};

// dereferencing the referable
int ta = t->a;
assert(ta == 2);

// dereferencing the ref_ptr
na::ref_ptr<test> tp = t;
int tb = tp->b;
assert(tb == 5.0f);

// ref_ptr to a sub object
na::ref_ptr<int> tp_a = {t, &test::a};
assert(*tp_a == 2);

na::ref_ptr<float> tp_b = {tp, &test::b};
assert(*tp_b == 5.0f);

// allow safe references using enable_ref_from_this
struct safely_referable_type : enable_ref_from_this<safely_referable_type>
{
    double d;
    std::string s;
};

na::ref_ptr<safely_referable_type> p = {3.0, "Hello"};

double d = p->d;
assert(d == 3.0);

std::string s = p->s;
assert(s == "Hello");
```

This implementation guarantees that the referred object will not be destroyed while any of the the ref_ptr s pointing to that referable object is alive.

If the referred object is destroyed before all the ref_ptr s are destroyed, an error is raised by either throwing an exception (if exceptions are enabled) or by calling std::terminate(). The default referable after free handler can be customized using na::set_referable_after_free_handler().

This is useful in situations where you want to store a reference to a value object in an unrelated object. **Specially in a situation where static analysis cannot prove that use after free will not occur**.

This forces you to re-think about RAII and fix object lifetimes of your program.

# When to use na::ref_ptr\<type\> #

ref_ptr is expected to be used when a reference to a non-owned object needs to be captured.

Use std::unique_ptr and std::shared_ptr s when ownership can be assumed.

Common use cases,
* Registering callbacks - Make the called back object a referable and store a ref_ptr inside the delegate.
* Storing a reference to an external service - Make the service a referable and store a ref_ptr inside the class that uses it.

# Licence #
MIT

