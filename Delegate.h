#pragma once
// Derived from: FastDelegate by Don Clugston, Mar 2004.

#include <cstring>
#include <utility>

#include <cassert>

#define ITANIUM_DELEGATE_SPACE_SAVER 1

#if defined(_DEBUG) || !defined(NDEBUG)
#define DEBUG_ASSERT(x) assert((x))
#else
#define DEBUG_ASSERT(x)
#endif

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

////////////////////////////////////////////////////////////////////////////////
//
// Delegate optimization and sizing across architectures and compilers
//
// Delegate binding involves adjusting the member and pointer offsets at
// bind time in order to keep the bound object/method small
// and make invocation consistent and fast.
//
// Tested sizeof method pointer (ie sizeof(void(X::*)()))
// using Compiler Explorer (godbolt.org).
//
//     struct A {};
//     struct B : virtual A {};
//     struct C {};
//     struct D : A, C {};
//     struct E;
//
// | Arch, Compiler, Flags          | A | B | C | D | E |
// |--------------------------------|---|---|---|---|---|
// | x86-64 gcc (1)                 |16 |16 |16 |16 |16 |
// | x86_64 clang (2)               |16 |16 |16 |16 |16 |
// | x86_64 ICC (3)                 |16 |16 |16 |16 |16 |
// | ARM gcc (4)                    |8  |8  |8  |8  |8  |
// | ARM64 gcc                      |16 |16 |16 |16 |16 |
// | ARM CL 2017 RTW                |4  |12 |4  |8  |16 |
// | x86 CL 19 2017 RTW             |4  |12 |4  |8  |16 |
// | x86-64 CL 19 2017 RTW /vmb     |8  |16 |8  |16 |24 |
// | x86-64 CL 19 2017 RTW /vmg     |24 |24 |24 |24 |24 |
//
// (1) tested GCC 4.4.x through 7.2
// (2) tested Clang 3.0 through 4.0
// (3) tested ICC 13.0, 16, 17
// (4) GCC 4.5.4, 4.6.4, 5.4, 6.3.0
//
// --------------------
// All versions of GCC, Clang, Intel follow the above structure for
//     all architectures (x86_64, ARM/ARM64, MIPS/MIPS64,
//      PowerPC/PowerPC 64, etc.).
// Clang and GCC use the Itanium ABI which stores member function pointers
// as this (http://refspecs.linuxbase.org/cxxabi-1.83.html#member-pointers):
//
//      ptrdiff_t ptr: either a function pointer or an offset into the vtable
//      ptrduff_t adj: required adjustement of this
//
// --------------------
// Microsoft Representation Model
//     See: https://msdn.microsoft.com/en-us/library/bkb78zf3.aspx
//         /vm[bgsmv] dictate the representation model for members to pointers:
//         /vmb "best-case", no forward declarations of pointer to member
//         /vmg "general-case", forward declarations of pointer to member
//         /vms - single inheritance
//         /vmm - multiple inheritance
//         /vmv - virtual inheritance
//
//     Single Inheritance: ptrdiff_t member pointer
//     Multiple Inheritance: ptrdiff_t member pointer, int byte adjustment
//     Virtual Inheritance: ptrdiff_t member pointer, int byte adjustment, int vtable_index
//     Unknown Inheritance: ptrdiff_t member pointer, int byte adjustment, int vtordisp, int vtable_index
//
// For CL optimizations, see:
//     https://msdn.microsoft.com/en-us/library/aa299371(v=vs.60).aspx
//

// Dummy class used for determining minimal function pointer size
#if defined(_MSC_VER)
// Depending on inheritance and compiler flags, member functions could be quite large
// See: https://msdn.microsoft.com/en-us/library/aa299371(v=vs.60).aspx
class __single_inheritance DummyClass;
#else
class DummyClass;
#endif

static const size_t SingleMemberFuncSize = sizeof(void (DummyClass::*)());
using StaticFunc = void (*) ();
using DummyMemFunc = void (DummyClass::*) ();

// Adjusts the this pointer as appropriate for the bound method
template <size_t MethodSize>
struct BindHelper;

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning( disable : 4820 )

#pragma pack(push, 1)
struct MicrosoftMultpleInheritanceRepr {
    DummyMemFunc func; // member function
    int delta; // byte adjustment to this
};
struct MicrosoftVirtualInheritanceRepr {
    DummyMemFunc func; // member function
    int delta; // byte adjustment to this
    int vtable_index; // vtable index or 0
};
struct MicrosoftUnknownInheritanceRepr {
    DummyMemFunc func; // member function
    int delta; // byte adjustment to this
    int vtordisp; // byte adjustment to find vtable
    int vtable_index; // vtable index or 0
};
#pragma pack(pop)

// A single inheritance member function should have no class or vtable adjustments
static_assert(sizeof(StaticFunc) == SingleMemberFuncSize);

// Single inheritance binding is a straight function pointer
template <>
struct BindHelper<SingleMemberFuncSize>
{
    // This handles all cases of GCC, Clang, Intel
    // And single inheritance model of Microsoft
    template <class X, class XFuncType>
    static std::pair<DummyClass*, DummyMemFunc> Convert(X* pthis, XFuncType func) {
        return { reinterpret_cast<DummyClass*>(pthis),
                 reinterpret_cast<DummyMemFunc>(func) };
    }
};

// Multiple inheritance binding
template <>
struct BindHelper<SingleMemberFuncSize + sizeof(int)>
{
    // Handles multiple inheritance model of Microsoft
    template <class X, class XFuncType>
    static std::pair<DummyClass*, DummyMemFunc> Convert(X* pthis, XFuncType func) {
        union {
            XFuncType func;
            MicrosoftMultpleInheritanceRepr repr;
        } u;
        u.func = func;
        return {
            reinterpret_cast<DummyClass*>(reinterpret_cast<char*>(pthis) + u.repr.delta),
            reinterpret_cast<DummyMemFunc>(u.repr.func)
        };
    }
};

// Dummy class that uses virtual inheritance to retrieve the 'this' pointer
// We do this by patching the GetThis onto a bound type to retrieve the adjusted 'this'
// Derived by Don Clugston and John M. Dlugosz
// XXX defined DummyClass2 due to CL error
class DummyClass2 {};
struct DummyVirtualClass : virtual public DummyClass2
{
    using ProbePtrType = DummyVirtualClass* (DummyVirtualClass::*)();
    DummyVirtualClass* GetThis() { return this; }
};

// Virtual inheritance
template <>
struct BindHelper<SingleMemberFuncSize + 2*sizeof(int)>
{
    // Handles multiple inheritance model of Microsoft
    template <class X, class XFuncType>
    static std::pair<DummyClass*, DummyMemFunc> Convert(X* pthis, XFuncType func) {

        // Union used to retrieve function pointer
        union {
            XFuncType func;
            DummyClass* (X::* probeFunc)(); // member pointer of X returning DummyClass
            MicrosoftVirtualInheritanceRepr repr;
        } u;
        static_assert(sizeof(func) == sizeof(u.repr)
                      && sizeof(func) == sizeof(u.probeFunc));
        u.func = func;
        DummyMemFunc dfunc = u.repr.func;

        // Union used to patch the probe function
        union {
            DummyVirtualClass::ProbePtrType virtFunc;
            MicrosoftVirtualInheritanceRepr repr;
        } u2;
        static_assert(sizeof(u2.virtFunc) == sizeof(u2.repr));

        // Patch the probe function onto the XFuncType
        // then invoke to retrieve 'this'
        // Taking address of MF prevents it from being inlined
        u2.virtFunc = &DummyClasss::GetThis;
        u.repr.func = u2.repr.func;
        DummyClass* obj = (pthis->*u.probeFunc)();
        return { obj, dfunc };
    }
};

// Unknown inheritance
template <>
struct BindHelper<SingleMemberFuncSize + 3*sizeof(int)>
{
    // Handles multiple inheritance model of Microsoft
    template <class X, class XFuncType>
    static std::pair<DummyClass*, DummyMemFunc> Convert(X* pthis, XFuncType func) {

        // Union used to retrieve function pointer
        union {
            XFuncType func;
            MicrosoftUnknownInheritanceRepr repr;
        } u;
        static_assert(sizeof(MicrosoftUnknownInheritanceRepr) == sizeof(XFuncType));

        u.func = func;
        DummyMemFunc dfunc = u.repr.func;

        // Determine number of additional bytes to adjust 'this'
        //     if virtual inheritance is used
        int virtual_delta = 0;
        if (u.repr.vtable_index) {
            // Virtual inheritance is used, find the vtable which is
            // 'vtordisp' bytes from the start of the class
            const int* vtable = *reinterpret_cast<const int* const*>(
                reinterpret_cast<const char*>(pthis) + u.repr.vtordisp);

            // Use 'vtable_index' to look up the delta adjustment
            virtual_delta = u.repr.vtordisp + *reinterpret_cast<const int*>(
                reinterpret_cast<const char*>(vtable) + u.repr.vtable_index);
        }

        // The int at 'virtual_delta' gives us the number of bytes to add to 'this'.
        // Add the three components to determine the adjusted this pointer.
        DummyClass* obj = reinterpret_cast<DummyClass*>(
            reinterpret_cast<char *>(pthis) + u.repr.delta + virtual_delta);

        return { obj, dfunc };
    }

};

using DelegateFuncStorage = DummyMemFunc;

#pragma warning(pop)
#else // End _MSC

// In the Itanium ABI, all member functions consist of two ptrdiff_t
//    values: the function pointer and a this adjustment
struct ItaniumMemberFuncRepr {
    StaticFunc func; // pointer to function
    std::ptrdiff_t  delta; // byte adjustment to this
};

#if ITANIUM_DELEGATE_SPACE_SAVER

template <>
struct BindHelper<SingleMemberFuncSize>
{
    // This handles all cases of GCC, Clang, Intel.
    // We adjust the object pointer since we will discard the delta.
    // XXX is this adjustment correct and/or ever necessary?
    template <class X, class XFuncType>
    static std::pair<DummyClass*, DummyMemFunc> Convert(X* pthis, XFuncType func) {

        // Union used to retrieve function pointer
        union {
            XFuncType func;
            ItaniumMemberFuncRepr repr;
        } u;
        static_assert(sizeof(ItaniumMemberFuncRepr) == sizeof(XFuncType));

        // If there is a delta, adjust the 'this' pointer and clear the delta
        u.func = func;
        auto* obj = reinterpret_cast<DummyClass*>(reinterpret_cast<char*>(pthis) + std::ptrdiff_t(u.repr.delta));
        u.repr.delta = 0;
        return { obj, reinterpret_cast<DummyMemFunc>(u.func) };
    }
};

class DelegateFuncStorage
{
public:

    DelegateFuncStorage() = default;

    DelegateFuncStorage(const std::nullptr_t)
        : DelegateFuncStorage() {}

    DelegateFuncStorage(DummyMemFunc f)
        : m_func(toStorage(f))
    {}

    explicit operator DummyMemFunc() const { return toCallable(); }

    bool operator==(const DelegateFuncStorage& o) const { return m_func == o.m_func; }

private:
    DummyMemFunc toCallable() const {
        // Convert stored function to callable member representation
        union {
            DummyMemFunc func;
            ItaniumMemberFuncRepr repr;
        } u;
        u.repr.func = m_func;
        u.repr.delta = 0;
        return u.func;
    }

    StaticFunc toStorage(DummyMemFunc method) {
        // Strip off the delta adjustment, assert it is 0
        union {
            DummyMemFunc func;
            ItaniumMemberFuncRepr repr;
        } u;
        u.func = method;
        DEBUG_ASSERT(!u.repr.delta);
        return u.repr.func;
    }

    StaticFunc m_func = nullptr;
};

#else // Non-space saver, just storing the object and method pointer

template <>
struct BindHelper<SingleMemberFuncSize>
{
    // This handles all cases of GCC, Clang, Intel
    // Doing a cast to our dummy type and method pointer
    template <class X, class XFuncType>
    static std::pair<DummyClass*, DummyMemFunc> Convert(X* pthis, XFuncType func) {
        return { reinterpret_cast<DummyClass*>(pthis),
                 reinterpret_cast<DummyMemFunc>(func) };
    }
};

using DelegateFuncStorage = DummyMemFunc;

#endif // !ITANIUM_DELEGATE_SPACE_SAVER

#endif // !_MSC_VER

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

    DelegateStorage() = default;
    DelegateStorage(DummyClass* p, DummyMemFunc f)
        : m_this(p), m_func(f)
    {}

    inline DummyClass *getThis() const { return m_this; }
    inline DummyMemFunc getMemFunc() const { return DummyMemFunc(m_func); }

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
    DelegateFuncStorage m_func = nullptr;
};

} // end details namespace

template <typename Signature> class Delegate;

////////////////////////////////////////////////////////////////////////////////
//
// Delegate allow binding and invocation of functions, static methods, lambdas
// (with no captures), and class instance methods.
//

template <typename RetType, typename... Args>
class Delegate<RetType(Args...)> {

    using DummyClass = details::DummyClass;
    using DelegateStorage = details::DelegateStorage;
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
        : m_storage(MakeStorage(static_cast<X*>(const_cast<Y*>(pthis)), func))
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
    RetType operator() (Args ... args) const {
        DummyClass* obj = m_storage.getThis();
        DummyMemFunc func = getMemFunc();
        return (obj->*func)(std::forward<Args>(args)...);
    }

    void reset() { m_storage.reset(); }

    inline bool empty() const { return m_storage.empty(); }
    inline explicit operator bool() const { return !empty(); }
    inline bool operator!() const { return empty(); }

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
        auto p = details::BindHelper<sizeof(func)>::Convert(pthis, func);
        return { p.first, p.second };
    }

    // Store static methods and functions
    template <class ParentInvokerSig>
    static DelegateStorage MakeStorage(ParentInvokerSig staticFuncInvoker, StaticFunc func)
    {
        /* 'Evil': store function pointer in 'this' pointer */
        auto f = (func == nullptr) ? nullptr : staticFuncInvoker;
        auto* dObj = details::horrible_cast<DummyClass*>(func);
        auto dFunc = reinterpret_cast<details::DummyMemFunc>(f);
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

    RetType InvokeStaticFunction(Args ... args) const {
        // 'Evil' invoke: this pointer is invalid within the context of this call.
        // It's actually our static function!
        auto* func = getStaticFunc();
        return (*func)(std::forward<Args>(args)...);
    }

    DelegateStorage m_storage;
};

////////////////////////////////////////////////////////////////////////////////
//
// Helper functions use argument deduction to create an appropriate Delegate
//

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

#undef DEBUG_ASSERT
