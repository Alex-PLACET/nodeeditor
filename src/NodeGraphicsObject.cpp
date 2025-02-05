#include "NodeGraphicsObject.hpp"

#include <cstdlib>
#include <iostream>

#include <QtWidgets/QGraphicsEffect>
#include <QtWidgets/QtWidgets>

#include "AbstractGraphModel.hpp"
#include "AbstractNodeGeometry.hpp"
#include "AbstractNodePainter.hpp"
#include "BasicGraphicsScene.hpp"
#include "ConnectionGraphicsObject.hpp"
#include "ConnectionIdUtils.hpp"
#include "NodeConnectionInteraction.hpp"
#include "StyleCollection.hpp"
#include "UndoCommands.hpp"

namespace QtNodes {

    NodeGraphicsObject::NodeGraphicsObject(BasicGraphicsScene &scene, NodeId nodeId)
            : _nodeId(nodeId), _graphModel(scene.graphModel()), _nodeState(*this), _proxyWidget(nullptr) {
        scene.addItem(this);

        setFlag(QGraphicsItem::ItemDoesntPropagateOpacityToChildren, true);
        setFlag(QGraphicsItem::ItemIsFocusable, true);
        setLockedState();
        setCacheMode(QGraphicsItem::DeviceCoordinateCache);

        const QJsonObject nodeStyleJson = _graphModel.nodeData(_nodeId, NodeRole::Style)
                                              .toJsonObject();

        const NodeStyle nodeStyle(nodeStyleJson);

        {
            auto effect = new QGraphicsDropShadowEffect;
            effect->setOffset(4, 4);
            effect->setBlurRadius(20);
            effect->setColor(nodeStyle.ShadowColor);
            setGraphicsEffect(effect);
        }

        setOpacity(nodeStyle.Opacity);
        setAcceptHoverEvents(true);
        setZValue(0);
        embedQWidget();
        nodeScene()->nodeGeometry().recomputeSize(_nodeId);
        const QPointF pos = _graphModel.nodeData<QPointF>(_nodeId, NodeRole::Position);
        setPos(pos);

        connect(&_graphModel, &AbstractGraphModel::nodeFlagsUpdated, [this](const NodeId nodeId) {
            if (_nodeId == nodeId) {
                setLockedState();
            }
        });
    }

    AbstractGraphModel &NodeGraphicsObject::graphModel() const {
        return _graphModel;
    }

    BasicGraphicsScene *NodeGraphicsObject::nodeScene() const {
        return dynamic_cast<BasicGraphicsScene *>(scene());
    }

    void NodeGraphicsObject::embedQWidget() {
        const AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();
        geometry.recomputeSize(_nodeId);

        if (auto w = _graphModel.nodeData(_nodeId, NodeRole::Widget).value<QWidget *>()) {
            _proxyWidget = new QGraphicsProxyWidget(this);
            _proxyWidget->setWidget(w);
            _proxyWidget->setPreferredWidth(5);
            geometry.recomputeSize(_nodeId);

            if (w->sizePolicy().verticalPolicy() & QSizePolicy::ExpandFlag) {
                const unsigned int widgetHeight = geometry.size(_nodeId).height()
                                                  - geometry.captionRect(_nodeId).height();

                // If the widget wants to use as much vertical space as possible, set
                // it to have the geom's equivalentWidgetHeight.
                _proxyWidget->setMinimumHeight(widgetHeight);
            }

            _proxyWidget->setPos(geometry.widgetPosition(_nodeId));

            //update();

            _proxyWidget->setOpacity(1.0);
            _proxyWidget->setFlag(QGraphicsItem::ItemIgnoresParentOpacity);
        }
    }

    void NodeGraphicsObject::setLockedState() {
        const NodeFlags flags = _graphModel.nodeFlags(_nodeId);
        const bool locked = flags.testFlag(NodeFlag::Locked);
        setFlag(QGraphicsItem::ItemIsMovable, !locked);
        setFlag(QGraphicsItem::ItemIsSelectable, !locked);
        setFlag(QGraphicsItem::ItemSendsScenePositionChanges, !locked);
    }

    QRectF NodeGraphicsObject::boundingRect() const {
        const AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();
        return geometry.boundingRect(_nodeId);
        //return NodeGeometry(_nodeId, _graphModel, nodeScene()).boundingRect();
    }

    void NodeGraphicsObject::setGeometryChanged() {
        prepareGeometryChange();
    }

    void NodeGraphicsObject::moveConnections() const {
        const auto &connected = _graphModel.allConnectionIds(_nodeId);

        for (auto &cnId: connected) {
            const auto cgo = nodeScene()->connectionGraphicsObject(cnId);
            if (cgo) {
                cgo->move();
            }
        }
    }

    void NodeGraphicsObject::reactToConnection(ConnectionGraphicsObject const *cgo) {
        _nodeState.storeConnectionForReaction(cgo);
        update();
    }

    void NodeGraphicsObject::paint(QPainter *painter, QStyleOptionGraphicsItem const *option, QWidget *) {
        painter->setClipRect(option->exposedRect);
        nodeScene()->nodePainter().paint(painter, *this);
    }

    QVariant NodeGraphicsObject::itemChange(GraphicsItemChange change, const QVariant &value) {
        if (change == ItemScenePositionHasChanged && scene()) {
            moveConnections();
        }
        return QGraphicsObject::itemChange(change, value);
    }

    void NodeGraphicsObject::mousePressEvent(QGraphicsSceneMouseEvent *event) {
        //if (_nodeState.locked())
        //return;
        const AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();

        for (PortType portToCheck: {PortType::In, PortType::Out}) {
            const QPointF nodeCoord = sceneTransform().inverted().map(event->scenePos());
            const PortIndex portIndex = geometry.checkPortHit(_nodeId, portToCheck, nodeCoord);
            if (portIndex == InvalidPortIndex) {
                continue;
            }

            const auto &connected = _graphModel.connections(_nodeId, portToCheck, portIndex);

            // Start dragging existing connection.
            if (!connected.empty() && portToCheck == PortType::In) {
                const auto &cnId = *connected.begin();

                // Need ConnectionGraphicsObject

                const NodeConnectionInteraction interaction(*this,
                                                            *nodeScene()->connectionGraphicsObject(
                                                                cnId),
                                                            *nodeScene());

                if (_graphModel.detachPossible(cnId)) {
                    interaction.disconnect(portToCheck);
                }
            } else // initialize new Connection
            {
                if (portToCheck == PortType::Out) {
                    const auto outPolicy = _graphModel
                                               .portData(_nodeId,
                                                         portToCheck,
                                                         portIndex,
                                                         PortRole::ConnectionPolicyRole)
                                               .value<ConnectionPolicy>();

                    if (!connected.empty() && outPolicy == ConnectionPolicy::One) {
                        for (auto &cnId: connected) {
                            _graphModel.deleteConnection(cnId);
                        }
                    }
                } // if port == out

                const ConnectionId incompleteConnectionId = makeIncompleteConnectionId(_nodeId,
                                                                                       portToCheck,
                                                                                       portIndex);

                nodeScene()->makeDraftConnection(incompleteConnectionId);
            }
        }

        if (_graphModel.nodeFlags(_nodeId) & NodeFlag::Resizable) {
            const auto pos = event->pos();
            const bool hit = geometry.resizeHandleRect(_nodeId).contains(QPoint(pos.x(), pos.y()));
            _nodeState.setResizing(hit);
        }

        QGraphicsObject::mousePressEvent(event);

        if (isSelected()) {
            Q_EMIT nodeScene()->nodeSelected(_nodeId);
        }
    }

    void NodeGraphicsObject::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
    {
        // Deselect all other items after this one is selected.
        // Unless we press a CTRL button to add the item to the selected group before
        // starting moving.
        if (!isSelected()) {
            if (!event->modifiers().testFlag(Qt::ControlModifier)) {
                scene()->clearSelection();
            }
            setSelected(true);
        }

        if (_nodeState.resizing()) {
            auto diff = event->pos() - event->lastPos();
            if (auto w = _graphModel.nodeData<QWidget *>(_nodeId, NodeRole::Widget)) {
                prepareGeometryChange();
                auto oldSize = w->size();
                oldSize += QSize(diff.x(), diff.y());
                w->resize(oldSize);
                const AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();
                // Passes the new size to the model.
                geometry.recomputeSize(_nodeId);
                update();
                moveConnections();
                event->accept();
            }
        } else {
            const auto diff = event->pos() - event->lastPos();
            nodeScene()->undoStack().push(new MoveNodeCommand(nodeScene(), diff));
            event->accept();
        }
        QRectF r = nodeScene()->sceneRect();
        r = r.united(mapToScene(boundingRect()).boundingRect());
        nodeScene()->setSceneRect(r);
    }

    void NodeGraphicsObject::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
        _nodeState.setResizing(false);
        QGraphicsObject::mouseReleaseEvent(event);
        // position connections precisely after fast node move
        moveConnections();
        nodeScene()->nodeClicked(_nodeId);
    }

    void NodeGraphicsObject::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
    {
        _nodeState.setHovered(true);
        setCursor(Qt::ArrowCursor);
        update();
        Q_EMIT nodeScene()->nodeHovered(_nodeId, event->screenPos());
        event->accept();
    }

    void NodeGraphicsObject::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
        _nodeState.setHovered(false);
        setZValue(0.0);
        update();
        Q_EMIT nodeScene()->nodeHoverLeft(_nodeId);
        event->accept();
    }

    void NodeGraphicsObject::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
        const AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();
        const QPointF nodeCoord = sceneTransform().inverted().map(event->scenePos());
        const PortIndex portIndex = geometry.checkPortHit(_nodeId, PortType::Out, nodeCoord);

        if (portIndex != InvalidPortIndex) {
            if (auto nodeData = _graphModel.portData<std::shared_ptr<NodeData>>(nodeId(), PortType::Out, portIndex,
                                                                                PortRole::Data)) {
                QPoint pos = QCursor::pos();
                pos.setY(pos.y() - 32);
                pos.setX(pos.x() + 16);
                QToolTip::showText(pos, nodeData->getDescription());
                return;
            }
        }

        event->accept();
    }

    void NodeGraphicsObject::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
        QGraphicsItem::mouseDoubleClickEvent(event);
        Q_EMIT nodeScene()->nodeDoubleClicked(_nodeId);
    }

    void NodeGraphicsObject::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
        Q_EMIT nodeScene()->nodeContextMenu(_nodeId, mapToScene(event->pos()));
    }

} // namespace QtNodes
