#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include <functional>
#include <memory>
#include <vector>
#include <cstddef>

#include <boost/type_index.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/function.hpp>

#include "ServiceLocator.hpp"

class ITest {
public:
    std::string contextPath;
    ITest(SLContext_sptr slc) {
        auto slcp = slc.get();
        while(slcp->getParent() != NULL) {
            contextPath = contextPath + slcp->getInterfaceTypeName() + "->";
            slcp = slcp->getParent();
        }
    }

    virtual std::string getIt() = 0;
};

class TransientDestructor {
public:
    TransientDestructor(SLContext_sptr slc) {
    }

    int* destructCount;

    virtual ~TransientDestructor() {
        (*destructCount)++;
    }
};

class TestA : public ITest {
public:
    TestA(SLContext_sptr slc) : ITest(slc) {
    }

    virtual std::string getIt()  {
        return "TestA";
    }
};

class TestB : public ITest {
public:
    TestB(SLContext_sptr slc) : ITest(slc) {
    }

    virtual std::string getIt()  {
        return "TestB";
    }
};

class TestC {
public:
    boost::shared_ptr<ITest> test;

    TestC(SLContext_sptr slc) {
        test = slc->tryResolve<ITest>();
    }

    std::string getIt() {
        return "TestC";
    }
};

class TestNoSL {
public:
    TestNoSL() {
    }

    std::string getIt() {
        return "TestNoSL";
    }
};

static int TestEagerCount = 0;
class TestEager {
public:
    TestEager() {
        TestEagerCount++;
    }
};


class TestAModule : public ServiceLocator::Module {
public:
    void load()  {
        bind<ITest>().to<TestA>().asSingleton();
    }
};

class TestCModule : public ServiceLocator::Module {
public:
    void load()  {
        bind<TestC>().toSelf();
    }
};

struct Functor
{
    TestA* operator()(SLContext_sptr sptrc)
    {
        return new TestA(sptrc);
    }
};


TEST_CASE( "ServiceLocator", "[servicelocator]" ) {
    GIVEN("a ServiceLocator") {
        auto sl = ServiceLocator::create();

        SECTION("Basic type binding") {
            sl->bind<ITest>().to<TestA>();
            auto slc = sl->getContext();

            auto a = slc->resolve<ITest>();

            REQUIRE(a->getIt() == "TestA");
            REQUIRE(a->contextPath == "ITest->");
        }

        SECTION("Transient Destructor") {
            sl->bind<TransientDestructor>().toSelf();
            auto slc = sl->getContext();

            int destructCount = 0;
            {
                auto a = slc->resolve<TransientDestructor>();

                a->destructCount = &destructCount;
            }
            // a is out of scope and SL should not have a shared_ptr to the instance so it should destruct

            REQUIRE(destructCount == 1);
        }

        SECTION("Singleton no Destructor") {
            sl->bind<TransientDestructor>().toSelf().asSingleton();
            auto slc = sl->getContext();

            int destructCount = 0;
            {
                auto a = slc->resolve<TransientDestructor>();

                a->destructCount = &destructCount;
            }
            // a is out of scope and SL should have a shared_ptr to the instance so it should not destruct

            REQUIRE(destructCount == 0);
        }

        SECTION("Basic type binding as Singleton") {
            sl->bind<ITest>().to<TestA>().asSingleton();
            auto slc = sl->getContext();

            auto a = slc->resolve<ITest>();
            auto aa = slc->resolve<ITest>();

            REQUIRE(a == aa);
            REQUIRE(a->contextPath == "ITest->");
        }

        SECTION("Basic type binding to Instance") {
            auto sa = boost::shared_ptr<TestNoSL>(new TestNoSL());
            sl->bind<TestNoSL>().toInstance(sa);
            auto slc = sl->getContext();

            auto a = slc->resolve<TestNoSL>();
            auto aa = slc->resolve<TestNoSL>();

            REQUIRE(a == aa);
            REQUIRE(a == sa);
        }

        SECTION("Basic type binding as transient") {
            sl->bind<ITest>().to<TestA>();
            auto slc = sl->getContext();

            auto a1 = slc->resolve<ITest>();
            auto a2 = slc->resolve<ITest>();

            REQUIRE(a1 != a2);
        }

        SECTION("Binding to implementation, tryResolve to null") {
            sl->bind<TestC>().toSelf();
            auto slc = sl->getContext();

            auto c = slc->resolve<TestC>();

            REQUIRE(c->getIt() == "TestC");
            boost::shared_ptr<TestC> pnull;
            REQUIRE(!c->test);
            // REQUIRE(c->test == pnull);
            // REQUIRE(c->test == nullptr);
        }

        SECTION("Deep binding") {
            sl->bind<ITest>().to<TestA>();
            sl->bind<TestC>().toSelf();
            auto slc = sl->getContext();

            auto c = slc->resolve<TestC>();

            REQUIRE(c->getIt() == "TestC");
            REQUIRE(c->test);
            REQUIRE(c->test->getIt() == "TestA");
            REQUIRE(c->test->contextPath == "ITest->TestC->");
        }

        SECTION("Binding to implementation") {
            sl->bind<ITest>().to<TestA>();
            sl->bind<TestC>().toSelf();
            auto slc = sl->getContext();

            auto c = slc->resolve<TestC>();

            REQUIRE(c->getIt() == "TestC");
            REQUIRE(c->test->getIt() == "TestA");
        }

        SECTION("Duplicate binding throws") {
            sl->bind<ITest>().to<TestA>();

            REQUIRE_THROWS((sl->bind<ITest>().to<TestB>()));
        }


        SECTION("Named binding") {

            sl->bind<ITest>("X").to<TestA>();
            sl->bind<ITest>("Y").to<TestB>();
            auto slc = sl->getContext();

            boost::shared_ptr<ITest> x;
#if 0
            REQUIRE_THROWS([&] () {
                x = slc->resolve<ITest>();
            }());
#else
            struct Functor
            {
                Functor(boost::shared_ptr<ITest> sptr, SLContext_sptr sptrc) :
                    m_sptr(sptr),
                    m_sptrc(sptrc)
                {

                }

                void operator()()
                {
                    m_sptr = m_sptrc->resolve<ITest>();
                }

                boost::shared_ptr<ITest> m_sptr;
                SLContext_sptr m_sptrc;
            };
            Functor functor(x, slc);
            REQUIRE_THROWS(functor());

#endif
            x = slc->resolve<ITest>("X");
            auto y = slc->resolve<ITest>("Y");

            REQUIRE(x != y);
            REQUIRE(x->getIt() == "TestA");
            REQUIRE(y->getIt() == "TestB");
        }

        SECTION("Binding to transient function") {
#if 0
            sl->bind<ITest>().to<TestA>([] (SLContext_sptr slc) { return new TestA(slc); }).asTransient();
#else
            Functor functor;
            sl->bind<ITest>().to<TestA>(functor).asTransient();
#endif
            auto slc = sl->getContext();

            auto a = slc->resolve<ITest>();

            REQUIRE(a->getIt() == "TestA");

            auto b = slc->resolve<ITest>();

            REQUIRE(b->getIt() == "TestA");
            REQUIRE(a != b);
            REQUIRE(a->contextPath == "ITest->");
        }


        SECTION("Binding to singleton function") {
#if 0
            sl->bind<ITest>().to<TestA>([] (SLContext_sptr slc) { return new TestA(slc); }).asSingleton();
#else
            Functor functor;
            sl->bind<ITest>().to<TestA>(functor).asSingleton();
#endif
            auto slc = sl->getContext();

            auto a = slc->resolve<ITest>();

            REQUIRE(a->getIt() == "TestA");

            auto b = slc->resolve<ITest>();

            REQUIRE(a == b);
            REQUIRE(a->contextPath == "ITest->");
        }

        SECTION("Nested locator") {
            sl->bind<ITest>().to<TestA>();
            auto slc = sl->getContext();

            auto child1 = sl->enter();
            auto child2 = sl->enter();

            child1->bind<ITest>().to<TestB>();       // This should not throw, it should  its parent

            auto b = child1->getContext()->resolve<ITest>();
            auto a = child2->getContext()->resolve<ITest>();

            REQUIRE(a != b);
            REQUIRE(a->getIt() == "TestA");
            REQUIRE(a->contextPath == "ITest->");
            REQUIRE(b->getIt() == "TestB");
            REQUIRE(b->contextPath == "ITest->");
        }

        SECTION("Module loading") {
            sl->modules().add<TestAModule>().add<TestCModule>();
            auto slc = sl->getContext();

            auto a = slc->resolve<ITest>();
            auto c = slc->resolve<TestC>();

            REQUIRE(a->getIt() == "TestA");
            REQUIRE(c->getIt() == "TestC");
            REQUIRE(c->test->getIt() == "TestA");
            REQUIRE(c->test == a);
        }

        SECTION("Binding to constant interface") {
            const TestNoSL ta = TestNoSL();

            sl->bind<const TestNoSL>().toInstance(boost::shared_ptr<const TestNoSL>(&ta, ServiceLocator::NoDelete));
            auto slc = sl->getContext();

            auto a = slc->tryResolve<const TestNoSL>();

            REQUIRE(a);
        }

        SECTION("Resolve All bindings of type") {
            sl->bind<ITest>("A").to<TestA>();
            sl->bind<ITest>("B").to<TestB>();
            auto slc = sl->getContext();

            std::vector<boost::shared_ptr<ITest>> all;
            slc->resolveAll<ITest>(&all);
            REQUIRE(all.size() == 2);
            REQUIRE(all[0]->getIt() == "TestA");
            REQUIRE(all[1]->getIt() == "TestB");
        }

        SECTION("Eager binding") {
            sl->bind<TestEager>().toSelfNoDependancy().asSingleton().eagerly();

            REQUIRE(TestEagerCount == 0);

            // The binding will instantiate when we call getContext()
            auto slc = sl->getContext();

            REQUIRE(TestEagerCount == 1);
        }

        SECTION("Using empty shared_ptr instead of nullptr") {
            boost::shared_ptr<ITest> pnull;
            // REQUIRE(pnull == nullptr);
        }
    }
}

