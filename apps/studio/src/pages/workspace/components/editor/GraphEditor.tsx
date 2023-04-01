import React, { useCallback, useLayoutEffect, useRef, useState } from "react";
import { nanoid } from "nanoid";
import ReactFlow, { Connection, Edge, Position, ReactFlowProvider, useReactFlow, XYPosition } from "reactflow";
import "reactflow/dist/style.css";

import "./reactflow.css";
import { OperatorNode } from "./OperatorNode";
import editor from "@/store/editor";
import { RunBtn } from "./RunBtn";

const getNode = ({ x, y }: XYPosition) => {
  const id = nanoid(4);
  return {
    id,
    sourcePosition: Position.Right,
    targetPosition: Position.Left,
    type: "operatorNode",
    data: {
      parameters: null,
    },
    position: { x, y },
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
  const connectingNodeId = useRef<string | null>(null);
  const draggingNodeId = useRef<string | null>(null);

  const [state, setState] = useState(editor.getState());
  useLayoutEffect(() => {
    editor.subscribe(setState);
  }, []);

  const { project } = useReactFlow();

  const onConnect = useCallback(
    (edge: Edge | Connection) => editor.addConnection(edge.source as string, edge.target as string),
    []
  );

  const onConnectStart = useCallback((_, { nodeId }) => {
    connectingNodeId.current = nodeId;
  }, []);

  const onConnectEnd = (event) => {
    handleAddNode(event);
  };
  const handleAddNode = (event) => {
    console.log("Adding node", event);
    const targetIsPane = event.target.classList.contains("react-flow__pane");

    if (targetIsPane) {
      // we need to remove the wrapper bounds, in order to get the correct position
      const { top, left } = (reactFlowWrapper as any).current.getBoundingClientRect();
      const id = addNode({ x: event.clientX - left - 75, y: event.clientY - top });
      editor.addConnection(connectingNodeId.current as unknown as string, id);
    }
  };

  const onClick = (event) => {
    if (state.program && state.program.operators.length === 0) handleAddNode(event);
  };

  const addNode = ({ x, y }: XYPosition) => {
    const id = getId();
    const position = project({ x, y });
    console.log("Adding new operator at x, y", position);
    editor.addOperator({
      id,
      metadata: { position },
    });
    return id;
  };

  const nodes = state.program
    ? state.program.operators.map((op) => ({ ...getNode(op.metadata.position), id: op.id, data: { ...op } }))
    : [];

  const edges = state.program
    ? state.program.connections.map((con) => ({ id: `${con.from}-${con.to}`, source: con.from, target: con.to }))
    : [];

  const onNodeDrag = (event) => {
    // TODO: movement feels a bit strange as it doesn't follow exactly the mouse
    // seems like we are missing a scaling factor
    const { x: dx, y: dy } = { x: event.movementX, y: event.movementY };
    editor.incrementOperatorPosition(draggingNodeId.current as string, dx, dy);
  };

  const onNodeDragStart = useCallback((_, { id }) => {
    draggingNodeId.current = id;
  }, []);

  return (
    <div className="wrapper" ref={reactFlowWrapper}>
      <RunBtn />
      <ReactFlow
        nodes={nodes}
        edges={edges}
        onClick={onClick}
        onNodeDrag={onNodeDrag}
        autoPanOnNodeDrag={true}
        onNodeDragStart={onNodeDragStart}
        onDrag={(event) => console.log("Drag start", event)}
        onConnect={onConnect}
        onConnectStart={onConnectStart}
        onConnectEnd={onConnectEnd}
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
