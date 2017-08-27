#pragma once
// Derived from: FastDelegate by Don Clugston, Mar 2004.

#include <string.h>
#include <utility>

namespace delly {

static_assert(sizeof((int*)nullptr) == sizeof(void(*)()),
              "Casting magic requires: sizeof(dataptr) < sizeof(codeptr)");

namespace details {

template <class OutputClass, class InputClass>
inline OutputClass horrible_cast(const InputClass input){

    union horrible_union{
        OutputClass out;
        InputClass in;
    } u;

    // Callable layout must be the same size in order to work properly
    static_assert(sizeof(u) == sizeof(InputClass)
                && sizeof(u) == sizeof(OutputClass));
    u.in = input;
    return u.out;
}

} // end details namespace

// Dummy class used for determining function pointer size
class DummyClass;
static const size_t SingleMemberFuncSize = sizeof(void (DummyClass::*)());

// Type-erased storage class for a delegate.  It can be cleared, but not directly
// invoked.  It must be converted to be properly invoked.
// All callables are stored as a pointer to an undefined class and an instance method.
//
// For function pointer storage, the function pointer is stored in m_this using
// horrible_cast. In this case the DelegateStorage implementation is simple:
// +--m_this--+-- p_func -+-- Meaning---------------------+
// |    0     |  0        | Empty                         |
// |  !=0     |  !=0*     | Static function or method call|
// +----------+-----------+-------------------------------+
struct DelegateStorage {
    using DummyMemFunc = void (DummyClass::*) ();

    DelegateStorage() = default;
    DelegateStorage(DummyClass* p, DummyMemFunc f)
        : m_this(p), m_func(f)
    {}

    inline DummyClass *getThis() const { return m_this; }
    inline DummyMemFunc getMemFunc() const { return m_func; }

    void reset() { m_this = nullptr; m_func = nullptr; }

    inline bool empty() const { return !m_this; }
    inline explicit operator bool() const { return !empty(); }
    inline bool operator! () const { return empty(); }

    inline bool operator==(const DelegateStorage& o) const {
        return m_this == o.m_this && m_func == o.m_func;
    }
    inline bool operator!=(const DelegateStorage& o) const {
        return !(this->operator==(o));
    }

    inline bool operator<(const DelegateStorage &o) const {
        return (m_this != o.m_this)
                ? (m_this < o.m_this)
                : (memcmp(&m_func, &o.m_func, sizeof(m_func)) < 0);
    }
    inline bool operator>(const DelegateStorage &o) const {
        return o.operator<(*this);
    }

private:
    DummyClass* m_this = nullptr;
    DummyMemFunc     m_func = nullptr;
};

template <typename Signature> class Delegate;

template <typename RetType, typename... Args>
class Delegate<RetType(Args...)> {

    using DummyMemFunc = RetType(DummyClass::*) (Args...);
public:
    using StaticFunc = RetType (*) (Args...);

    Delegate() = default;
    Delegate(const Delegate& o) = default;
    Delegate(Delegate&& o) = default;
    Delegate(const std::nullptr_t) noexcept : Delegate() {}

    // Non-const pointer and method
    template <class X, class Y>
    Delegate(Y* pthis, RetType (X::* func)(Args...))
        : m_storage(MakeStorage(static_cast<X*>(pthis), func))
    {}

    // Non-const pointer, const method
    template <class X, class Y>
    Delegate(Y* pthis, RetType (X::* func)(Args...) const)
        : m_storage(MakeStorage(static_cast<X*>(pthis), func))
    {}

    // Const pointer, const method
    template <class X, class Y>
    Delegate(const Y* pthis, RetType (X::* func)(Args...) const)
        : m_storage(MakeStorage(const_cast<X*>(pthis), func))
    {}

    // Non-const reference, non-const method
    template <class X, class Y>
    Delegate(Y& p, RetType (X::* func)(Args...))
        : m_storage(MakeStorage(static_cast<X*>(&p), func))
    {}

    // Non-const reference, const method
    template <class X, class Y>
    Delegate(Y& p, RetType (X::* func)(Args...) const)
        : m_storage(MakeStorage(static_cast<X*>(&p), func))
    {}

    // Static method or function
    Delegate(StaticFunc func)
        : m_storage(MakeStorage(&Delegate::InvokeStaticFunction, func))
    {}

    // Invoke the delegate
    template <typename ... Args2>
    RetType operator() (Args2&& ... args) const {
        DummyClass* obj = m_storage.getThis();
        DummyMemFunc func = getMemFunc();
        return (obj->*func)(std::forward<Args2>(args)...);
    }

    void reset() { m_storage.reset(); }

    inline bool empty() const { return m_storage.empty(); }
    inline explicit operator bool() const { return !empty(); }
    inline bool operator!() const { return empty(); }

    constexpr Delegate& operator=(Delegate& o) = default;
    Delegate& operator=(Delegate&& o) = default;
    Delegate& operator=(const Delegate& o) = default;

    bool operator==(const Delegate& o) const { return m_storage == o.m_storage; }
    bool operator!=(const Delegate& o) const { return m_storage != o.m_storage; }
    bool operator<(const Delegate& o) const { return m_storage < o.m_storage; }
    bool operator>(const Delegate& o) const { return m_storage > o.m_storage; }

    inline bool operator==(StaticFunc func) const { return (!func) ? empty() : (func == getStaticFunc()); }
    inline bool operator!=(StaticFunc func) const { return !operator==(func); }

    template <class X, class Y>
    inline void bind(Y* pthis, RetType (X::* func)(Args...)) {
        m_storage = MakeStorage(static_cast<X*>(pthis), func);
    }

    template <class X, class Y>
    inline void bind(const Y* pthis, RetType (X::* func)(Args...) const) {
        m_storage = MakeStorage(const_cast<X*>(pthis), func);
    }

    template <class X, class Y>
    inline void bind(Y& p, RetType (X::* func)(Args...)) {
        m_storage = MakeStorage(static_cast<X*>(&p), func);
    }

    template <class X, class Y>
    inline void bind(Y& p, RetType (X::* func)(Args...) const) {
        m_storage = MakeStorage(static_cast<X*>(&p), func);
    }

    inline void bind(StaticFunc func) {
        m_storage = MakeStorage(&Delegate::InvokeStaticFunction, func);
    }

    inline Delegate& operator=(StaticFunc func) {
        bind(func);
        return *this;
    }

private:
    // Store pointer to member
    template <class X, class XMemFunc>
    static DelegateStorage MakeStorage(X *pthis, XMemFunc func)
    {
        static_assert(sizeof(func) == SingleMemberFuncSize, "Unable to bind method, member size is invalid");
        auto* dObj = reinterpret_cast<DummyClass*>(pthis);
        auto dFunc = reinterpret_cast<DelegateStorage::DummyMemFunc>(func);
        return DelegateStorage(dObj, dFunc);
    }

    // Store static methods and functions
    template <class ParentInvokerSig>
    static DelegateStorage MakeStorage(ParentInvokerSig staticFuncInvoker, StaticFunc func)
    {
        /* 'Evil': store function pointer in 'this' pointer */
        auto f = (func == nullptr) ? nullptr : staticFuncInvoker;
        auto* dObj = details::horrible_cast<DummyClass*>(func);
        auto dFunc = reinterpret_cast<DelegateStorage::DummyMemFunc>(f);
        return DelegateStorage(dObj, dFunc);
    }

    inline DummyMemFunc getMemFunc() const {
        // Convert from storage to the signature of the delegate
        return reinterpret_cast<DummyMemFunc>(m_storage.getMemFunc());
    }

    inline StaticFunc getStaticFunc() const {
        // 'Evil' cast from this pointer back to a static function pointer
        return details::horrible_cast<StaticFunc>(this);
    }

    inline bool operator==(const DelegateStorage& o) const {
        return DelegateStorage::operator==(o);
    }

    RetType InvokeStaticFunction(Args ... args) const {
        auto* func = getStaticFunc();
        return (*func)(std::forward<Args>(args)...);
    }

    DelegateStorage m_storage;
};

template <class X, class Y, typename RetType, typename... Args>
Delegate<RetType(Args...)> MakeDelegate(Y* x, RetType (X::*func)(Args...)) {
    return Delegate<RetType(Args...)>(x, func);
}

template <class X, class Y, typename RetType, typename... Args>
Delegate<RetType(Args...)> MakeDelegate(Y* x, RetType (X::*func)(Args...) const) {
    return Delegate<RetType(Args...)>(x, func);
}

template <class X, class Y, typename RetType, typename... Args>
Delegate<RetType(Args...)> MakeDelegate(const Y* x, RetType (X::*func)(Args...) const) {
    return Delegate<RetType(Args...)>(x, func);
}

template <class X, class Y, typename RetType, typename... Args>
Delegate<RetType(Args...)> MakeDelegate(Y& x, RetType (X::*func)(Args...)) {
    return Delegate<RetType(Args...)>(x, func);
}

template <class X, class Y, typename RetType, typename... Args>
Delegate<RetType(Args...)> MakeDelegate(Y& x, RetType (X::*func)(Args...) const) {
    return Delegate<RetType(Args...)>(x, func);
}

// For lambda expressions, use a leading '+' operator, MakeDelegate(+[](...) { /*...*/ })
// Does not work for lambda instances, because type deduction fails
// Use bind instead:
//    auto lambda = [](...) { /* ... */ )
//    Delegate<void(...)> d;
//    d.bind(lambda)
template <typename RetType, typename... Args>
Delegate<RetType(Args...)> MakeDelegate(RetType (* func)(Args...)) {
    return Delegate<RetType(Args...)>(func);
}

} // end delly namespace
