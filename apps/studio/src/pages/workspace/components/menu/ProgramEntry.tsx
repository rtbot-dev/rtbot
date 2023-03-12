import { IoMdOpen, IoMdTrash } from "react-icons/all";
import { Program } from "@/store/editor/schemas";
import menu from "@/store/menu";
import { useState } from "react";
import { programApi } from "../../../../api/program";

export const ProgramEntry = (props: Program) => {
  const [state, setState] = useState({ editing: false, title: props.metadata ? props.metadata.title : "" });
  const onTitleKeyUp = async (e) => {
    if (e.key === "Escape") {
      setState({ ...state, editing: false });
    }
    if (e.key === "Enter") {
      // finish edition
      setState({ ...state, editing: false });
      // save new program title in database
      await programApi.update(props.metadata!!.id as string, {
        metadata: { title: state.title },
      });
      // refresh the list
      await programApi.list();
    }
  };
  return (
    <div className="flow-root w-full justify-center">
      {state.editing ? (
        <input
          type="text"
          onKeyUp={onTitleKeyUp}
          onChange={(e) => setState({ ...state, title: e.target.value })}
          value={state.title}
        />
      ) : (
        <div className="float-left align-middle" onClick={() => setState({ ...state, editing: true })}>
          {state.title}
        </div>
      )}
      <div className="float-right btn-group">
        <button className="btn btn-outline btn-xs">
          <IoMdOpen />
        </button>
        <button
          className="btn btn-outline btn-xs"
          onClick={() => {
            if (props.metadata) menu.deleteProgram(props.metadata.id as string);
          }}
        >
          <IoMdTrash />
        </button>
      </div>
    </div>
  );
};
