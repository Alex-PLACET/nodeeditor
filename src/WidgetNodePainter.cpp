#include "WidgetNodePainter.hpp"

#include <cmath>


#include <QtNodes/NodeColors.hpp>
#include <QtNodes/AbstractGraphModel>
#include <QtNodes/internal/AbstractNodeGeometry.hpp>
#include <QtNodes/internal/NodeStyle.hpp>
#include <QtNodes/NodeData>
#include <QtNodes/internal/NodeState.hpp>
#include "AbstractGraphModel.hpp"
#include "BasicGraphicsScene.hpp"
#include "ConnectionGraphicsObject.hpp"
#include "ConnectionIdUtils.hpp"
#include "NodeGraphicsObject.hpp"
#include <QApplication>

QtNodes::WidgetNodePainter::WidgetNodePainter() : AbstractNodePainter() {
}

void QtNodes::WidgetNodePainter::paint(QPainter *painter, NodeGraphicsObject &ngo) const
{
    drawNodeBackground(painter, ngo);
    drawNodeCaption(painter, ngo);
    drawNodeBoundary(painter, ngo);
    drawConnectionPoints(painter, ngo);
    drawFilledConnectionPoints(painter, ngo);
    drawResizeRect(painter, ngo);
}

void QtNodes::WidgetNodePainter::drawNodeBackground(QPainter *painter, NodeGraphicsObject &ngo) const {
    const AbstractGraphModel &model = ngo.graphModel();
    const NodeId nodeId = ngo.nodeId();
    const AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();
    const QSize size = geometry.size(nodeId);
    const QJsonDocument json = QJsonDocument::fromVariant(model.nodeData(nodeId, NodeRole::Style));
    const NodeStyle nodeStyle(json.object());
    const QPen p(nodeStyle.NormalBoundaryColor, 0);
    painter->setPen(p);

    QLinearGradient gradient(QPointF(0.0, 0.0), QPointF(0, size.height()));
    const QPalette palette = QApplication::palette();
    gradient.setColorAt(0.0, palette.color(QPalette::Base));
    gradient.setColorAt(0.10, palette.color(QPalette::AlternateBase));
    gradient.setColorAt(0.90, palette.color(QPalette::AlternateBase));
    gradient.setColorAt(1.0, palette.color(QPalette::Window));
    painter->setBrush(gradient);
    const QRectF boundary(0, 0, size.width(), size.height());
    constexpr double radius = 3.0;
    painter->drawRoundedRect(boundary, radius, radius);
}

void QtNodes::WidgetNodePainter::drawNodeBoundary(QPainter *painter, NodeGraphicsObject &ngo) const {
    const QColor col = ngo.isSelected() ? QColor(255, 255, 255) : QColor(0, 0, 0);
    const int width = ngo.isSelected() ? 2 : 0;
    const NodeId nodeId = ngo.nodeId();
    const AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();
    const QSize size = geometry.size(nodeId);
    const QRectF boundary(0, 0, size.width(), size.height());

    static const QBrush brush(Qt::transparent);
    painter->setBrush(brush);
    QPen borderPen(col);
    borderPen.setWidth(width);
    painter->setPen(borderPen);
    constexpr double radius = 3.0;
    painter->drawRoundedRect(boundary, radius, radius);
}

void QtNodes::WidgetNodePainter::drawConnectionPoints(QPainter *painter, NodeGraphicsObject &ngo) const {
    const AbstractGraphModel &model = ngo.graphModel();
    const NodeId nodeId = ngo.nodeId();
    const AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();
    const QJsonDocument json = QJsonDocument::fromVariant(model.nodeData(nodeId, NodeRole::Style));
    const NodeStyle nodeStyle(json.object());
    const float diameter = nodeStyle.ConnectionPointDiameter;
    const auto reducedDiameter = diameter * 0.6;

    for (PortType portType: {PortType::Out, PortType::In}) {
        const size_t n = model
                             .nodeData(nodeId,
                                       (portType == PortType::Out) ? NodeRole::OutPortCount
                                                                   : NodeRole::InPortCount)
                             .toUInt();

        for (PortIndex portIndex = 0; portIndex < n; ++portIndex) {
            const QPointF p = geometry.portPosition(nodeId, portType, portIndex);
            auto const &dataType = model.portData(nodeId, portType, portIndex, PortRole::DataType)
                    .value<NodeDataType>();

            double r = 1.0;
            const NodeState &state = ngo.nodeState();
            if (auto const *cgo = state.connectionForReaction()) {
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
            painter->setPen({0, 0, 0});
            painter->setBrush(dataType.color);

            painter->drawEllipse(p, reducedDiameter * r, reducedDiameter * r);
        }
    }

    if (ngo.nodeState().connectionForReaction()) {
        ngo.nodeState().resetConnectionForReaction();
    }
}

void QtNodes::WidgetNodePainter::drawFilledConnectionPoints(QPainter *painter, NodeGraphicsObject &ngo) const {
    const AbstractGraphModel &model = ngo.graphModel();
    const NodeId nodeId = ngo.nodeId();
    const AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();
    const QJsonDocument json = QJsonDocument::fromVariant(model.nodeData(nodeId, NodeRole::Style));
    const NodeStyle nodeStyle(json.object());
    const auto diameter = nodeStyle.ConnectionPointDiameter;

    for (PortType portType: {PortType::Out, PortType::In}) {
        size_t const n = model
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

                painter->setPen(dataType.color);
                painter->setBrush(dataType.color);
                painter->drawEllipse(p, diameter * 0.4, diameter * 0.4);
            }
        }
    }
}

void QtNodes::WidgetNodePainter::drawNodeCaption(QPainter *painter, NodeGraphicsObject &ngo) const {
    const AbstractGraphModel &model = ngo.graphModel();
    const NodeId const nodeId = ngo.nodeId();
    const AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();

    if (!model.nodeData(nodeId, NodeRole::CaptionVisible).toBool()) {
        return;
    }

    const QString name = model.nodeData(nodeId, NodeRole::Caption).toString();

    QFont f = painter->font();
    f.setBold(true);

    QPointF position = geometry.captionPosition(nodeId);
    position.setX(10);
    position.setY(16);

    const QJsonDocument json = QJsonDocument::fromVariant(model.nodeData(nodeId, NodeRole::Style));
    const NodeStyle nodeStyle(json.object());
    const QSize size = geometry.size(nodeId);
    const QRectF boundary(0, 0, size.width(), 22);
    const QColor captionCol = NodeColors::getColor(
        model.nodeData(nodeId, NodeRole::Type).value<QString>());
    painter->setPen(captionCol);
    constexpr double radius = 3.0;
    painter->setBrush(QBrush(captionCol));
    painter->drawRoundedRect(boundary, radius, radius);
    painter->setFont(f);
    painter->setPen(nodeStyle.FontColor);
    painter->drawText(position, name);
    f.setBold(false);
    painter->setFont(f);
}

void QtNodes::WidgetNodePainter::drawResizeRect(QPainter *painter, NodeGraphicsObject &ngo) const {
    const AbstractGraphModel &model = ngo.graphModel();
    const NodeId nodeId = ngo.nodeId();
    const AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();

    if (model.nodeFlags(nodeId) & NodeFlag::Resizable) {
        painter->setBrush(Qt::gray);
        painter->drawEllipse(geometry.resizeHandleRect(nodeId));
    }
}
