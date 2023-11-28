#include "AbstractNodeGeometry.hpp"

#include "AbstractGraphModel.hpp"
#include "StyleCollection.hpp"

#include <QMargins>

#include <cmath>

namespace QtNodes {

AbstractNodeGeometry::AbstractNodeGeometry(AbstractGraphModel &graphModel)
    : _graphModel(graphModel)
{
    //
}

QRectF AbstractNodeGeometry::boundingRect(NodeId const nodeId) const
{
    const QSize s = size(nodeId);
    constexpr double ratio = 0.05;
    const int widthMargin = s.width() * ratio;
    const int heightMargin = s.height() * ratio;
    const QMargins margins(widthMargin, heightMargin, widthMargin, heightMargin);
    const QRectF r(QPointF(0, 0), s);
    return r.marginsAdded(margins);
}

QPointF AbstractNodeGeometry::portScenePosition(NodeId const nodeId,
                                                PortType const portType,
                                                PortIndex const index,
                                                QTransform const &t) const
{
    const QPointF result = portPosition(nodeId, portType, index);
    return t.map(result);
}

PortIndex AbstractNodeGeometry::checkPortHit(NodeId const nodeId,
                                             PortType const portType,
                                             QPointF const nodePoint) const
{
    const auto &nodeStyle = StyleCollection::nodeStyle();
    PortIndex result = InvalidPortIndex;

    if (portType == PortType::None) {
        return result;
    }

    const double tolerance = 2.0 * nodeStyle.ConnectionPointDiameter;
    const size_t n = _graphModel.nodeData<unsigned int>(nodeId,
                                                        (portType == PortType::Out)
                                                            ? NodeRole::OutPortCount
                                                            : NodeRole::InPortCount);

    for (unsigned int portIndex = 0; portIndex < n; ++portIndex) {
        const auto pp = portPosition(nodeId, portType, portIndex);
        const QPointF p = pp - nodePoint;
        const auto distance = std::sqrt(QPointF::dotProduct(p, p));
        if (distance < tolerance) {
            result = portIndex;
            break;
        }
    }

    return result;
}

} // namespace QtNodes
