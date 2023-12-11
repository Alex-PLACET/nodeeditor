#include "DefaultNodePainter.hpp"

#include <cmath>

#include <QtCore/QMargins>

#include "AbstractGraphModel.hpp"
#include "AbstractNodeGeometry.hpp"
#include "BasicGraphicsScene.hpp"
#include "ConnectionGraphicsObject.hpp"
#include "ConnectionIdUtils.hpp"
#include "NodeGraphicsObject.hpp"
#include "NodeState.hpp"
#include "StyleCollection.hpp"

namespace QtNodes {

void DefaultNodePainter::paint(QPainter *painter, NodeGraphicsObject &ngo) const
{
    // TODO?
    //AbstractNodeGeometry & geometry = ngo.nodeScene()->nodeGeometry();
    //geometry.recomputeSizeIfFontChanged(painter->font());

    drawNodeRect(painter, ngo);
    drawConnectionPoints(painter, ngo);
    drawFilledConnectionPoints(painter, ngo);
    drawNodeCaption(painter, ngo);
    drawEntryLabels(painter, ngo);
    drawResizeRect(painter, ngo);
}

void DefaultNodePainter::drawNodeRect(QPainter *painter, NodeGraphicsObject &ngo) const
{
    const AbstractGraphModel &model = ngo.graphModel();
    const NodeId nodeId = ngo.nodeId();
    const AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();
    const QSize size = geometry.size(nodeId);
    const QJsonDocument json = QJsonDocument::fromVariant(model.nodeData(nodeId, NodeRole::Style));
    const NodeStyle nodeStyle(json.object());
    const auto color = ngo.isSelected() ? nodeStyle.SelectedBoundaryColor
                                        : nodeStyle.NormalBoundaryColor;

    if (ngo.nodeState().hovered()) {
        const QPen p(color, nodeStyle.HoveredPenWidth);
        painter->setPen(p);
    } else {
        const QPen p(color, nodeStyle.PenWidth);
        painter->setPen(p);
    }

    QLinearGradient gradient(QPointF(0.0, 0.0), QPointF(2.0, size.height()));
    gradient.setColorAt(0.0, nodeStyle.GradientColor0);
    gradient.setColorAt(0.10, nodeStyle.GradientColor1);
    gradient.setColorAt(0.90, nodeStyle.GradientColor2);
    gradient.setColorAt(1.0, nodeStyle.GradientColor3);

    painter->setBrush(gradient);

    const QRectF boundary(0, 0, size.width(), size.height());
    constexpr double radius = 3.0;
    painter->drawRoundedRect(boundary, radius, radius);
}

void DefaultNodePainter::drawConnectionPoints(QPainter *painter, NodeGraphicsObject &ngo) const
{
    const AbstractGraphModel &model = ngo.graphModel();
    const NodeId nodeId = ngo.nodeId();
    const AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();

    const QJsonDocument json = QJsonDocument::fromVariant(model.nodeData(nodeId, NodeRole::Style));
    const NodeStyle nodeStyle(json.object());

    const auto &connectionStyle = StyleCollection::connectionStyle();

    const float diameter = nodeStyle.ConnectionPointDiameter;
    const auto reducedDiameter = diameter * 0.6;

    for (PortType portType : {PortType::Out, PortType::In}) {
        const size_t n = model
                             .nodeData(nodeId,
                                       (portType == PortType::Out) ? NodeRole::OutPortCount
                                                                   : NodeRole::InPortCount)
                             .toUInt();

        for (PortIndex portIndex = 0; portIndex < n; ++portIndex) {
            const QPointF p = geometry.portPosition(nodeId, portType, portIndex);
            const auto &dataType = model.portData(nodeId, portType, portIndex, PortRole::DataType)
                                       .value<NodeDataType>();

            double r = 1.0;
            const NodeState &state = ngo.nodeState();

            if (const auto *cgo = state.connectionForReaction()) {
                const PortType requiredPort = cgo->connectionState().requiredPort();

                if (requiredPort == portType) {
                    const ConnectionId possibleConnectionId
                        = makeCompleteConnectionId(cgo->connectionId(), nodeId, portIndex);

                    const bool possible = model.connectionPossible(possibleConnectionId);

                    auto cp = cgo->sceneTransform().map(cgo->endPoint(requiredPort));
                    cp = ngo.sceneTransform().inverted().map(cp);

                    const auto diff = cp - p;
                    const double dist = std::sqrt(QPointF::dotProduct(diff, diff));

                    if (possible) {
                        constexpr double thres = 40.0;
                        r = (dist < thres) ? (2.0 - dist / thres) : 1.0;
                    } else {
                        constexpr double thres = 80.0;
                        r = (dist < thres) ? (dist / thres) : 1.0;
                    }
                }
            }

            if (connectionStyle.useDataDefinedColors()) {
                painter->setBrush(connectionStyle.normalColor(dataType.id));
            } else {
                painter->setBrush(nodeStyle.ConnectionPointColor);
            }

            painter->drawEllipse(p, reducedDiameter * r, reducedDiameter * r);
        }
    }

    if (ngo.nodeState().connectionForReaction()) {
        ngo.nodeState().resetConnectionForReaction();
    }
}

void DefaultNodePainter::drawFilledConnectionPoints(QPainter *painter, NodeGraphicsObject &ngo) const
{
    const AbstractGraphModel &model = ngo.graphModel();
    const NodeId nodeId = ngo.nodeId();
    const AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();
    const QJsonDocument json = QJsonDocument::fromVariant(model.nodeData(nodeId, NodeRole::Style));
    const NodeStyle nodeStyle(json.object());
    const auto diameter = nodeStyle.ConnectionPointDiameter;

    for (PortType portType : {PortType::Out, PortType::In}) {
        const size_t n = model
                             .nodeData(nodeId,
                                       (portType == PortType::Out) ? NodeRole::OutPortCount
                                                                   : NodeRole::InPortCount)
                             .toUInt();

        for (PortIndex portIndex = 0; portIndex < n; ++portIndex) {
            const QPointF p = geometry.portPosition(nodeId, portType, portIndex);

            const auto &connected = model.connections(nodeId, portType, portIndex);

            if (!connected.empty()) {
                const auto &dataType = model
                                           .portData(nodeId, portType, portIndex, PortRole::DataType)
                                           .value<NodeDataType>();

                const auto &connectionStyle = StyleCollection::connectionStyle();
                if (connectionStyle.useDataDefinedColors()) {
                    const QColor c = connectionStyle.normalColor(dataType.id);
                    painter->setPen(c);
                    painter->setBrush(c);
                } else {
                    painter->setPen(nodeStyle.FilledConnectionPointColor);
                    painter->setBrush(nodeStyle.FilledConnectionPointColor);
                }

                painter->drawEllipse(p, diameter * 0.4, diameter * 0.4);
            }
        }
    }
}

void DefaultNodePainter::drawNodeCaption(QPainter *painter, NodeGraphicsObject &ngo) const
{
    const AbstractGraphModel &model = ngo.graphModel();
    const NodeId nodeId = ngo.nodeId();
    const AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();

    if (!model.nodeData(nodeId, NodeRole::CaptionVisible).toBool()) {
        return;
    }

    const QString name = model.nodeData(nodeId, NodeRole::Caption).toString();

    QFont f = painter->font();
    f.setBold(true);

    const QPointF position = geometry.captionPosition(nodeId);

    const QJsonDocument json = QJsonDocument::fromVariant(model.nodeData(nodeId, NodeRole::Style));
    const NodeStyle nodeStyle(json.object());

    painter->setFont(f);
    painter->setPen(nodeStyle.FontColor);
    painter->drawText(position, name);

    f.setBold(false);
    painter->setFont(f);
}

void DefaultNodePainter::drawEntryLabels(QPainter *painter, NodeGraphicsObject &ngo) const
{
    const AbstractGraphModel &model = ngo.graphModel();
    const NodeId nodeId = ngo.nodeId();
    const AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();

    const QJsonDocument json = QJsonDocument::fromVariant(model.nodeData(nodeId, NodeRole::Style));
    const NodeStyle nodeStyle(json.object());

    for (PortType portType : {PortType::Out, PortType::In}) {
        const unsigned int n = model.nodeData<unsigned int>(nodeId,
                                                            (portType == PortType::Out)
                                                                ? NodeRole::OutPortCount
                                                                : NodeRole::InPortCount);

        for (PortIndex portIndex = 0; portIndex < n; ++portIndex) {
            const auto &connected = model.connections(nodeId, portType, portIndex);
            const QPointF p = geometry.portTextPosition(nodeId, portType, portIndex);

            if (connected.empty()) {
                painter->setPen(nodeStyle.FontColorFaded);
            } else {
                painter->setPen(nodeStyle.FontColor);
            }

            QString s;

            if (model.portData<bool>(nodeId, portType, portIndex, PortRole::CaptionVisible)) {
                s = model.portData<QString>(nodeId, portType, portIndex, PortRole::Caption);
            } else {
                const auto portData = model.portData(nodeId,
                                                     portType,
                                                     portIndex,
                                                     PortRole::DataType);
                s = portData.value<NodeDataType>().name;
            }

            painter->drawText(p, s);
        }
    }
}

void DefaultNodePainter::drawResizeRect(QPainter *painter, NodeGraphicsObject &ngo) const
{
    const AbstractGraphModel &model = ngo.graphModel();
    const NodeId nodeId = ngo.nodeId();
    const AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();

    if (model.nodeFlags(nodeId) & NodeFlag::Resizable) {
        painter->setBrush(Qt::gray);
        painter->drawEllipse(geometry.resizeHandleRect(nodeId));
    }
}

} // namespace QtNodes
