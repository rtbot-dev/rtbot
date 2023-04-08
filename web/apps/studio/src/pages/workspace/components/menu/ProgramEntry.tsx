import { IoMdTrash } from "react-icons/all";
import { format } from "date-fns";
import { enGB } from "date-fns/locale";
import { Program } from "@/store/editor/schemas";
import menu from "@/store/menu";
import { useState } from "react";
import { programApi } from "@/api/program";
import editor from "@/store/editor";

export const ProgramEntry = (props: Program) => {
  const [state, setState] = useState({
    editing: false,
    showEditBtn: false,
    showDeleteBtn: false,
    title: props.metadata ? props.metadata.title : "",
  });
  const onTitleKeyUp = async (event) => {
    if (event.key === "Escape") {
      setState({ ...state, editing: false });
    }
    if (event.key === "Enter") {
      await updateTitle(event);
    }
  };
  // TODO: improve finish title edition trigger
  // window.document.addEventListener("keyup", onTitleKeyUp);

  const updateTitle = async (event) => {
    event.stopPropagation();
    // finish edition
    setState({ ...state, editing: false });
    // save new program title in database
    await programApi.update(props.metadata!!.id as string, {
      "metadata.title": state.title,
    });
    // refresh the list
    await programApi.list();
  };
  return (
    <div
      className="flow-root w-full justify-center"
      onClick={() => {
        if (state.editing) return;
        editor.setProgram(props);
        menu.hide();
      }}
      onMouseEnter={() => setState({ ...state, showDeleteBtn: true })}
      onMouseLeave={() => setState({ ...state, showDeleteBtn: false, showEditBtn: false })}
    >
      {state.editing ? (
        <input
          type="text"
          onKeyUp={onTitleKeyUp}
          onBlur={updateTitle}
          onChange={(e) => setState({ ...state, title: e.target.value })}
          value={state.title}
          onClick={(event) => event.stopPropagation()}
        />
      ) : (
        <div
          className="float-left align-middle"
          onMouseEnter={() => setState({ ...state, showEditBtn: true })}
          onMouseLeave={() => setState({ ...state, showEditBtn: false })}
        >
          {state.title}
          {state.showEditBtn && (
            <small
              className="program-title-edit-btn"
              onClick={(event) => {
                event.stopPropagation();
                setState({ ...state, editing: true });
              }}
            >
              edit
            </small>
          )}
          <div>
            {props.metadata && props.metadata.createdAt && (
              <small className="program-entry-created-at">
                {"created " +
                  format((props.metadata.createdAt as any).toDate(), "PP, HH:mm:ss", { locale: enGB }) +
                  `, O=${props.operators.length}` +
                  `, C=${props.connections.length}`}
              </small>
            )}
          </div>
        </div>
      )}
      <div className="float-right">
        {state.showDeleteBtn && (
          <button
            className="btn btn-ghost btn-sm btn-circle"
            onClick={(event) => {
              event.stopPropagation();
              if (props.metadata) menu.deleteProgram(props.metadata.id as string);
            }}
          >
            <IoMdTrash />
          </button>
        )}
      </div>
    </div>
  );
};
