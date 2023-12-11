#include "NodeConnectionInteraction.hpp"

#include "AbstractNodeGeometry.hpp"
#include "BasicGraphicsScene.hpp"
#include "ConnectionGraphicsObject.hpp"
#include "ConnectionIdUtils.hpp"
#include "NodeGraphicsObject.hpp"
#include "UndoCommands.hpp"

#include <QtCore/QDebug>

#include <QUndoStack>

namespace QtNodes {

NodeConnectionInteraction::NodeConnectionInteraction(NodeGraphicsObject &ngo,
                                                     ConnectionGraphicsObject &cgo,
                                                     BasicGraphicsScene &scene)
    : _ngo(ngo)
    , _cgo(cgo)
    , _scene(scene)
{}


bool dfs(AbstractGraphModel &model, NodeId currentNode, NodeId targetNode, std::unordered_map<NodeId, bool>& visited) {

    if (currentNode == targetNode) {
        // Target node reached, cycle found
        return true;
    }
    if(visited[currentNode]) {
        return false;
    }
    visited[currentNode] = true;

    const PortCount nOutPorts = model.nodeData<PortCount>(currentNode, NodeRole::OutPortCount);
    for (PortIndex index = 0; index < nOutPorts; ++index) {
        const auto &outConnectionIds = model.connections(currentNode, PortType::Out, index);
        for (const auto &connection : outConnectionIds) {
            const NodeId neighbour = connection.inNodeId;
            if (!visited[neighbour]) {
                if (dfs(model, neighbour, targetNode, visited)) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool NodeConnectionInteraction::introducesCycle(AbstractGraphModel &model, NodeId sourceNode, NodeId targetNode) const {
    // Mark all nodes as not visited
    std::unordered_map<NodeId, bool> visited;
    for (auto& id : model.allNodeIds()) {
        visited[id] = false;
    }

    // Perform DFS from the target node
    return dfs(model, sourceNode, targetNode, visited);
}


bool NodeConnectionInteraction::canConnect(PortIndex *portIndex) const
{
    // 1. Connection requires a port.

    const PortType requiredPort = _cgo.connectionState().requiredPort();

    if (requiredPort == PortType::None) {
        return false;
    }

    const NodeId connectedNodeId = getNodeId(oppositePort(requiredPort), _cgo.connectionId());

    // 2. Forbid connecting the node to itself.

    if (_ngo.nodeId() == connectedNodeId) {
        return false;
    }

    AbstractGraphModel &model = _ngo.nodeScene()->graphModel();

    // 3. Forbid connections that introduce cycles
    auto srcId = _ngo.nodeId();
    auto targetId = connectedNodeId;
    if(_cgo.connectionId().outNodeId == InvalidNodeId) {
        std::swap(srcId, targetId);
    }
    if(introducesCycle(model, srcId, targetId)) {
        return false;
    }

    // 4. Connection loose end is above the node port.

    const QPointF connectionPoint = _cgo.sceneTransform().map(_cgo.endPoint(requiredPort));

    *portIndex = nodePortIndexUnderScenePoint(requiredPort, connectionPoint);

    if (*portIndex == InvalidPortIndex) {
        return false;
    }

    // 5. Model allows connection.

    const ConnectionId connectionId = makeCompleteConnectionId(_cgo.connectionId(), // incomplete
                                                               _ngo.nodeId(), // missing node id
                                                               *portIndex);   // missing port index

    return model.connectionPossible(connectionId);
}

bool NodeConnectionInteraction::tryConnect() const
{
    // 1. Check conditions from 'canConnect'.

    PortIndex targetPortIndex = InvalidPortIndex;
    if (!canConnect(&targetPortIndex)) {
        return false;
    }

    // 2. Remove existing connections to the port
    const AbstractGraphModel &model = _ngo.nodeScene()->graphModel();

    auto srcId = _ngo.nodeId();
    if(_cgo.connectionId().outNodeId == InvalidNodeId) {
        srcId = getNodeId(oppositePort(_cgo.connectionState().requiredPort()), _cgo.connectionId());
    }

    const auto connected = model.connections(srcId, PortType::In, targetPortIndex);
    if(!connected.empty()) {
        for(auto conId : connected) {
            _scene.undoStack().push(new DisconnectCommand(&_scene, conId));
        }
    }

    // 3. Create new connection.

    const ConnectionId incompleteConnectionId = _cgo.connectionId();
    const ConnectionId newConnectionId = makeCompleteConnectionId(incompleteConnectionId,
                                                                  _ngo.nodeId(),
                                                                  targetPortIndex);

    _ngo.nodeScene()->resetDraftConnection();
    _ngo.nodeScene()->undoStack().push(new ConnectCommand(_ngo.nodeScene(), newConnectionId));

    return true;
}

bool NodeConnectionInteraction::disconnect(PortType portToDisconnect) const
{
    const ConnectionId connectionId = _cgo.connectionId();

    _scene.undoStack().push(new DisconnectCommand(&_scene, connectionId));

    const AbstractNodeGeometry &geometry = _scene.nodeGeometry();

    const QPointF scenePos = geometry.portScenePosition(_ngo.nodeId(),
                                                        portToDisconnect,
                                                        getPortIndex(portToDisconnect, connectionId),
                                                        _ngo.sceneTransform());

    // Converted to "draft" connection with the new incomplete id.
    const ConnectionId incompleteConnectionId = makeIncompleteConnectionId(connectionId,
                                                                           portToDisconnect);

    // Grabs the mouse
    const auto &draftConnection = _scene.makeDraftConnection(incompleteConnectionId);

    const QPointF looseEndPos = draftConnection->mapFromScene(scenePos);
    draftConnection->setEndPoint(portToDisconnect, looseEndPos);

    // Repaint connection points.
    const NodeId connectedNodeId = getNodeId(oppositePort(portToDisconnect), connectionId);
    _scene.nodeGraphicsObject(connectedNodeId)->update();

    const NodeId disconnectedNodeId = getNodeId(portToDisconnect, connectionId);
    _scene.nodeGraphicsObject(disconnectedNodeId)->update();

    return true;
}

// ------------------ util functions below

PortType NodeConnectionInteraction::connectionRequiredPort() const
{
    auto const &state = _cgo.connectionState();
    return state.requiredPort();
}

QPointF NodeConnectionInteraction::nodePortScenePosition(PortType portType,
                                                         PortIndex portIndex) const
{
    const AbstractNodeGeometry &geometry = _scene.nodeGeometry();
    const QPointF p = geometry.portScenePosition(_ngo.nodeId(),
                                                 portType,
                                                 portIndex,
                                                 _ngo.sceneTransform());
    return p;
}

PortIndex NodeConnectionInteraction::nodePortIndexUnderScenePoint(PortType portType,
                                                                  QPointF const &scenePoint) const
{
    const AbstractNodeGeometry &geometry = _scene.nodeGeometry();
    const QTransform sceneTransform = _ngo.sceneTransform();
    const QPointF nodePoint = sceneTransform.inverted().map(scenePoint);
    return geometry.checkPortHit(_ngo.nodeId(), portType, nodePoint);
}

} // namespace QtNodes
