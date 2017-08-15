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

// Dummy class used for determining function pointer size
class DummyClass;
static const int SingleMemberFuncSize = sizeof(void (DummyClass::*)());

} // end details namespace

// Type-erased storage class for a delegate.  It can be cleared, but not directly
// invoked.  It must be converted to be properly invoked.
//
// For function pointer storage, the function pointer is stored in m_this using
// horrible_cast. In this case the DelegateStorage implementation is simple:
// +--m_this--+-- p_func -+-- Meaning---------------------+
// |    0     |  0        | Empty                         |
// |  !=0     |  !=0*     | Static function or method call|
// +----------+-----------+-------------------------------+
struct DelegateStorage {
    using DummyMemFuncType = void (details::DummyClass::*) (); // member func

    DelegateStorage() = default;
    DelegateStorage(details::DummyClass* p, DummyMemFuncType f)
        : m_this(p), m_func(f)
    {}

    inline details::DummyClass *getThis() const { return m_this; }
    inline DummyMemFuncType getMemFunc() const {
        return reinterpret_cast<DummyMemFuncType>(m_func);
    }

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

    inline bool operator <(const DelegateStorage &o) const {
        return (m_this != o.m_this)
                ? (m_this < o.m_this)
                : (memcmp(&m_func, &o.m_func, sizeof(m_func)) < 0);
    }
    inline bool operator >(const DelegateStorage &o) const {
        return o.operator<(*this);
    }

private:
    details::DummyClass* m_this = nullptr;
    DummyMemFuncType     m_func = nullptr;
};

namespace details {

// ClosurePtr<> private wrapper that adds function signature to DelegateStorage

template <class DummyMemFunc, class StaticFuncPtr>
class Closure : public DelegateStorage {
public:
    // Set delegate to a member function

    Closure() = default;

    // Convert member function into a standard form
    template <class X, class XMemFunc>
    Closure(X *pthis, XMemFunc func)
        : DelegateStorage(bindMemFunc(pthis, func))
    {}

    template <class DerivedClass, class ParentInvokerSig>
    Closure(DerivedClass *pParent, ParentInvokerSig staticFuncInvoker,
            StaticFuncPtr func)
        : DelegateStorage(bindStaticFunction(pParent, staticFuncInvoker, func))
    {}

    inline DummyMemFunc getMemFunc() const {
        return reinterpret_cast<DummyMemFunc>(DelegateStorage::getMemFunc());
    }

    inline StaticFuncPtr getStaticFunc() const {
        // 'Evil' cast from this pointer back to a static function pointer
        return horrible_cast<StaticFuncPtr>(this);
    }

    inline bool operator==(const DelegateStorage& o) const {
        return DelegateStorage::operator==(o);
    }

    inline bool operator==(StaticFuncPtr func) {
        // If not storing a static function, no valid instance pointer
        // will be equal to the function poitner
        return (!func) ? empty()
                : func == getStaticFunc();
    }

private:
    template <class X, class XMemFunc>
    inline static DelegateStorage bindMemFunc(X *pthis, XMemFunc func) {
        static_assert(sizeof(func) == SingleMemberFuncSize,
                      "Unable to bind method, member size is invalid");
        return DelegateStorage(reinterpret_cast<DummyClass*>(pthis),
                  reinterpret_cast<DummyMemFuncType>(func));
    }

    template <class DerivedClass, class ParentInvokerSig>
    inline static DelegateStorage bindStaticFunction(DerivedClass *pParent,
            ParentInvokerSig staticFuncInvoker,
            StaticFuncPtr func)
    {
        // 'Evil': store function pointer in 'this' pointer
        return DelegateStorage(horrible_cast<DummyClass*>(func),
                               reinterpret_cast<DummyMemFuncType>((func == nullptr) ? nullptr : staticFuncInvoker));
    }
};

} // end namespace detail

template <typename Signature> class Delegate;

template <typename RetType, typename... Args>
class Delegate<RetType(Args...)> {

    using StaticFuncType = RetType (*) (Args...);
    using DummyMemFuncType = void (details::DummyClass::*) (Args...);
    using ClosureType = details::Closure<DummyMemFuncType, StaticFuncType>;
public:

    Delegate() = default;
    Delegate(const Delegate& o) = default;
    Delegate(Delegate&& o) = default;
    Delegate(const std::nullptr_t) noexcept : Delegate() {}

    template <class X, class Y>
    Delegate(Y* pthis, RetType (X::* func)(Args...))
        : m_closure(static_cast<X*>(pthis), func)
    {}

    template <class X, class Y>
    Delegate(Y* pthis, RetType (X::* func)(Args...) const)
        : m_closure(static_cast<X*>(pthis), func)
    {}

    template <class X, class Y>
    Delegate(const Y* pthis, RetType (X::* func)(Args...) const)
        : m_closure(const_cast<X*>(pthis), func)
    {}

    template <class X, class Y>
    Delegate(Y& p, RetType (X::* func)(Args...))
        : m_closure(static_cast<X&>(p), func)
    {}

    template <class X, class Y>
    Delegate(Y& p, RetType (X::* func)(Args...) const)
        : m_closure(static_cast<X&>(p), func)
    {}

    template <class X, class Y>
    Delegate(const Y& p, RetType (X::* func)(Args...) const)
        : m_closure(const_cast<X&>(p), func)
    {}

    Delegate(StaticFuncType func)
        : m_closure(this, &Delegate::InvokeStaticFunction, func)
    {}

    // Invoke the delegate when not void
    template <typename ... Args2>
    RetType operator() (Args2 ... args) const {
        details::DummyClass* obj = m_closure.getThis();
        DummyMemFuncType func     = m_closure.getMemFunc();
        return (obj->*func)(args...);
    }

    void clear() { m_closure.clear(); }

    inline bool empty() const { return m_closure.empty(); }
    inline explicit operator bool() const { return !empty(); }
    inline bool operator! () const { return empty(); }

    constexpr Delegate& operator= (Delegate& o) = default;
    Delegate& operator = (Delegate&& o) = default;
    Delegate& operator = (const Delegate& o) = default;

    bool operator== (const Delegate& o) const { return m_closure == o.m_closure; }
    bool operator!= (const Delegate& o) const { return m_closure != o.m_closure; }
    bool operator< (const Delegate& o) const { return m_closure < o.m_closure; }
    bool operator> (const Delegate& o) const { return m_closure > o.m_closure; }

    inline bool operator==(StaticFuncType func) {
        return m_closure == func; }
    inline bool operator!=(StaticFuncType func) {
        return !operator==(func); }

    template <class X, class Y>
    inline void bind(Y* pthis, RetType (X::* func)(Args...)) {
        m_closure = { static_cast<X*>(pthis), func };
    }

    template <class X, class Y>
    inline void bind(Y* pthis, RetType (X::* func)(Args...) const) {
        m_closure = { static_cast<X*>(pthis), func };
    }

    template <class X, class Y>
    inline void bind(const Y* pthis, RetType (X::* func)(Args...) const) {
        m_closure = { const_cast<X*>(pthis), func };
    }

    template <class X, class Y>
    inline void bind(Y& p, RetType (X::* func)(Args...)) {
        m_closure = { static_cast<X&>(p), func };
    }

    template <class X, class Y>
    inline void bind(Y& p, RetType (X::* func)(Args...) const) {
        m_closure = { static_cast<X&>(p), func };
    }

    template <class X, class Y>
    inline void bind(const Y& p, RetType (X::* func)(Args...) const) {
        m_closure = { static_cast<Y&>(p), func };
    }

    inline void bind(StaticFuncType func) {
        m_closure = { this, &Delegate::InvokeStaticFunction, func };
    }

    inline Delegate& operator = (StaticFuncType func) {
        bind(func);
        return *this;
    }

private:
    RetType InvokeStaticFunction(Args ... args) const {
        auto* func = m_closure.getStaticFunc();
        return (*func)(args...);
    }

    ClosureType m_closure;
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

template <typename RetType, typename... Args>
Delegate<RetType(Args...)> MakeDelegate(RetType (* func)(Args...)) {
    return Delegate<RetType(Args...)>(func);
}

} // end delly namespace
