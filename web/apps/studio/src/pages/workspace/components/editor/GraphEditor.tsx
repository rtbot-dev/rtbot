import React, { MouseEventHandler, useCallback, useLayoutEffect, useRef, useState } from "react";
import { nanoid } from "nanoid";
import ReactFlow, {
  Connection,
  Edge,
  Position,
  ReactFlowProvider,
  useReactFlow,
  XYPosition,
  MarkerType,
} from "reactflow";
import "reactflow/dist/style.css";

import "./reactflow.css";
import { OperatorNode } from "./OperatorNode";
import editor from "@/store/editor";
import { RunBtn } from "./RunBtn";

const getNode = (position?: { x?: number; y?: number }) => {
  const id = nanoid(4);
  return {
    id,
    sourcePosition: Position.Right,
    targetPosition: Position.Left,
    type: "operatorNode",
    data: {
      parameters: null,
    },
    position: { x: position?.x ?? 0, y: position?.y ?? 0 },
  };
};

const nodeTypes = {
  operatorNode: OperatorNode,
};

const getId = () => nanoid(3);

const fitViewOptions = {
  padding: 3,
};

const AddNodeOnEdgeDrop = ({ programId }: { programId: string }) => {
  const reactFlowWrapper = useRef(null);
  const connectingNodeId = useRef<string | null>(null);
  const draggingNodeId = useRef<string | null>(null);

  const [state, setState] = useState({ program: editor.getState().programs.find((p) => p.metadata?.id === programId) });

  useLayoutEffect(() => {
    editor.subscribe((editorState) =>
      setState({
        program: editorState.programs.find((p) => p.metadata?.id === programId),
      })
    );
  }, []);

  const { project } = useReactFlow();

  const onConnect = useCallback((edge: Edge | Connection) => {
    console.log("Adding connection", edge);
    editor.addConnection(
      programId,
      edge.source as string,
      edge.target as string,
      edge.sourceHandle as string,
      edge.targetHandle as string
    );
  }, []);

  const onConnectStart = useCallback((_: any, { nodeId }: { nodeId: string | null }) => {
    connectingNodeId.current = nodeId;
  }, []);

  const onConnectEnd = (event: any) => {
    console.log("On connect ends");
    handleAddNode(event);
  };
  const handleAddNode: MouseEventHandler<HTMLDivElement> = (event) => {
    const targetIsPane = (event.target as HTMLDivElement).classList.contains("react-flow__pane");

    if (targetIsPane) {
      console.log("Adding node", event);
      // we need to remove the wrapper bounds, in order to get the correct position
      const { top, left } = (reactFlowWrapper as any).current.getBoundingClientRect();
      const id = addNode({ x: event.clientX - left - 75, y: event.clientY - top });
      editor.addConnection(programId, connectingNodeId.current as unknown as string, id);
    }
  };

  const onClick: MouseEventHandler<HTMLDivElement> = (event) => {
    if (state.program && state.program.operators.length === 0) handleAddNode(event);
  };

  const addNode = ({ x, y }: XYPosition) => {
    const id = getId();
    const position = project({ x, y });
    console.log("Adding new operator at x, y", position);
    editor.addOperator(programId, {
      id,
      metadata: { position },
      title: "",
      opType: "",
    });
    return id;
  };

  const nodes = state.program
    ? state.program.operators.map((op) => ({ ...getNode(op.metadata.position), id: op.id, data: { ...op, programId } }))
    : [];

  const edges = state.program
    ? state.program.connections.map((con) => ({
        id: `${con.from}-${con.to}`,
        source: con.from,
        target: con.to,
        targetHandle: `${con.toPort ?? "out"}`,
        markerEnd: {
          type: MarkerType.Arrow,
        },
      }))
    : [];

  const onNodeDrag = (event: { movementX: any; movementY: any }) => {
    // TODO: movement feels a bit strange as it doesn't follow exactly the mouse
    // seems like we are missing a scaling factor
    const { x: dx, y: dy } = { x: event.movementX, y: event.movementY };
    editor.incrementOperatorPosition(programId, draggingNodeId.current as string, dx, dy);
  };

  const onNodeDragStart = useCallback((_: any, { id }: any) => {
    draggingNodeId.current = id;
  }, []);

  return (
    <div className="wrapper" ref={reactFlowWrapper}>
      <RunBtn programId={programId} />
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

export const GraphEditor = ({ programId }: { programId: string }) => (
  <ReactFlowProvider>
    <AddNodeOnEdgeDrop programId={programId} />
  </ReactFlowProvider>
);
