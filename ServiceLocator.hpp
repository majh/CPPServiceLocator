#ifndef ServiceLocator_hpp
#define ServiceLocator_hpp
#include <stdio.h>
#include <string>
#include <iostream>
#include <memory>
#include <map>
#include <list>
#include <vector>
#include <set>
#include <boost/type_index.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/function.hpp>
#include <cxxabi.h>
#include <functional>
#include <vector>
#include <tr1/memory>
#include <memory>

class ServiceLocatorException {
private:
    std::string m_message;

public:
    ServiceLocatorException(const std::string& message) :
        m_message(message)
    {
    }

    const std::string& getMessage() const
    {
        return m_message;
    }
};

class DuplicateBindingException : public ServiceLocatorException
{
public:
    DuplicateBindingException(const std::string& message) :
        ServiceLocatorException(message)
    {
    }
};

class RecursiveResolveException : public ServiceLocatorException {
public:
    RecursiveResolveException(const std::string& message) :
        ServiceLocatorException(message)
    {
    }
};

class BindingIssueException : public ServiceLocatorException {
public:
    BindingIssueException(const std::string& message) :
        ServiceLocatorException(message)
    {
    }
};

class UnableToResolveException : public ServiceLocatorException {
public:
    UnableToResolveException(const std::string& message) :
        ServiceLocatorException(message)
    {
        std::cerr << message << std::endl;
    }
};

class ServiceLocator
{
public:
    friend class Context;

    class Context
    {
    private:
        // Only the root Context will run the AfterResolveList - this allows circular dependancies to
        // resolve by using afterResolve property injection
        Context* m_root;
        std::list<boost::function<void(boost::shared_ptr<Context>)>>* m_fnAfterResolveList;

        Context* m_parent;
        boost::weak_ptr<ServiceLocator> m_sl;
        boost::typeindex::type_index m_interfaceType;
        mutable std::unique_ptr<std::string> m_interfaceTypeName;
        std::string m_name;

        std::unique_ptr<boost::typeindex::type_index> m_concreteType;
        mutable std::unique_ptr<std::string> m_concreteTypeName;


        std::string getTypeName(const boost::typeindex::type_index& typeIndex) const {
            int status;
            auto s = __cxxabiv1::__cxa_demangle (typeIndex.name(), NULL, NULL, &status);
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


        void afterResolve() {
            if (this == m_root) {
                if (m_fnAfterResolveList != NULL) {
                    for(auto fn = m_fnAfterResolveList->begin(); fn != m_fnAfterResolveList->end(); fn++) {
                        auto ctx = boost::shared_ptr<Context>(new Context(m_sl));
                        (*fn)(ctx);
                    }
                    delete m_fnAfterResolveList;
                    m_fnAfterResolveList = NULL;
                }
            }
        }

    public:
        Context(Context* root, Context* parent, boost::weak_ptr<ServiceLocator> sl, const boost::typeindex::type_index interfaceType, const std::string& name) :
            m_root(root),
            m_parent(parent),
            m_sl(sl),
            m_interfaceType(interfaceType),
            m_name(name),
            m_fnAfterResolveList(NULL)
        {
            // Empty
        }

        Context(boost::weak_ptr<ServiceLocator> sl) :
            m_root(this),
            m_parent(NULL),
            m_sl(sl),
            m_interfaceType(boost::typeindex::type_index(typeid(void))),
            m_name(""),
            m_fnAfterResolveList(NULL)
        {
            // Empty
        }

        Context(Context* parent, const boost::typeindex::type_index interfaceType, const std::string& name) :
            m_root(parent->m_root),
            m_parent(parent),
            m_sl(parent->m_sl),
            m_interfaceType(interfaceType),
            m_name(name)
        {
            // Empty
        }

        Context(boost::weak_ptr<ServiceLocator> sl, const boost::typeindex::type_index interfaceType, const std::string& name) :
            m_root(this),
            m_parent(NULL),
            m_sl(sl),
            m_interfaceType(interfaceType),
            m_name(name)
        {
            // Empty
        }

        const std::string& getName() const
        {
            return m_name;
        }

        const std::string& getInterfaceTypeName() const {
            if (m_interfaceTypeName == NULL)
            {
                m_interfaceTypeName = std::unique_ptr<std::string>(new std::string(getTypeName(m_interfaceType)));
            }
            return *m_interfaceTypeName;
        }

        const boost::typeindex::type_index& getInterfaceTypeIndex() const
        {
            return m_interfaceType;
        }

        void setConcreteType(const boost::typeindex::type_index& concreteType) {
            if (m_concreteType != NULL) {
                throw BindingIssueException("Concrete type on Context already set");
            }
            m_concreteType = std::unique_ptr<boost::typeindex::type_index>(new boost::typeindex::type_index(concreteType));
        }

        const std::string& getConcreteTypeName() const {
            if (m_concreteTypeName == NULL) {
                m_concreteTypeName = std::unique_ptr<std::string>(new std::string(getTypeName(*m_concreteType)));
            }
            return *m_concreteTypeName;
        }

        const boost::typeindex::type_index& getConcreteTypeIndex() const {
            return *m_concreteType;
        }

        Context* getParent() const {
            return m_parent;
        }

        boost::shared_ptr<ServiceLocator> getServiceLocator() const {
            return m_sl.lock();
        }

        void checkRecursiveResolve(Context* resolveCtx, Context* compareCtx) {
            if (resolveCtx->m_interfaceType == compareCtx->m_interfaceType && resolveCtx->m_name == compareCtx->m_name) {
                throw RecursiveResolveException("Recursive resolve path = " + resolveCtx->getResolvePath());
            }
            if (compareCtx->m_parent != NULL) {
                checkRecursiveResolve(resolveCtx, compareCtx->m_parent);
            }
        }

        // Resolve a named interface, throws if not able to resolve
        template <class IFace>
        boost::shared_ptr<IFace> resolve(const std::string& named) {
            auto ctx = boost::shared_ptr<Context>(new Context(this, boost::typeindex::type_index(typeid(IFace)), named));
            checkRecursiveResolve(ctx.get(), this);
            auto ptr = m_sl.lock()->m_resolve<IFace>(ctx);
            afterResolve();
            return ptr;
        }

        // Resolve an interface, throws if not able to resolve
        template <class IFace>
        boost::shared_ptr<IFace> resolve() {
            auto ctx = boost::shared_ptr<Context>(new Context(this, boost::typeindex::type_index(typeid(IFace)), ""));
            checkRecursiveResolve(ctx.get(), this);
            auto ptr = m_sl.lock()->m_resolve<IFace>(ctx);
            afterResolve();
            return ptr;
        }

        template <class IFace>
        void resolveAll(std::vector<boost::shared_ptr<IFace>>* all) {
            Functor<IFace> visit(this, all);
            m_sl.lock()->_visitAll<IFace>(visit);
            afterResolve();
        }

        // Determine if a named interface can be resolved
        template <class IFace>
        bool canResolve(const std::string& named) {
            auto ctx = boost::shared_ptr<Context>(new Context(this, boost::typeindex::type_index(typeid(IFace)), named));
            return m_sl.lock()->_canResolve<IFace>(ctx);
        }

        // Determine if an interface can be resolved
        template <class IFace>
        bool canResolve() {
            auto ctx = boost::shared_ptr<Context>(new Context(this, boost::typeindex::type_index(typeid(IFace)), ""));
            return m_sl.lock()->_canResolve<IFace>(ctx);
        }

        // Try to resolve a named interface, returns NULL on failure
        template <class IFace>
        boost::shared_ptr<IFace> tryResolve(const std::string& named) {
            auto ctx = boost::shared_ptr<Context>(new Context(this, boost::typeindex::type_index(typeid(IFace)), named));
            checkRecursiveResolve(ctx.get(), this);
            auto ptr = m_sl.lock()->m_tryResolve<IFace>(ctx);
            afterResolve();
            return ptr;
        }

        // Try to resolve an interface, returns NULL on failure
        template <class IFace>
        boost::shared_ptr<IFace> tryResolve() {
            auto ctx = boost::shared_ptr<Context>(new Context(this, boost::typeindex::type_index(typeid(IFace)), ""));
            checkRecursiveResolve(ctx.get(), this);
            auto ptr = m_sl.lock()->m_tryResolve<IFace>(ctx);
            afterResolve();
            return ptr;
        }

        template <class IFace>
        boost::function<boost::shared_ptr<IFace>(const std::string&)> provider() {
            // We lock the weak_ptr to our ServiceLocator, the lock returns a shared_ptr which will keep
            // it alive into the returned lambda via the capture of sl
            auto sl = m_sl.lock();
            Functor2<IFace> visit(sl);
            return visit();
        }

        template <class IFace>
        boost::function<boost::shared_ptr<IFace>(const std::string&)> tryProvider() {
            // We lock the weak_ptr to our ServiceLocator, the lock returns a shared_ptr which will keep
            // it alive into the returned lambda via the capture of sl
            auto sl = m_sl.lock();
            Functor3<IFace> visit(sl);
            return visit();
        }

        std::string getResolvePath() const {
            std::string path = "";
            // Note the root Parent has a <ServiceLocator> IFace which is not real, just cannot have no interface defined
            if (m_parent != NULL && m_parent->m_parent != NULL) {
                path = m_parent->getResolvePath() + " -> ";
            }

            path += "resolve<" + getInterfaceTypeName() + ">(" + m_name + ")";

            if (m_concreteType != NULL) {
                path += ".to<" + getConcreteTypeName() + ">";
            }

            return path;
        }

        void afterResolve(boost::function<void(boost::shared_ptr<Context>)> fnAfterResolve) {
            if (m_root->m_fnAfterResolveList == NULL) {
                m_root->m_fnAfterResolveList = new std::list<boost::function<void(boost::shared_ptr<Context>)>>();
            }
            m_root->m_fnAfterResolveList->push_back(fnAfterResolve);
        }
    };

private:
    class AnyServiceLocator {
    public:
        virtual ~AnyServiceLocator() {
        }

        class loose_binding {
        public:
            virtual ~loose_binding() {
            }

            virtual void eagerBind(boost::shared_ptr<Context> slc) = 0;
        };
    };

    template <class IFace>
    class TypedServiceLocator : public AnyServiceLocator {
    public:
        class shared_ptr_binding : public loose_binding {
            template <typename T> friend struct Functor4;
        private:
            boost::function<boost::shared_ptr<IFace>(boost::shared_ptr<Context>)> m_fnGet;
            boost::function<boost::shared_ptr<IFace>(boost::shared_ptr<Context>)> m_fnCreate;
            // Note, this is used during binding only ..
            std::list<loose_binding*>* m_eagerBindings;

        public:

            class eagerly_clause {
            private:
                shared_ptr_binding* m_ibinding;
            public:
                eagerly_clause(shared_ptr_binding* ibinding) :
                    m_ibinding(ibinding)
                {
                }

                void eagerly() {
                    m_ibinding->m_eagerBindings->push_back(m_ibinding);
                }
            };

            class as_clause {
            private:
                boost::shared_ptr<IFace> m_singleton;
                shared_ptr_binding* m_ibinding;

            public:

                as_clause(shared_ptr_binding* ibinding) :
                    m_ibinding(ibinding)
                {
                }

                eagerly_clause& asSingleton()
                {
                    Functor4<IFace> v4(m_singleton, m_ibinding);
                    m_ibinding->m_fnGet = v4;
                    return m_ibinding->m_eagerly_clause;
                }

                void asTransient() {
                    m_ibinding->m_fnGet = m_ibinding->m_fnCreate;
                }
            };

            class to_clause {
            private:
                shared_ptr_binding* m_ibinding;

            public:

                to_clause(shared_ptr_binding* ibinding) :
                    m_ibinding(ibinding)
                {
                }

                void toInstance(boost::shared_ptr<IFace> instance) {
                    FunctorY<IFace> v5(instance);
                    m_ibinding->m_fnGet = v5;
                }


                void toInstance(IFace* instance) {
                    auto cached = boost::shared_ptr<IFace>(instance);
                    Functor6<IFace> fn(cached);
                    m_ibinding->m_fnGet = fn;
                }


                as_clause& toSelf() {
                    Functor7<IFace> fn;
                    m_ibinding->m_fnGet = m_ibinding->m_fnCreate = fn;
                    return m_ibinding->m_as_clause;
                }

                as_clause& toSelfNoDependancy() {
                    Functor8<IFace> fn;
                    m_ibinding->m_fnGet = m_ibinding->m_fnCreate = fn;
                    return m_ibinding->m_as_clause;
                }

                template <class TImpl>
                as_clause& to() {
                    Functor9<TImpl> fn;
                    m_ibinding->m_fnGet = m_ibinding->m_fnCreate = fn;
                    return m_ibinding->m_as_clause;
                }

                template <class TImpl>
                as_clause& toNoDependancy() {
                    Functor10<TImpl> fn;
                    m_ibinding->m_fnGet = m_ibinding->m_fnCreate = fn;
                    return m_ibinding->m_as_clause;
                }

                // similar to above, except caller can return IFace* instead of boost::shared_ptr<IFace>
                template <class TImpl>
                as_clause& to(boost::function<TImpl*(boost::shared_ptr<Context>)> fnCreate) {
                    Functor12<TImpl> fn(fnCreate);
                    m_ibinding->m_fnGet = m_ibinding->m_fnCreate = fn;
                    return m_ibinding->m_as_clause;
                }

                as_clause& alias(const std::string& name) {
                    Functor13<IFace> fn(name);
                    m_ibinding->m_fnGet = m_ibinding->m_fnCreate = fn;
                    return m_ibinding->m_as_clause;
                }

                template <class IAlias>
                as_clause& alias() {
                    Functor14<IAlias> fn();
                    m_ibinding->m_fnGet = m_ibinding->m_fnCreate = fn;
                    return m_ibinding->m_as_clause;
                }

                template <class IAlias>
                as_clause& alias(const std::string& name) {
                    Functor15<IAlias> fn(name);
                    m_ibinding->m_fnGet = m_ibinding->m_fnCreate = fn;
                    return m_ibinding->m_as_clause;
                }
            };

            to_clause m_to_clause;
            as_clause m_as_clause;
            eagerly_clause m_eagerly_clause;

        public:
            shared_ptr_binding(std::list<loose_binding*>* eagerBindings) :
                m_eagerBindings(eagerBindings),
                m_to_clause(this),
                m_as_clause(this),
                m_eagerly_clause(this) {
            }

            virtual boost::shared_ptr<IFace> get(boost::shared_ptr<Context> slc) const {
                return m_fnGet(slc);
            }

            void eagerBind(boost::shared_ptr<Context> slc)
            {
                auto ctx = boost::shared_ptr<Context>(new Context(slc.get(), boost::typeindex::type_index(typeid(IFace)), ""));
                m_fnGet(ctx);
            }
        };

        std::map<std::string, boost::shared_ptr<loose_binding>> m_bindings;

    public:
        typename shared_ptr_binding::to_clause& bind(const std::string& name, std::list<loose_binding*>* eagerBindings) {
            if (canResolve(name)) {
                throw DuplicateBindingException(std::string("Duplicate binding for <") + typeid(IFace).name() + "> named " + name);
            }

            auto binding = boost::shared_ptr<shared_ptr_binding>(new shared_ptr_binding(eagerBindings));

            // (non const) IFace binding
            m_bindings.insert(std::pair<std::string, boost::shared_ptr<loose_binding>>(name, binding));

            return binding->m_to_clause;
        }

        bool canResolve(const std::string& name) {
            return m_bindings.find(name) != m_bindings.end();
        }

        boost::shared_ptr<IFace> tryResolve(const std::string& name, boost::shared_ptr<Context> slc) {
            auto binding = m_bindings.find(name);
            if (binding == m_bindings.end()) {
                boost::shared_ptr<IFace> nullp;
                return nullp;
            }
            auto ibinding = boost::dynamic_pointer_cast<shared_ptr_binding>(binding->second);
            return ibinding->get(slc);
        }

        void visitAll(boost::function<void(boost::shared_ptr<TypedServiceLocator<IFace>::shared_ptr_binding>)> fnVisit) {
            for(auto binding = m_bindings.begin(); binding != m_bindings.end(); ++binding) {
                auto ibinding = boost::dynamic_pointer_cast<shared_ptr_binding>((*binding).second);
                fnVisit(ibinding);
            }
        }
    };

    // Named locator bindings (simple map from string to NamedServiceLocator)
    std::map<boost::typeindex::type_index, AnyServiceLocator*> m_typed_locators;
    mutable std::list<AnyServiceLocator::loose_binding*> m_eagerBindings;

    boost::shared_ptr<ServiceLocator> m_parent;
    boost::shared_ptr<Context> m_context;

    // We store a weak_ptr to ourselves so that we can create shared_ptr's from it when we enter() child
    // locators
    boost::weak_ptr<ServiceLocator> m_this;

    template <class IFace>
    TypedServiceLocator<IFace>* getTypedServiceLocator(bool createIfRequired) {
        auto typeIndex = boost::typeindex::type_index(typeid(IFace));
        auto find = m_typed_locators.find(typeIndex);
        if (find != m_typed_locators.end()) {
            return dynamic_cast<TypedServiceLocator<IFace>*>(find->second);
        }

        if (!createIfRequired) {
            return NULL;
        }

        auto nsl = new TypedServiceLocator<IFace>();
        m_typed_locators.insert(std::pair<boost::typeindex::type_index, AnyServiceLocator*>(typeIndex, nsl));
        return nsl;
    }

    template <class IFace>
    struct Functor
    {
            Functor(Context* ctx, std::vector<boost::shared_ptr<IFace> >* all) :
                m_ctx(ctx),
                m_all(all)
            {

            }

            void operator()(boost::shared_ptr<typename TypedServiceLocator<IFace>::shared_ptr_binding> binding)
            {
                auto ctx = boost::shared_ptr<Context>(new Context(m_ctx, boost::typeindex::type_index(typeid(IFace)), ""));
                m_ctx->checkRecursiveResolve(ctx.get(), m_ctx);
                m_all->push_back(binding->get(ctx));
            }

            Context* m_ctx;
            std::vector<boost::shared_ptr<IFace> >* m_all;
    };

    template <class IFace>
    struct Functor2
    {
        Functor2(boost::shared_ptr<ServiceLocator> sl) :
            m_sl(sl)
        {
        }

        boost::function<boost::shared_ptr<IFace> > operator()(const std::string& name = "")
        {
            auto ctx = boost::shared_ptr<Context>(new Context(m_sl, boost::typeindex::type_index(typeid(IFace)), name));
            // Don't need to check for recursive resolve since this is a provider (root) call
            auto ptr = m_sl->m_resolve<IFace>(ctx);
            // ctx is root Context, it can afterResolve
            ctx->afterResolve();
            return ptr;
        }

        boost::shared_ptr<ServiceLocator> m_sl;
    };

    template <class IFace>
    struct Functor3
    {
        Functor3(boost::shared_ptr<ServiceLocator> sl) :
            m_sl(sl)
        {

        }

        boost::function<boost::shared_ptr<IFace> > operator()(const std::string& name = "")
        {
            auto ctx = boost::shared_ptr<Context>(new Context(m_sl, boost::typeindex::type_index(typeid(IFace)), name));
            // Don't need to check for recursive resolve since this is a provider (root) call
            auto ptr = m_sl->m_tryResolve<IFace>(ctx);
            // ctx is root Context, it can afterResolve
            ctx->afterResolve();
            return ptr;
        }

        boost::shared_ptr<ServiceLocator> m_sl;
    };

    template <class IFace>
    struct FunctorY
    {
        FunctorY(boost::shared_ptr<IFace> instance) :
            m_instance(instance)
        {
        }

        boost::shared_ptr<IFace> operator()(boost::shared_ptr<Context> sl)
        {
            return m_instance;
        }

        boost::shared_ptr<IFace> m_instance;
    };

    template <class IFace>
    struct Functor4a
    {
            Functor4a(boost::shared_ptr<IFace> singleton) :
                m_singleton(singleton)
            {
            }

            // boost::function<boost::shared_ptr<IFace> > operator()(boost::shared_ptr<Context> sl)
            boost::shared_ptr<IFace> operator()(boost::shared_ptr<Context> sl)
            {
                return m_singleton;
            }

            boost::shared_ptr<IFace> m_singleton;
    };

    template <class IFace>
    struct Functor5a
    {
        Functor5a(boost::shared_ptr<IFace> singleton) :
            m_singleton(singleton)
        {
        }

        boost::shared_ptr<IFace> operator()(boost::shared_ptr<Context> sl)
        {
            return m_singleton;
        }

        boost::shared_ptr<IFace> m_singleton;
    };

    template <class IFace>
    struct Functor5b
    {
        Functor5b(boost::shared_ptr<IFace> singleton) :
            m_singleton(singleton)
        {
        }

        boost::shared_ptr<IFace> operator()(boost::shared_ptr<Context> sl)
        {
            return m_singleton;
        }

        boost::shared_ptr<IFace> m_singleton;
    };

    template <class IFace>
    struct Functor4
    {
        // class TypedServiceLocator<IFace>::shared_ptr_binding;
        Functor4(boost::shared_ptr<IFace> sing, typename TypedServiceLocator<IFace>::shared_ptr_binding* binding) :
            m_sing(sing),
            m_ibinding(binding)
        {
        }

        boost::shared_ptr<IFace> operator()(boost::shared_ptr<Context> sl)
        {
            m_sing = m_ibinding->m_fnCreate(sl);
            auto singleton = m_sing;

            Functor4a<IFace> fn(m_sing);
            // rebind fnGet to return the new singleton
            m_ibinding->m_fnGet = fn;
            return singleton;
        }

        boost::shared_ptr<IFace> m_sing;
        typename TypedServiceLocator<IFace>::shared_ptr_binding* m_ibinding;
    };

    template <class IFace>
    struct Functor6
    {
        Functor6(boost::shared_ptr<IFace> instance) :
            m_instance(instance)
        {
        }

        boost::function<boost::shared_ptr<IFace> > operator()(boost::shared_ptr<Context> sl)
        {
            return m_instance;
        }

        boost::shared_ptr<IFace> m_instance;
    };

    template <class IFace>
    struct Functor7
    {
        boost::shared_ptr<IFace> operator()(boost::shared_ptr<Context> sl)
        {
            sl->setConcreteType(boost::typeindex::type_index(typeid(IFace)));
            return boost::shared_ptr<IFace>(new IFace(sl));
        }
    };

    template <class IFace>
    struct Functor8
    {
        boost::shared_ptr<IFace>  operator()(boost::shared_ptr<Context> sl)
        {
            sl->setConcreteType(boost::typeindex::type_index(typeid(IFace)));
            return boost::shared_ptr<IFace>(new IFace());
        }
    };

    template <class TImpl>
    struct Functor9
    {
        boost::shared_ptr<TImpl> operator()(boost::shared_ptr<Context> sl)
        {
            sl->setConcreteType(boost::typeindex::type_index(typeid(TImpl)));
            return boost::shared_ptr<TImpl>(new TImpl(sl));
        }
    };

    template <class TImpl>
    struct Functor10
    {
        boost::shared_ptr<TImpl>  operator()(boost::shared_ptr<Context> sl)
        {
            sl->setConcreteType(boost::typeindex::type_index(typeid(TImpl)));
            return boost::shared_ptr<TImpl>(new TImpl());
        }
    };

    template <class TImpl>
    struct Functor11
    {
        Functor11(boost::function<boost::shared_ptr<TImpl>(boost::shared_ptr<Context>)> fn):
            m_fn(fn)
        {
        }

        boost::function<boost::shared_ptr<TImpl> > operator()(boost::shared_ptr<Context> sl)
        {
            sl->setConcreteType(boost::typeindex::type_index(typeid(TImpl)));
            return m_fn(sl);
        }
        boost::function<boost::shared_ptr<TImpl>(boost::shared_ptr<Context>)> m_fn;
    };

    template <class TImpl>
    struct Functor12
    {
        Functor12(boost::function<TImpl*(boost::shared_ptr<Context>)> fn) :
            m_fn(fn)
        {
        }

        boost::shared_ptr<TImpl> operator()(boost::shared_ptr<Context> sl)
        {
            sl->setConcreteType(boost::typeindex::type_index(typeid(TImpl)));
            // create boost::shared_ptr around the returned ptr
            auto ptr = m_fn(sl);
            return boost::shared_ptr<TImpl>(ptr);
        }
        boost::function<TImpl*(boost::shared_ptr<Context>)> m_fn;
    };

    template <class TImpl>
    struct Functor13
    {
        Functor13(const std::string& name) :
            m_name(name)
        {
        }

        boost::function<boost::shared_ptr<TImpl> > operator()(boost::shared_ptr<Context> sl)
        {
            return sl->resolve<TImpl>(m_name);
        }

        const std::string& m_name;
    };

    template <class TImpl>
    struct Functor14
    {
        boost::function<boost::shared_ptr<TImpl> > operator()(boost::shared_ptr<Context> sl)
        {
            return sl->resolve<TImpl>(sl->getName());
        }
    };

    template <class TImpl>
    struct Functor15
    {
        Functor15(const std::string& name) :
            m_name(name)
        {
        }

        boost::function<boost::shared_ptr<TImpl> > operator()(boost::shared_ptr<Context> sl)
        {
            return sl->resolve<TImpl>(m_name);
        }

        const std::string& m_name;
    };


    // Hide default constructor - client should call ::create which returns a shared_ptr version
    ServiceLocator() :
        m_parent()
    {
    }

    // Child locators keep a shared_ptr to their parent
    ServiceLocator(boost::shared_ptr<ServiceLocator> parent) :
        m_parent(parent)
    {
    }

    // Resolve a named interface, throws if not able to resolve
    template <class IFace>
    boost::shared_ptr<IFace> m_resolve(boost::shared_ptr<Context> slc) {
        auto& name = slc->getName();
        auto nsl = getTypedServiceLocator<IFace>(false);
        if (nsl == NULL) {
            if (m_parent == NULL) {
                throw UnableToResolveException(std::string("Unable to resolve <") + slc->getInterfaceTypeName() + ">  resolve path = " + slc->getResolvePath());
            }
            return m_parent->m_resolve<IFace>(slc);
        }

        auto ptr = nsl->tryResolve(name, slc);
        if (ptr == NULL) {
            if (m_parent == NULL) {
                throw UnableToResolveException(std::string("Unable to resolve <") + slc->getInterfaceTypeName() + ">  resolve path = " + slc->getResolvePath());
            }
            return m_parent->m_resolve<IFace>(slc);
        }

        return ptr;
    }

    // Resolve a named interface, throws if not able to resolve
    template <class IFace>
    void _visitAll(boost::function<void(boost::shared_ptr<typename TypedServiceLocator<IFace>::shared_ptr_binding>)> fnVisit) {
        auto nsl = getTypedServiceLocator<IFace>(false);
        if (nsl != NULL) {
            nsl->visitAll(fnVisit);
        }

        if (m_parent != NULL) {
            m_parent->_visitAll<IFace>(fnVisit);
        }
    }

    template <class IFace>
    bool _canResolve(boost::shared_ptr<Context> slc) {
        auto nsl = getTypedServiceLocator<IFace>(false);
        if (nsl == NULL) {
            if (m_parent == NULL) {
                return false;
            }

            return m_parent->_canResolve<IFace>(slc);
        }

        return nsl->canResolve(slc->getName());
    }

    // Try to resolve a named interface, returns NULL on failure
    template <class IFace>
    boost::shared_ptr<IFace> m_tryResolve(boost::shared_ptr<Context> slc) {
        auto nsl = getTypedServiceLocator<IFace>(false);
        if (nsl == NULL) {
            if (m_parent == NULL) {
                boost::shared_ptr<IFace> nullp;
                return nullp;
            }

            return m_parent->m_tryResolve<IFace>(slc);
        }

        auto ptr = nsl->tryResolve(slc->getName(), slc);
        if (ptr == NULL && m_parent != NULL) {
            return m_parent->m_tryResolve<IFace>(slc);
        }
        return ptr;
    }

public:
    // Create a root ServiceLocator
    static boost::shared_ptr<ServiceLocator> create() {
        auto slp = boost::shared_ptr<ServiceLocator>(new ServiceLocator());

        // Keep a weak reference to ourselves (weird) - have to do this in order to be able to
        // create children which have shared_ptr's to their parents - you cannot create 2 shared_ptr
        // instances from a raw pointer you will crash on 2nd shared_ptr going out of scope and deleting
        // the instance which has already been deleted by the 1st shared_ptr going out of scope
        slp->m_this = slp;
        slp->m_context = boost::shared_ptr<Context>(new Context(slp));

        return slp;
    }

    virtual ~ServiceLocator() {
    }

    // Create a child ServiceLocator.  Children can override parent bindings or add new ones (they cannot delete
    // a parent binding).  Children will revert to their parents for unresolved bindings
    boost::shared_ptr<ServiceLocator> enter() {
        auto slp = boost::shared_ptr<ServiceLocator>(new ServiceLocator(boost::shared_ptr<ServiceLocator>(m_this)));
        slp->m_this = slp;
        slp->m_context = boost::shared_ptr<Context>(new Context(slp));
        return slp;
    }

    // Create a named binding
    template <class IFace>
    typename TypedServiceLocator<IFace>::shared_ptr_binding::to_clause& bind(const std::string& named) {
        auto nsl = getTypedServiceLocator<IFace>(true);

        return nsl->bind(named, &m_eagerBindings);
    }

    // Create a binding
    template <class IFace>
    typename TypedServiceLocator<IFace>::shared_ptr_binding::to_clause& bind() {
        auto nsl = getTypedServiceLocator<IFace>(true);

        return nsl->bind("", &m_eagerBindings);
    }

    boost::shared_ptr<Context> getContext() const {
        if (m_eagerBindings.size() > 0) {
            for(auto eagerBinding  = m_eagerBindings.begin(); eagerBinding != m_eagerBindings.end(); ++eagerBinding) {
                (*eagerBinding)->eagerBind(m_context);
            }
            m_eagerBindings.clear();
        }
        return m_context;
    }


    class module_clause;

    // Derive from this to create custom Modules to load from
    class Module {
        friend class module_clause;

    protected:
        boost::shared_ptr<ServiceLocator> m_sl;

    public:
        // Create a named binding
        template <class IFace>
        typename TypedServiceLocator<IFace>::shared_ptr_binding::to_clause& bind(const std::string& named) {
            return m_sl->bind<IFace>(named);
        }

        // Create a binding
        template <class IFace>
        typename TypedServiceLocator<IFace>::shared_ptr_binding::to_clause& bind() {
            return m_sl->bind<IFace>();
        }

    public:
        virtual void load() = 0;
    };


    // Simple module loading syntax
    // modules().add<MyModule>().add<MyOtherModule>()....

    class module_clause {
    private:
        mutable boost::shared_ptr<ServiceLocator> m_sl;

    public:
        module_clause(boost::shared_ptr<ServiceLocator> sl) :
            m_sl(sl)
        {
        }

        template <class TModule>
        module_clause& add() {
            auto module = std::unique_ptr<TModule>(new TModule());
            module->m_sl = m_sl;

            module->load();

            return *this;
        }

        module_clause& add(ServiceLocator::Module&& module) {
            module.m_sl = m_sl;

            module.load();

            return *this;
        }

        module_clause& add(ServiceLocator::Module& module) {
            module.m_sl = m_sl;

            module.load();

            return *this;
        }

    };

    boost::shared_ptr<module_clause> m_module_clause;
    module_clause& modules() {
        if (m_module_clause == NULL) {
            m_module_clause = boost::shared_ptr<module_clause>(new module_clause(boost::shared_ptr<ServiceLocator>(m_this)));
        }
        return *m_module_clause;
    }


    // This function is used to allow ServiceLocator to "manage" objects which are actually owned outside
    // of ServiceLocator's realm (ie ServiceLocator should not be able to delete the instance through shared_ptr
    // reference decrementing to 0)
    //
    // This is useful when something else owns an object we need to Inject, ServiceLocator always returns
    // shared_ptr<IFace> objects.  In order to make ServiceLocator not delete the alien instance, bind like so
    //
    // bind<IFace>().toInstance(boost::shared_ptr<IFace>(alienObj, ServiceLocator::NoDelete));
    //
    static void NoDelete(const void*) {
    }
};

typedef boost::shared_ptr<ServiceLocator::Context> SLContext_sptr;

#endif /* ServiceLocator_hpp */
