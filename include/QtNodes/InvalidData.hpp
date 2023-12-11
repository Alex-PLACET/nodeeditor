#pragma once

#include <QtNodes/NodeData>

namespace QtNodes {
    class InvalidData final : public NodeData {
    public:
        InvalidData() = default;

        [[nodiscard]] NodeDataType type() const override {
            return NodeDataType{
                "invalid", "Invalid", {0, 0, 0} // TODO: use a different color
            };
        }

        [[nodiscard]] bool empty() const override { return true; }
    };
} // QtNodes
