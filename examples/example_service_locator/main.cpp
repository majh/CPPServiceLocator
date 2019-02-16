#include <iostream>
#include "ServiceLocator.hpp"
#include <assert.h>

// template <class T>
// using sptr = std::shared_ptr<T>;

class IFood {
public:
	virtual std::string name() = 0;
};

class Banana : public IFood {
public:
	std::string name()  {
		return "Banana";
	}
};

class Pizza : public IFood {
public:
	std::string name()  {
		return "Pizza";
	}
};

class IAnimal {
public:
	virtual void eatFavouriteFood() = 0;
};

class Monkey : public IAnimal {
private:
	boost::shared_ptr<IFood> _food;

public:
	Monkey(SLContext_sptr slc) : _food(slc->resolve<IFood>("Monkey")) {
	}

	void eatFavouriteFood()  {
		std::cout << "Monkey eats " << _food->name() << "\n";
	}
};

class Human : public IAnimal {
private:
	boost::shared_ptr<IFood> _food;

public:
	Human(SLContext_sptr slc) : _food(slc->resolve<IFood>("Human")) {
	}

	void eatFavouriteFood()  {
		std::cout << "Human eats " << _food->name() << "\n";
	}
};

struct VisitFunctor16
{
    // TypedServiceLocator
    VisitFunctor16()
    {
    }

    Monkey*  operator()(SLContext_sptr slc)
    {
        return new Monkey(slc);
    }
};

struct VisitFunctor17
{
    // TypedServiceLocator
    VisitFunctor17()
    {
    }

    Human*  operator()(SLContext_sptr slc)
    {
        return new Human(slc);
    }
};



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
class TestA : public ITest {
public:
    TestA(SLContext_sptr slc) : ITest(slc) {
    }

    virtual std::string getIt()  {
        return "TestA";
    }
};


int main(int argc, const char * argv[]) {
    auto sl = ServiceLocator::create();
    // sl->bind<IAnimal>("Monkey").to<Monkey>([] (SLContext_sptr slc) { return new Monkey(slc); });
    // VisitFunctor16<Monkey> fn;
    VisitFunctor16 fn;
    sl->bind<IAnimal>("Monkey").to<Monkey>(fn);

    VisitFunctor17 fn2;
    sl->bind<IAnimal>("Human").to<Human>(fn2);

#if 0
    sl->bind<IAnimal>("Human").to<Human>([] (SLContext_sptr slc) { return new Human(slc); });

#endif
    sl->bind<IFood>("Monkey").toNoDependancy<Banana>();
    sl->bind<IFood>("Human").toNoDependancy<Pizza>();

    auto slc = sl->getContext();

    auto monkey = slc->resolve<IAnimal>("Monkey");

    monkey->eatFavouriteFood();

    auto human = slc->resolve<IAnimal>("Human");
    human->eatFavouriteFood();

    return 0;
}
