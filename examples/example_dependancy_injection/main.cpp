#include <iostream>
#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <typeindex>
#include <typeinfo>
#include "ServiceLocator.hpp"

#include <boost/type_index.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/function.hpp>

std::string getTypeName(const boost::typeindex::type_index& typeIndex) {
    int status;
    auto s = __cxxabiv1::__cxa_demangle (typeIndex.name(), nullptr, nullptr, &status);
    std::string result;
    switch(status) {
        case 0:
            result = std::string(s);
            break;
        case 1:
            result = "Memory failure";
            break;
        case 2:
            result = "Not a mangled name";
            break;
        case 3:
            result = "Invalid arguments";
            break;
    }
    if (s) {
        free(s);
    }
    return result;
}



// Some plain interfaces
class IFood
{
public:
    virtual std::string name() = 0;
};

class IAnimal
{
public:
    virtual void eatFavouriteFood() = 0;
};


// Concrete classes which implement our interfaces, these 2 have no dependancies
class Banana : public IFood {
public:
    virtual std::string name() override {
        return "Banana";
    }
};

class Pizza : public IFood {
public:
    std::string name() override {
        return "Pizza";
    }
};

class Warthog : public IFood {
public:
    std::string name() override {
        return "Warthog";
    }
};

class Arepa : public IFood {
public:
    std::string name() override {
        return "Arepa";
    }
};


// Monkey requires a favourite food, note it is not dependant on ServiceLocator now
class Monkey : public IAnimal {
private:
    boost::shared_ptr<IFood> _food;

public:
    Monkey(boost::shared_ptr<IFood> food) : _food(food) {
    }

    void eatFavouriteFood() override {
        std::cout << "Monkey eats " << _food->name() << "\n";
    }
};

// Human requires a favourite food, note it is not dependant on ServiceLocator now
class Human : public IAnimal {
private:
    boost::shared_ptr<IFood> _food;

public:
    Human(boost::shared_ptr<IFood> food) : _food(food) {
    }

    void eatFavouriteFood() override {
        std::cout << "Human eats " << _food->name() << "\n";
    }
};

class Lion : public IAnimal {
private:
    boost::shared_ptr<IFood> _food;

public:
    Lion(boost::shared_ptr<IFood> food) : _food(food) {
    }

    void eatFavouriteFood() override {
        std::cout << "Lion eats " << _food->name() << "\n";
    }
};

class Matt : public IAnimal {
private:
    boost::shared_ptr<IFood> _food;

public:
    Matt(boost::shared_ptr<IFood> food) : _food(food) {
    }

    void eatFavouriteFood() override {
        std::cout << "Matt eats " << _food->name() << "\n";
    }
};

/* The SLModule classes are ServiceLocator aware, and they are also intimate with the concrete classes they bind to
   and so know what dependancies are required to create instances */
class FoodSLModule : public ServiceLocator::Module {
public:
    void load() override {
        bind<IFood>("Monkey").to<Banana>([] (SLContext_sptr slc) {
            return new Banana();
        });

        bind<IFood>("Human").to<Pizza>([] (SLContext_sptr slc) {
            return new Pizza();
        });

        bind<IFood>("Lion").to<Warthog>([] (SLContext_sptr slc) {
            return new Warthog();
        });

        bind<IFood>("Matt").to<Arepa>([] (SLContext_sptr slc) {
            return new Arepa();
        });
    }
};

class AnimalsSLModule : public ServiceLocator::Module {
public:
    void load() override {
        bind<IAnimal>("Human").to<Human>([] (SLContext_sptr slc) {
            return new Human(slc->resolve<IFood>("Human"));
        });

        bind<IAnimal>("Monkey").to<Monkey>([] (SLContext_sptr slc) {
            return new Monkey(slc->resolve<IFood>("Monkey"));
        });

        bind<IAnimal>("Lion").to<Lion>([] (SLContext_sptr slc) {
            return new Lion(slc->resolve<IFood>("Lion"));
        });

        bind<IAnimal>("Matt").to<Matt>([] (SLContext_sptr slc) {
            return new Matt(slc->resolve<IFood>("Matt"));
        });
    }
};

int main(int argc, const char * argv[]) {
    auto sl = ServiceLocator::create();

    sl->modules()
        .add<FoodSLModule>()
        .add<AnimalsSLModule>();

    auto slc = sl->getContext();

    std::vector<boost::shared_ptr<IAnimal>> animals;
    slc->resolveAll<IAnimal>(&animals);

    for(auto animal : animals) {
        animal->eatFavouriteFood();
    }

    boost::shared_ptr<IFood> food = slc->resolve<IFood>("Matt");
    std::cout << "found food " << food->name() << std::endl;

    std::cout << "type name '" << getTypeName(boost::typeindex::type_index(typeid(IFood))) << "'" << std::endl;
    std::cout << "type name '" << getTypeName(boost::typeindex::type_index(typeid(IAnimal))) << "'" << std::endl;
    std::cout << "type name '" << getTypeName(boost::typeindex::type_index(typeid(Human))) << "'" << std::endl;

    return 0;
}
