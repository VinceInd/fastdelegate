#include "gtest/gtest.h"

#include "Delegate.h"
#include <vector>
#include <string>
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
    ASSERT_LT(g_received.size(), g_expected.size());
}

double ProcessReceived(const std::string& id, int a, const char* b, const std::string& c) {
    TestUnexpectedReceive();
    g_received.emplace_back(id, a, b, c);
    auto& received = g_received.back();
    auto& expect = g_expected[g_received.size()-1];

    EXPECT_EQ(expect.storage, received.storage);
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

    EXPECT_DOUBLE_EQ(4.5, d1(-1, "literal0", "foobar0"));
    EXPECT_DOUBLE_EQ(40.5, d2(-2, "literal1", "foobar1"));
    EXPECT_DOUBLE_EQ(400.5, d3(-3, "literal2", "foobar2"));
    EXPECT_DOUBLE_EQ(-400.5, d1(-4, "literal3", "foobar3"));
    EXPECT_DOUBLE_EQ(-40.5, d2(-5, "literal4", "foobar4"));
    EXPECT_DOUBLE_EQ(-4.5, d3(-6, "literal5", "foobar5"));

    d1.bind(&InlinedSimpleFunction2);
    d2.bind(&StaticSimpleFunction3);
    d3.bind(&SimpleFunction1);

    Expect("InlinedSimpleFunction2", -1.1, 1000, "a", {});
    Expect("StaticSimpleFunction3", -2.2, 100, "b", "E");
    Expect("SimpleFunction1", -3.3, 10, "c", {"d"});

    EXPECT_DOUBLE_EQ(-1.1, d1(1000, "a", ""));
    EXPECT_DOUBLE_EQ(-2.2, d2(100, "b", "E"));
    EXPECT_DOUBLE_EQ(-3.3, d3(10, "c", "d"));

    DelegateType d4(&InlinedSimpleFunction2);
    DelegateType d5(&StaticSimpleFunction3);
    DelegateType d6(&SimpleFunction1);

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

    DelegateType d4(&InlinedSimpleFunction2);
    DelegateType d5(&StaticSimpleFunction3);
    DelegateType d6(&SimpleFunction1);

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
    NonVirtualClass1 c1, c2;

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
    NonVirtualClass1* b2 = &obj2;

    std::vector<DelegateType> ds;

    ds.push_back(MakeDelegate(d1, &NonVirtualClass1::InlineMethod2));
    ds.push_back(MakeDelegate(d1, &NonVirtualClass1::ConstMethod3));
    ds.push_back(MakeDelegate(b1, &NonVirtualClass1::InlineMethod2));
    ds.push_back(MakeDelegate(b1, &NonVirtualClass1::ConstMethod3));
    ds.push_back(MakeDelegate(d1, &DerivedNonVirtualClass1::InlineMethod2));
    ds.push_back(MakeDelegate(d1, &DerivedNonVirtualClass1::ConstMethod3));

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
    DelegateType d1 = MakeDelegate(+[](int a, const char* b, const std::string& c)
    { return ProcessReceived("Lambda1", a, b, c); });

    Expect("Lambda1", -1.1, 1000, "a", "A");
    Expect("Lambda1", -2.2, 100, "b", "B");
    EXPECT_DOUBLE_EQ(-1.1, d1(1000, "a", "A"));
    EXPECT_DOUBLE_EQ(-2.2, d1(100, "b", "B"));
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
    std::string A = "A";

    d(std::move(A));
    d(std::move(A));

    EXPECT_EQ(t.received[0], "A");
    EXPECT_EQ(t.received[1], "");

    d = +[](std::string&& s) -> void { g_received_move.emplace_back(std::move(s)); };
    A = "A";

    d(std::move(A));
    d(std::move(A));

    EXPECT_EQ(g_received_move[0], "A");
    EXPECT_EQ(g_received_move[1], "");
}
