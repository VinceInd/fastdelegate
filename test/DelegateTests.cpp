#include "gtest/gtest.h"

#include "Delegate.h"
#include "HexDump.h"
#include <vector>
#include <string>
#include <sstream>
#include <tuple>
#include <exception>

using namespace delly;

template <typename Signature> class Received;

template <typename RetType, typename... Args>
struct Received<RetType(Args...)>{
    using Storage = std::tuple<typename std::decay<Args>::type...> ;
    using DelegateType = Delegate<RetType(Args...)>;

    template <typename... Args2>
    Received(const std::string& id, const Args2& ... args)
        : identifier(id),
          storage(args...)
    {}

    std::string identifier;
    Storage storage;
};

template <typename Signature> class Expectation;

template <typename RetType, typename... Args>
struct Expectation<RetType(Args...)> : public Received<RetType(Args...)> {

    using ReceivedType = Received<RetType(Args...)>;

    Expectation(const std::string& id, const RetType& toReturn, const Args& ... args)
        : ReceivedType(id, args...),
          ret(toReturn)
    {}

    RetType ret;
};

//--------------------------------------------------------------------------------
// Test framework

using ExpectType = Expectation<double(int, const char*, const std::string&)>;
using ReceivedType = ExpectType::ReceivedType;
using DelegateType = ExpectType::DelegateType;

std::vector<ReceivedType> g_received;
std::vector<ExpectType> g_expected;

void Expect(const std::string& id, double ret, int a, const char* b, const std::string& c) {
    g_expected.emplace_back(ExpectType{id, ret, a, b, c});
}

void TestUnexpectedReceive()
{
    // gtest: need to wrap assert in function returning void
    if (g_received.size() >= g_expected.size())
        printf("With %lu received...\n", g_received.size());
    ASSERT_LT(g_received.size(), g_expected.size());
}

double ProcessReceived(const std::string& id, int a, const char* b, const std::string& c) {
    TestUnexpectedReceive();
    g_received.emplace_back(id, a, b, c);
    auto& received = g_received.back();
    auto& expect = g_expected[g_received.size()-1];

    if (expect.storage != received.storage)
        printf("With %lu received...\n", g_received.size());
    EXPECT_EQ(expect.storage, received.storage);
    if (expect.identifier != received.identifier)
        printf("With %lu received...\n", g_received.size());
    EXPECT_EQ(expect.identifier, received.identifier);
    return expect.ret;
}

double SimpleFunction1(int a, const char* b, const std::string& c)
{ return ProcessReceived("SimpleFunction1", a, b, c); }

inline double InlinedSimpleFunction2(int a, const char* b, const std::string& c)
{ return ProcessReceived("InlinedSimpleFunction2", a, b, c); }

static double StaticSimpleFunction3(int a, const char* b, const std::string& c)
{ return ProcessReceived("StaticSimpleFunction3", a, b, c); }

struct NonVirtualClass1
{
    double Method1(int a, const char* b, const std::string& c)
    { return ProcessReceived("NVC.Method1", a, b, c); }

    inline double InlineMethod2(int a, const char* b, const std::string& c)
    { return ProcessReceived("NVC.InlineMethod2", a, b, c); }

    double ConstMethod3(int a, const char* b, const std::string& c) const
    { return ProcessReceived("NVC.ConstMethod3", a, b, c); }

    static double StaticMethod1(int a, const char* b, const std::string& c)
    { return ProcessReceived("NVC.StaticMethod1", a, b, c); }

    DelegateType GetPrivateMethod() { return MakeDelegate(this, &NonVirtualClass1::PrivateMethod1); }

private:
    double PrivateMethod1(int a, const char* b, const std::string& c)
    { return ProcessReceived("NVC.PrivateMethod1", a, b, c); }
};

struct DerivedNonVirtualClass1 : public NonVirtualClass1
{
    double Method4(int a, const char* b, const std::string& c)
    { return ProcessReceived("DNVC.Method4", a, b, c); }

    // hides base method
    inline double InlineMethod2(int a, const char* b, const std::string& c)
    { return ProcessReceived("DNVC.InlineMethod2", a, b, c); }

    // hides base method
    double ConstMethod3(int a, const char* b, const std::string& c) const
    { return ProcessReceived("DNVC.ConstMethod3", a, b, c); }

    static double StaticMethod1(int a, const char* b, const std::string& c)
    { return ProcessReceived("DNVC.StaticMethod1", a, b, c); }

};

//--------------------------------------------------------------------------------
// Test framework

class DelegateTestFramework : public ::testing::Test {

    virtual void SetUp() {
    }

    virtual void TearDown() {
        // No missing ivocations
        EXPECT_EQ(g_received.size(), g_expected.size());

        g_received.clear();
        g_expected.clear();
    }
};

template <typename T>
int DoDump(std::ostringstream& os, const T& t) {
    MakeHexDump(os, t);
    printf("%s\n", os.str().c_str());
    return 0;
}

template <typename T>
int DoDump(std::ostringstream& os, const std::vector<T>& vec) {
    for (const auto& t : vec)
    {
        MakeHexDump(os, t);
        printf("%s\n", os.str().c_str());
    }
    return 0;
}

template <typename... Items>
void Dump(const Items& ...item) {
#if 0
    std::ostringstream os;
    int unused[] = { DoDump(os, item)... };
    (void)unused;
#else
    // Do nothing
#endif
}

TEST_F(DelegateTestFramework, testStaticFunctions)
{
    Expect("SimpleFunction1", 4.5, -1, "literal0", std::string("foobar0"));
    Expect("InlinedSimpleFunction2", 40.5, -2, "literal1", std::string("foobar1"));
    Expect("StaticSimpleFunction3", 400.5, -3, "literal2", std::string("foobar2"));
    Expect("SimpleFunction1", -400.5, -4, "literal3", std::string("foobar3"));
    Expect("InlinedSimpleFunction2", -40.5, -5, "literal4", std::string("foobar4"));
    Expect("StaticSimpleFunction3", -4.5, -6, "literal5", std::string("foobar5"));

    DelegateType d1,d2,d3;
    d1 = MakeDelegate(&SimpleFunction1);
    d2 = MakeDelegate(&InlinedSimpleFunction2);
    d3 = MakeDelegate(&StaticSimpleFunction3);
    Dump(d1, d2, d3);

    EXPECT_DOUBLE_EQ(4.5, d1(-1, "literal0", "foobar0"));
    EXPECT_DOUBLE_EQ(40.5, d2(-2, "literal1", "foobar1"));
    EXPECT_DOUBLE_EQ(400.5, d3(-3, "literal2", "foobar2"));
    EXPECT_DOUBLE_EQ(-400.5, d1(-4, "literal3", "foobar3"));
    EXPECT_DOUBLE_EQ(-40.5, d2(-5, "literal4", "foobar4"));
    EXPECT_DOUBLE_EQ(-4.5, d3(-6, "literal5", "foobar5"));

    d1.bind(&InlinedSimpleFunction2);
    d2.bind(&StaticSimpleFunction3);
    d3.bind(&SimpleFunction1);
    Dump(d1, d2, d3);

    Expect("InlinedSimpleFunction2", -1.1, 1000, "a", {});
    Expect("StaticSimpleFunction3", -2.2, 100, "b", "E");
    Expect("SimpleFunction1", -3.3, 10, "c", {"d"});

    EXPECT_DOUBLE_EQ(-1.1, d1(1000, "a", ""));
    EXPECT_DOUBLE_EQ(-2.2, d2(100, "b", "E"));
    EXPECT_DOUBLE_EQ(-3.3, d3(10, "c", "d"));

    DelegateType d4(&InlinedSimpleFunction2);
    DelegateType d5(&StaticSimpleFunction3);
    DelegateType d6(&SimpleFunction1);
    Dump(d4, d5, d6);

    Expect("InlinedSimpleFunction2", -1.1, 1000, "a", {});
    Expect("StaticSimpleFunction3", -2.2, 100, "b", "E");
    Expect("SimpleFunction1", -3.3, 10, "c", {"d"});

    EXPECT_DOUBLE_EQ(-1.1, d4(1000, "a", ""));
    EXPECT_DOUBLE_EQ(-2.2, d5(100, "b", "E"));
    EXPECT_DOUBLE_EQ(-3.3, d6(10, "c", "d"));
}

TEST_F(DelegateTestFramework, testStaticComparisons)
{
    DelegateType d1, d2, d3;
    d1.bind(&InlinedSimpleFunction2);
    d2.bind(&StaticSimpleFunction3);
    d3.bind(&SimpleFunction1);
    Dump(d1, d2, d3);

    DelegateType d4(&InlinedSimpleFunction2);
    DelegateType d5(&StaticSimpleFunction3);
    DelegateType d6(&SimpleFunction1);
    Dump(d4, d5, d6);

    // Delegates are equal to themselves
    EXPECT_EQ(d1, d1);
    EXPECT_EQ(d2, d2);
    EXPECT_EQ(d3, d3);

    // And equal to identical function pointers
    EXPECT_EQ(d1, d4);
    EXPECT_EQ(d2, d5);
    EXPECT_EQ(d3, d6);

    // And not-equal
    EXPECT_NE(d1, d2);
    EXPECT_NE(d2, d1);

    // Empty compare equal to each other
    DelegateType e1, e2;
    Dump(e1, e2);
    EXPECT_EQ(e1, e2);
    EXPECT_NE(d1, e1);

    EXPECT_TRUE(e1.empty());
    EXPECT_FALSE(d1.empty());
    EXPECT_TRUE(bool(d1));
    EXPECT_FALSE(bool(!d1));
    EXPECT_TRUE(bool(!e1));
    EXPECT_FALSE(bool(e1));

    d1.reset();
    EXPECT_TRUE(d1.empty());

    EXPECT_FALSE(d2.empty());
    d2 = {};
    EXPECT_TRUE(d2.empty());
}


TEST_F(DelegateTestFramework, testBindMethods)
{
    NonVirtualClass1 c1, c2;

    std::vector<DelegateType> ds;
    ds.push_back(MakeDelegate(c1, &NonVirtualClass1::Method1));
    ds.push_back(MakeDelegate(c1, &NonVirtualClass1::InlineMethod2));
    ds.push_back(MakeDelegate(c1, &NonVirtualClass1::ConstMethod3));
    ds.push_back(MakeDelegate(&c1, &NonVirtualClass1::Method1));
    ds.push_back(MakeDelegate(&c1, &NonVirtualClass1::InlineMethod2));
    ds.push_back(MakeDelegate(&c1, &NonVirtualClass1::ConstMethod3));
    ds.push_back(MakeDelegate(c2, &NonVirtualClass1::Method1));
    ds.push_back(MakeDelegate(c2, &NonVirtualClass1::InlineMethod2));
    ds.push_back(MakeDelegate(c2, &NonVirtualClass1::ConstMethod3));
    ds.push_back(MakeDelegate(&c2, &NonVirtualClass1::Method1));
    ds.push_back(MakeDelegate(&c2, &NonVirtualClass1::InlineMethod2));
    ds.push_back(MakeDelegate(&c2, &NonVirtualClass1::ConstMethod3));
    Dump(ds);

    // Identical instance bindings
    EXPECT_EQ(ds[0], ds[3]);
    EXPECT_EQ(ds[1], ds[4]);
    EXPECT_EQ(ds[2], ds[5]);

    // Different methods, same object
    EXPECT_NE(ds[0], ds[1]);
    EXPECT_NE(ds[3], ds[4]);

    // Same method, different object
    EXPECT_NE(ds[0], ds[6]);
    EXPECT_NE(ds[1], ds[7]);
    EXPECT_NE(ds[2], ds[8]);
    EXPECT_NE(ds[3], ds[9]);
    EXPECT_NE(ds[4], ds[10]);
    EXPECT_NE(ds[5], ds[11]);

    Expect("NVC.Method1", -1.1, 1000, "a", "A");
    Expect("NVC.InlineMethod2", -2.2, 100, "b", "B");
    Expect("NVC.ConstMethod3", -3.3, 10, "c", "C");
    Expect("NVC.Method1", -4.4, 2000, "d", "D");
    Expect("NVC.InlineMethod2", -5.5, 200, "e", "E");
    Expect("NVC.ConstMethod3", -6.6, 20, "f", "F");

    EXPECT_DOUBLE_EQ(-1.1, ds[0](1000, "a", "A"));
    EXPECT_DOUBLE_EQ(-2.2, ds[1](100, "b", "B"));
    EXPECT_DOUBLE_EQ(-3.3, ds[2](10, "c", "C"));
    EXPECT_DOUBLE_EQ(-4.4, ds[3](2000, "d", "D"));
    EXPECT_DOUBLE_EQ(-5.5, ds[4](200, "e", "E"));
    EXPECT_DOUBLE_EQ(-6.6, ds[5](20, "f", "F"));
}

TEST_F(DelegateTestFramework, testStaticMethods)
{
    std::vector<DelegateType> ds;
    ds.push_back(MakeDelegate(&NonVirtualClass1::StaticMethod1));
    ds.push_back(MakeDelegate(&NonVirtualClass1::StaticMethod1));
    ds.push_back(MakeDelegate(&DerivedNonVirtualClass1::StaticMethod1));

    // Identical instance bindings
    EXPECT_EQ(ds[0], ds[1]);
    EXPECT_NE(ds[0], ds[2]);

    Expect("NVC.StaticMethod1", -1.1, 1000, "a", "A");
    Expect("NVC.StaticMethod1", -2.2, 100, "b", "B");
    Expect("DNVC.StaticMethod1", -3.3, 10, "c", "C");
    Expect("NVC.StaticMethod1", -4.4, 2000, "d", "D");
    Expect("NVC.StaticMethod1", -5.5, 200, "e", "E");
    Expect("DNVC.StaticMethod1", -6.6, 20, "f", "F");

    EXPECT_DOUBLE_EQ(-1.1, ds[0](1000, "a", "A"));
    EXPECT_DOUBLE_EQ(-2.2, ds[1](100, "b", "B"));
    EXPECT_DOUBLE_EQ(-3.3, ds[2](10, "c", "C"));
    EXPECT_DOUBLE_EQ(-4.4, ds[0](2000, "d", "D"));
    EXPECT_DOUBLE_EQ(-5.5, ds[1](200, "e", "E"));
    EXPECT_DOUBLE_EQ(-6.6, ds[2](20, "f", "F"));
}

TEST_F(DelegateTestFramework, testCapturePrivateMember)
{
    NonVirtualClass1 c1;
    DelegateType d1 = c1.GetPrivateMethod();
    Dump(d1);

    Expect("NVC.PrivateMethod1", -1.1, 1000, "a", "A");
    Expect("NVC.PrivateMethod1", -2.2, 100, "b", "B");

    EXPECT_DOUBLE_EQ(-1.1, d1(1000, "a", "A"));
    EXPECT_DOUBLE_EQ(-2.2, d1(100, "b", "B"));
}

TEST_F(DelegateTestFramework, testDerivedAndHiddenMethods)
{
    DerivedNonVirtualClass1 obj1, obj2;
    DerivedNonVirtualClass1* d1 = &obj1;
    DerivedNonVirtualClass1* d2 = &obj2;
    NonVirtualClass1* b1 = &obj1;

    std::vector<DelegateType> ds;

    ds.push_back(MakeDelegate(d1, &NonVirtualClass1::InlineMethod2));
    ds.push_back(MakeDelegate(d1, &NonVirtualClass1::ConstMethod3));
    ds.push_back(MakeDelegate(b1, &NonVirtualClass1::InlineMethod2));
    ds.push_back(MakeDelegate(b1, &NonVirtualClass1::ConstMethod3));
    ds.push_back(MakeDelegate(d1, &DerivedNonVirtualClass1::InlineMethod2));
    ds.push_back(MakeDelegate(d1, &DerivedNonVirtualClass1::ConstMethod3));
    Dump(ds);

    Expect("NVC.InlineMethod2", -1.1, 1000, "a", "A");
    Expect("NVC.ConstMethod3", -2.2, 100, "b", "B");
    Expect("NVC.InlineMethod2", -3.3, 10, "c", "C");
    Expect("NVC.ConstMethod3", -4.4, 2000, "d", "D");
    Expect("DNVC.InlineMethod2", -5.5, 200, "e", "E");
    Expect("DNVC.ConstMethod3", -6.6, 20, "f", "F");

    // Different methods for the same object
    EXPECT_NE(ds[0], ds[4]);
    EXPECT_NE(ds[1], ds[5]);

    // Different objects for the same method
    EXPECT_NE(ds[0], MakeDelegate(d2, &NonVirtualClass1::InlineMethod2));
    EXPECT_NE(ds[1], MakeDelegate(d2, &NonVirtualClass1::ConstMethod3));

    EXPECT_DOUBLE_EQ(-1.1, ds[0](1000, "a", "A"));
    EXPECT_DOUBLE_EQ(-2.2, ds[1](100, "b", "B"));
    EXPECT_DOUBLE_EQ(-3.3, ds[2](10, "c", "C"));
    EXPECT_DOUBLE_EQ(-4.4, ds[3](2000, "d", "D"));
    EXPECT_DOUBLE_EQ(-5.5, ds[4](200, "e", "E"));
    EXPECT_DOUBLE_EQ(-6.6, ds[5](20, "f", "F"));
}

TEST_F(DelegateTestFramework, testLambda)
{
    // Method 1 for binding lambdas
    auto l1 = [](int a, const char* b, const std::string& c)
        { return ProcessReceived("Lambda1", a, b, c); };
    DelegateType d1 = { l1 };

    Expect("Lambda1", -1.1, 1000, "a", "A");
    Expect("Lambda1", -2.2, 100, "b", "B");
    EXPECT_DOUBLE_EQ(-1.1, d1(1000, "a", "A"));
    EXPECT_DOUBLE_EQ(-2.2, d1(100, "b", "B"));

    // Method 2 for binding lambdas
    auto l2 = [](int a, const char* b, const std::string& c)
        { return ProcessReceived("Lambda2", a, b, c); };
    DelegateType d2;
    d2.bind(l2);

    Expect("Lambda2", -1.1, 1000, "a", "A");
    Expect("Lambda2", -2.2, 100, "b", "B");
    EXPECT_DOUBLE_EQ(-1.1, d2(1000, "a", "A"));
    EXPECT_DOUBLE_EQ(-2.2, d2(100, "b", "B"));

    // Method 3 for binding lambdas
    DelegateType d3 = MakeDelegate(+[](int a, const char* b, const std::string& c)
        { return ProcessReceived("Lambda3", a, b, c); });

    Expect("Lambda3", -3.3, 3000, "a", "A");
    Expect("Lambda3", -2.2, 300, "b", "B");
    EXPECT_DOUBLE_EQ(-3.3, d3(3000, "a", "A"));
    EXPECT_DOUBLE_EQ(-2.2, d3(300, "b", "B"));

    Dump(d1, d2, d3);
}

struct MoveMethodTester
{
    void Method1(std::string&& s)
    {
        received.emplace_back(std::move(s));
    }

    std::vector<std::string> received;
};

std::vector<std::string> g_received_move;

TEST_F(DelegateTestFramework, testMoveInvoke)
{
    MoveMethodTester t;
    Delegate<void(std::string&&)> d;

    d = MakeDelegate(t, &MoveMethodTester::Method1);
    Dump(d);
    std::string A = "A";

    d(std::move(A));
    d(std::move(A));

    EXPECT_EQ(t.received[0], "A");
    EXPECT_EQ(t.received[1], "");

    d = +[](std::string&& s) -> void { g_received_move.emplace_back(std::move(s)); };
    Dump(d);

    A = "A";

    d(std::move(A));
    d(std::move(A));

    EXPECT_EQ(g_received_move[0], "A");
    EXPECT_EQ(g_received_move[1], "");
}

struct BaseVirtualClass1
{
    ~BaseVirtualClass1() noexcept = default;

    double NonVirtualMethod1(int a, const char* b, const std::string& c)
    { return ProcessReceived("BVC.NonVirtualMethod1", a, b, c); }

    virtual double PureVirtualBase1(int a, const char* b, const std::string& c) = 0;
    virtual double PureVirtualBase2(int a, const char* b, const std::string& c) = 0;

    virtual double VirtualMethod3(int a, const char* b, const std::string& c)
    { return ProcessReceived("BVC.VirtualMethod3", a, b, c); }
};

// Multiple inheritance to adjust this
template <size_t n>
struct OtherStuff
{
    int foo1[n];
};

struct DerivedVirtualClass1 : public OtherStuff<8>, public BaseVirtualClass1
{
    double NonVirtualMethod1(int a, const char* b, const std::string& c)
    { return ProcessReceived("DVC.NonVirtualMethod1", a, b, c); }

    double PureVirtualBase1(int a, const char* b, const std::string& c) override
    { return ProcessReceived("DVC.PureVirtualBase1", a, b, c); }
    double PureVirtualBase2(int a, const char* b, const std::string& c) override
    { return ProcessReceived("DVC.PureVirtualBase2", a, b, c); }

    double VirtualMethod3(int a, const char* b, const std::string& c) override
    { return ProcessReceived("DVC.VirtualMethod3", a, b, c); }
};

TEST_F(DelegateTestFramework, testVirtualMethods)
{
    DerivedVirtualClass1 c1, c2;
    BaseVirtualClass1* b1 = &c1;
    BaseVirtualClass1* b2 = &c2;

    std::vector<DelegateType> ds;
    ds.push_back(MakeDelegate(&c1, &DerivedVirtualClass1::NonVirtualMethod1));
    ds.push_back(MakeDelegate(&c1, &DerivedVirtualClass1::PureVirtualBase1));
    ds.push_back(MakeDelegate(&c1, &DerivedVirtualClass1::PureVirtualBase2));
    ds.push_back(MakeDelegate(&c1, &DerivedVirtualClass1::VirtualMethod3));
    ds.push_back(MakeDelegate(&c2, &DerivedVirtualClass1::NonVirtualMethod1));
    ds.push_back(MakeDelegate(&c2, &DerivedVirtualClass1::PureVirtualBase1));
    ds.push_back(MakeDelegate(&c2, &DerivedVirtualClass1::PureVirtualBase2));
    ds.push_back(MakeDelegate(&c2, &DerivedVirtualClass1::VirtualMethod3));
    ds.push_back(MakeDelegate(b1, &BaseVirtualClass1::NonVirtualMethod1));
    ds.push_back(MakeDelegate(b1, &DerivedVirtualClass1::PureVirtualBase1));
    ds.push_back(MakeDelegate(b1, &DerivedVirtualClass1::PureVirtualBase2));
    ds.push_back(MakeDelegate(b1, &DerivedVirtualClass1::VirtualMethod3));
    ds.push_back(MakeDelegate(b2, &BaseVirtualClass1::NonVirtualMethod1));
    ds.push_back(MakeDelegate(b2, &DerivedVirtualClass1::PureVirtualBase1));
    ds.push_back(MakeDelegate(b2, &DerivedVirtualClass1::PureVirtualBase2));
    ds.push_back(MakeDelegate(b2, &DerivedVirtualClass1::VirtualMethod3));
    Dump(ds);

    EXPECT_NE(ds[0], ds[8]); // Method is non-virtual
    EXPECT_EQ(ds[1], ds[9]);
    EXPECT_EQ(ds[2], ds[10]);
    EXPECT_EQ(ds[3], ds[11]);
    EXPECT_NE(ds[4], ds[12]); // Method is non-virtual
    EXPECT_EQ(ds[5], ds[13]);
    EXPECT_EQ(ds[6], ds[14]);
    EXPECT_EQ(ds[7], ds[15]);

    // Instances are different
    EXPECT_NE(ds[0], ds[4]);
    EXPECT_NE(ds[1], ds[5]);
    EXPECT_NE(ds[2], ds[6]);
    EXPECT_NE(ds[3], ds[7]);

    Expect("DVC.NonVirtualMethod1", 1, 11, "a", "A");
    Expect("DVC.PureVirtualBase1",  2, 12, "b", "B");
    Expect("DVC.PureVirtualBase2",  3, 13, "c", "C");
    Expect("DVC.VirtualMethod3",    4, 14, "d", "D");
    Expect("DVC.NonVirtualMethod1", 5, 15, "e", "E");
    Expect("DVC.PureVirtualBase1",  6, 16, "f", "F");
    Expect("DVC.PureVirtualBase2",  7, 17, "g", "G");
    Expect("DVC.VirtualMethod3",    8, 18, "h", "H");
    Expect("BVC.NonVirtualMethod1", 9, 19, "i", "I");
    Expect("DVC.PureVirtualBase1", 10, 20, "j", "J");
    Expect("DVC.PureVirtualBase2", 11, 21, "k", "K");
    Expect("DVC.VirtualMethod3",   12, 22, "l", "L");
    Expect("BVC.NonVirtualMethod1",13, 23, "m", "M");
    Expect("DVC.PureVirtualBase1", 14, 24, "n", "N");
    Expect("DVC.PureVirtualBase2", 15, 25, "o", "O");
    Expect("DVC.VirtualMethod3",   16, 26, "p", "P");

    EXPECT_DOUBLE_EQ(1,  ds[ 0](11, "a", "A"));
    EXPECT_DOUBLE_EQ(2,  ds[ 1](12, "b", "B"));
    EXPECT_DOUBLE_EQ(3,  ds[ 2](13, "c", "C"));
    EXPECT_DOUBLE_EQ(4,  ds[ 3](14, "d", "D"));
    EXPECT_DOUBLE_EQ(5,  ds[ 4](15, "e", "E"));
    EXPECT_DOUBLE_EQ(6,  ds[ 5](16, "f", "F"));
    EXPECT_DOUBLE_EQ(7,  ds[ 6](17, "g", "G"));
    EXPECT_DOUBLE_EQ(8,  ds[ 7](18, "h", "H"));
    EXPECT_DOUBLE_EQ(9,  ds[ 8](19, "i", "I"));
    EXPECT_DOUBLE_EQ(10, ds[ 9](20, "j", "J"));
    EXPECT_DOUBLE_EQ(11, ds[10](21, "k", "K"));
    EXPECT_DOUBLE_EQ(12, ds[11](22, "l", "L"));
    EXPECT_DOUBLE_EQ(13, ds[12](23, "m", "M"));
    EXPECT_DOUBLE_EQ(14, ds[13](24, "n", "N"));
    EXPECT_DOUBLE_EQ(15, ds[14](25, "o", "O"));
    EXPECT_DOUBLE_EQ(16, ds[15](26, "p", "P"));
}

struct VirtualDerivedVirtualClass1 : public OtherStuff<10>, public virtual BaseVirtualClass1
{};
struct VirtualDerivedVirtualClass2 : public OtherStuff<12>, public virtual BaseVirtualClass1
{};

struct VirtualDerivedVirtualClass3 : public VirtualDerivedVirtualClass1, public VirtualDerivedVirtualClass2
{
    double NonVirtualMethod1(int a, const char* b, const std::string& c)
    { return ProcessReceived("VDVC.NonVirtualMethod1", a, b, c); }

    double PureVirtualBase1(int a, const char* b, const std::string& c) override
    { return ProcessReceived("VDVC.PureVirtualBase1", a, b, c); }
    double PureVirtualBase2(int a, const char* b, const std::string& c) override
    { return ProcessReceived("VDVC.PureVirtualBase2", a, b, c); }

    double VirtualMethod3(int a, const char* b, const std::string& c) override
    { return ProcessReceived("VDVC.VirtualMethod3", a, b, c); }
};

TEST_F(DelegateTestFramework, testVirtualInheritanceMethods)
{
    VirtualDerivedVirtualClass3 c1, c2;
    BaseVirtualClass1* b1 = &c1;
    BaseVirtualClass1* b2 = &c2;

    std::vector<DelegateType> ds;
    ds.push_back(MakeDelegate(&c1, &VirtualDerivedVirtualClass3::NonVirtualMethod1));
    ds.push_back(MakeDelegate(&c1, &VirtualDerivedVirtualClass3::PureVirtualBase1));
    ds.push_back(MakeDelegate(&c1, &VirtualDerivedVirtualClass3::PureVirtualBase2));
    ds.push_back(MakeDelegate(&c1, &VirtualDerivedVirtualClass3::VirtualMethod3));
    ds.push_back(MakeDelegate(&c2, &VirtualDerivedVirtualClass3::NonVirtualMethod1));
    ds.push_back(MakeDelegate(&c2, &VirtualDerivedVirtualClass3::PureVirtualBase1));
    ds.push_back(MakeDelegate(&c2, &VirtualDerivedVirtualClass3::PureVirtualBase2));
    ds.push_back(MakeDelegate(&c2, &VirtualDerivedVirtualClass3::VirtualMethod3));
    ds.push_back(MakeDelegate(b1, &BaseVirtualClass1::NonVirtualMethod1));
    ds.push_back(MakeDelegate(b1, &BaseVirtualClass1::PureVirtualBase1));
    ds.push_back(MakeDelegate(b1, &BaseVirtualClass1::PureVirtualBase2));
    ds.push_back(MakeDelegate(b1, &BaseVirtualClass1::VirtualMethod3));
    ds.push_back(MakeDelegate(b2, &BaseVirtualClass1::NonVirtualMethod1));
    ds.push_back(MakeDelegate(b2, &BaseVirtualClass1::PureVirtualBase1));
    ds.push_back(MakeDelegate(b2, &BaseVirtualClass1::PureVirtualBase2));
    ds.push_back(MakeDelegate(b2, &BaseVirtualClass1::VirtualMethod3));
    Dump(ds);

    EXPECT_NE(ds[0], ds[8]); // Method is non-virtual
    EXPECT_EQ(ds[1], ds[9]);
    EXPECT_EQ(ds[2], ds[10]);
    EXPECT_EQ(ds[3], ds[11]);
    EXPECT_NE(ds[4], ds[12]); // Method is non-virtual
    EXPECT_EQ(ds[5], ds[13]);
    EXPECT_EQ(ds[6], ds[14]);
    EXPECT_EQ(ds[7], ds[15]);

    // Instances are different
    EXPECT_NE(ds[0], ds[4]);
    EXPECT_NE(ds[1], ds[5]);
    EXPECT_NE(ds[2], ds[6]);
    EXPECT_NE(ds[3], ds[7]);

    Expect("VDVC.NonVirtualMethod1", 1, 11, "a", "A");
    Expect("VDVC.PureVirtualBase1",  2, 12, "b", "B");
    Expect("VDVC.PureVirtualBase2",  3, 13, "c", "C");
    Expect("VDVC.VirtualMethod3",    4, 14, "d", "D");
    Expect("VDVC.NonVirtualMethod1", 5, 15, "e", "E");
    Expect("VDVC.PureVirtualBase1",  6, 16, "f", "F");
    Expect("VDVC.PureVirtualBase2",  7, 17, "g", "G");
    Expect("VDVC.VirtualMethod3",    8, 18, "h", "H");
    Expect("BVC.NonVirtualMethod1", 9, 19, "i", "I");
    Expect("VDVC.PureVirtualBase1", 10, 20, "j", "J");
    Expect("VDVC.PureVirtualBase2", 11, 21, "k", "K");
    Expect("VDVC.VirtualMethod3",   12, 22, "l", "L");
    Expect("BVC.NonVirtualMethod1",13, 23, "m", "M");
    Expect("VDVC.PureVirtualBase1", 14, 24, "n", "N");
    Expect("VDVC.PureVirtualBase2", 15, 25, "o", "O");
    Expect("VDVC.VirtualMethod3",   16, 26, "p", "P");

    EXPECT_DOUBLE_EQ(1,  ds[ 0](11, "a", "A"));
    EXPECT_DOUBLE_EQ(2,  ds[ 1](12, "b", "B"));
    EXPECT_DOUBLE_EQ(3,  ds[ 2](13, "c", "C"));
    EXPECT_DOUBLE_EQ(4,  ds[ 3](14, "d", "D"));
    EXPECT_DOUBLE_EQ(5,  ds[ 4](15, "e", "E"));
    EXPECT_DOUBLE_EQ(6,  ds[ 5](16, "f", "F"));
    EXPECT_DOUBLE_EQ(7,  ds[ 6](17, "g", "G"));
    EXPECT_DOUBLE_EQ(8,  ds[ 7](18, "h", "H"));
    EXPECT_DOUBLE_EQ(9,  ds[ 8](19, "i", "I"));
    EXPECT_DOUBLE_EQ(10, ds[ 9](20, "j", "J"));
    EXPECT_DOUBLE_EQ(11, ds[10](21, "k", "K"));
    EXPECT_DOUBLE_EQ(12, ds[11](22, "l", "L"));
    EXPECT_DOUBLE_EQ(13, ds[12](23, "m", "M"));
    EXPECT_DOUBLE_EQ(14, ds[13](24, "n", "N"));
    EXPECT_DOUBLE_EQ(15, ds[14](25, "o", "O"));
    EXPECT_DOUBLE_EQ(16, ds[15](26, "p", "P"));
}

