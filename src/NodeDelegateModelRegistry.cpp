#include "NodeDelegateModelRegistry.hpp"

#include <QtCore/QFile>
#include <QtWidgets/QMessageBox>

using QtNodes::NodeDataType;
using QtNodes::NodeDelegateModel;
using QtNodes::NodeDelegateModelRegistry;

std::unique_ptr<NodeDelegateModel> NodeDelegateModelRegistry::create(QString const &modelName)
{
    const auto it = _registeredItemCreators.find(modelName);
    if (it != _registeredItemCreators.end()) {
        return it->second(_convertersRegister);
    }
    return nullptr;
}

const NodeDelegateModelRegistry::RegisteredModelCreatorsMap &
NodeDelegateModelRegistry::registeredModelCreators() const
{
    return _registeredItemCreators;
}

const NodeDelegateModelRegistry::RegisteredModelsCategoryMap &
NodeDelegateModelRegistry::registeredModelsCategoryAssociation() const
{
    return _registeredModelsCategory;
}

const NodeDelegateModelRegistry::CategoriesSet &NodeDelegateModelRegistry::categories() const
{
    return _categories;
}

const std::shared_ptr<QtNodes::ConvertersRegister> &NodeDelegateModelRegistry::convertersRegister()
{
    return _convertersRegister;
}
