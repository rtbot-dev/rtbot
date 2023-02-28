import React, { useCallback, useRef } from "react";
import { nanoid } from "nanoid";
import ReactFlow, {
  useNodesState,
  useEdgesState,
  addEdge,
  useReactFlow,
  ReactFlowProvider,
  Position,
  Edge,
  Connection,
  XYPosition,
} from "reactflow";
import "reactflow/dist/style.css";

import "./reactflow.css";
import { OperatorNode } from "./OperatorNode";

const getNode = ({ x, y }: XYPosition, removeNode: (id: string) => void) => {
  const id = nanoid(4);
  return {
    id,
    sourcePosition: Position.Right,
    targetPosition: Position.Left,
    type: "operatorNode",
    data: {
      id,
      menu: {
        remove() {
          removeNode(id);
        },
      },
    },
    position: { x: 0, y: 50 },
  };
};

const nodeTypes = {
  operatorNode: OperatorNode,
};

const getId = () => nanoid(3);

const fitViewOptions = {
  padding: 3,
};

const AddNodeOnEdgeDrop = () => {
  const reactFlowWrapper = useRef(null);
  const connectingNodeId = useRef(null);
  const [nodes, setNodes, onNodesChange] = useNodesState([]);
  const [edges, setEdges, onEdgesChange] = useEdgesState([]);
  const removeNode = (id: string) => setNodes((nds) => nds.filter((n) => n.data.id !== id));

  // always keep at least 1 node in the stage
  if (nodes.length === 0) {
    const defaultNode = getNode({ x: 0, y: 50 }, removeNode);
    setNodes([defaultNode]);
  }
  const { project } = useReactFlow();
  const onConnect = useCallback((params: Edge | Connection) => setEdges((eds) => addEdge(params, eds)), []);

  const onConnectStart = useCallback((_, { nodeId }) => {
    connectingNodeId.current = nodeId;
  }, []);

  const addNodeOnConnectEnd = (event) => {
    console.log("Adding node", event);
    const targetIsPane = event.target.classList.contains("react-flow__pane");

    if (targetIsPane) {
      // we need to remove the wrapper bounds, in order to get the correct position
      const { top, left } = (reactFlowWrapper as any).current.getBoundingClientRect();
      const { id } = addNode({ x: event.clientX - left - 75, y: event.clientY - top });
      setEdges((eds) => [...eds, { id, source: connectingNodeId.current as unknown as string, target: id }]);
    }
  };

  const addNode = ({ x, y }: XYPosition) => {
    const id = getId();
    const newNode = {
      id,
      sourcePosition: Position.Right,
      targetPosition: Position.Left,
      type: "operatorNode",
      // we are removing the half of the node width (75) to center the new node
      position: project({ x, y }),
      data: {
        id,
        menu: {
          remove() {
            removeNode(id);
          },
        },
      },
    };

    setNodes((nds) => [...nds, newNode]);
    return newNode;
  };

  return (
    <div className="wrapper" ref={reactFlowWrapper}>
      <ReactFlow
        nodes={nodes}
        edges={edges}
        //nodesDraggable={false}
        //panOnDrag={false}
        onNodesChange={onNodesChange}
        onEdgesChange={onEdgesChange}
        onConnect={onConnect}
        onConnectStart={onConnectStart}
        onConnectEnd={addNodeOnConnectEnd}
        nodeTypes={nodeTypes}
        fitView
        fitViewOptions={fitViewOptions}
      />
    </div>
  );
};

export const GraphEditor = () => (
  <ReactFlowProvider>
    <AddNodeOnEdgeDrop />
  </ReactFlowProvider>
);
