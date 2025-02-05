#pragma once

#include "ConvertersRegister.hpp"
#include "Export.hpp"
#include "NodeDelegateModel.hpp"
#include <QtCore/QString>

#include <functional>
#include <memory>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <QtNodes/ConvertersRegister>

namespace QtNodes {

/// Class uses map for storing models (name, model)
class NODE_EDITOR_PUBLIC NodeDelegateModelRegistry
{
public:
    using RegistryItemPtr = std::unique_ptr<NodeDelegateModel>;
    using RegistryItemCreator
        = std::function<RegistryItemPtr(const std::shared_ptr<const ConvertersRegister> &)>;
    using RegisteredModelCreatorsMap = std::unordered_map<QString, RegistryItemCreator>;
    using RegisteredModelsCategoryMap = std::unordered_map<QString, QString>;
    using CategoriesSet = std::set<QString>;

    //using RegisteredTypeConvertersMap = std::map<TypeConverterId, TypeConverter>;

    NodeDelegateModelRegistry() = default;
    ~NodeDelegateModelRegistry() = default;

    NodeDelegateModelRegistry(NodeDelegateModelRegistry const &) = delete;
    NodeDelegateModelRegistry(NodeDelegateModelRegistry &&) = default;

    NodeDelegateModelRegistry &operator=(NodeDelegateModelRegistry const &) = delete;

    NodeDelegateModelRegistry &operator=(NodeDelegateModelRegistry &&) = default;

public:
    template<typename ModelType>
    void registerModel(RegistryItemCreator creator, const QString &category = "Nodes")
    {
        const QString name = computeName<ModelType>(HasStaticMethodName<ModelType>{}, creator);
        if (!_registeredItemCreators.contains(name)) {
            _registeredItemCreators[name] = std::move(creator);
            _categories.insert(category);
            _registeredModelsCategory[name] = category;
        } // TODO: Error handling
    }

    template<typename ModelType>
    void registerModel(QString const &category = "Nodes")
    {
        RegistryItemCreator creator = []() { return std::make_unique<ModelType>(); };
        registerModel<ModelType>(std::move(creator), category);
    }

#if 0
  template<typename ModelType>
  void
  registerModel(RegistryItemCreator creator,
                QString const&      category = "Nodes")
  {
    registerModel<ModelType>(std::move(creator), category);
  }


  template <typename ModelCreator>
  void
  registerModel(ModelCreator&& creator, QString const& category = "Nodes")
  {
    using ModelType = compute_model_type_t<decltype(creator())>;
    registerModel<ModelType>(std::forward<ModelCreator>(creator), category);
  }


  template <typename ModelCreator>
  void
  registerModel(QString const& category, ModelCreator&& creator)
  {
    registerModel(std::forward<ModelCreator>(creator), category);
  }


  void
  registerTypeConverter(TypeConverterId const& id,
                        TypeConverter          typeConverter)
  {
    _registeredTypeConverters[id] = std::move(typeConverter);
  }

#endif

    std::unique_ptr<NodeDelegateModel> create(QString const &modelName);

    const RegisteredModelCreatorsMap &registeredModelCreators() const;

    const RegisteredModelsCategoryMap &registeredModelsCategoryAssociation() const;

    const CategoriesSet &categories() const;

    const std::shared_ptr<ConvertersRegister> &convertersRegister();

#if 0
  TypeConverter
  getTypeConverter(NodeDataType const& d1,
                   NodeDataType const& d2) const;
#endif

private:
    RegisteredModelsCategoryMap _registeredModelsCategory;

    CategoriesSet _categories;

    RegisteredModelCreatorsMap _registeredItemCreators;

    std::shared_ptr<ConvertersRegister> _convertersRegister = std::make_shared<ConvertersRegister>();

#if 0
  RegisteredTypeConvertersMap _registeredTypeConverters;
#endif

private:
    // If the registered ModelType class has the static member method
    // `static QString Name();`, use it. Otherwise use the non-static
    // method: `virtual QString name() const;`
    template<typename T, typename = void>
    struct HasStaticMethodName : std::false_type
    {};

    template<typename T>
    struct HasStaticMethodName<
        T,
        typename std::enable_if_t<std::is_same<decltype(T::Name()), QString>::value>>
        : std::true_type
    {};

    template<typename ModelType>
    static QString computeName(std::true_type, const RegistryItemCreator &)
    {
        return ModelType::Name();
    }

    template<typename ModelType>
    QString computeName(std::false_type, const RegistryItemCreator &creator) const
    {
        return creator(_convertersRegister)->name();
    }

    template<typename T>
    struct UnwrapUniquePtr
    {
        // Assert always fires, but the compiler doesn't know this:
        static_assert(!std::is_same<T, T>::value,
                      "The ModelCreator must return a std::unique_ptr<T>, where T "
                      "inherits from NodeDelegateModel");
    };

    template<typename T>
    struct UnwrapUniquePtr<std::unique_ptr<T>>
    {
        static_assert(std::is_base_of<NodeDelegateModel, T>::value,
                      "The ModelCreator must return a std::unique_ptr<T>, where T "
                      "inherits from NodeDelegateModel");
        using type = T;
    };

    template<typename CreatorResult>
    using compute_model_type_t = typename UnwrapUniquePtr<CreatorResult>::type;
};

} // namespace QtNodes
