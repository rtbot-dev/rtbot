import { IoMdDownload, IoMdTrash } from "react-icons/all";
import { format } from "date-fns";
import { enGB } from "date-fns/locale";
import { Data } from "@/store/editor/schemas";
import menu from "@/store/menu";
import { useState } from "react";
import { dataApi } from "@/api/data";
import editor from "@/store/editor";

export const DataEntry = (props: Data) => {
  const [state, setState] = useState({
    editing: false,
    showEditBtn: false,
    showDeleteBtn: false,
    title: props.title,
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
    // save new data title in database
    await dataApi.update(props.metadata.id, {
      title: state.title,
    });
    // refresh the list
    await dataApi.list();
  };
  return (
    <div
      className="flow-root w-full justify-center"
      onClick={() => {
        if (state.editing) return;
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
              className="data-title-edit-btn"
              onClick={(event) => {
                event.stopPropagation();
                setState({ ...state, editing: true });
              }}
            >
              edit
            </small>
          )}
          <div>
            {props.createdAt && (
              <small className="data-entry-created-at">
                {"uploaded " +
                  format((props.createdAt as any).toDate(), "PP, HH:mm:ss", { locale: enGB }) +
                  ', delimiter: "' +
                  props.metadata.delimiter +
                  '", rows: ' +
                  props.metadata.numRows}
              </small>
            )}
          </div>
        </div>
      )}
      <div className="float-right">
        {state.showDeleteBtn && (
          <>
            <button
              className="btn btn-ghost btn-sm btn-circle"
              onClick={(event) => {
                menu.loadData(props.metadata.id);
              }}
            >
              <IoMdDownload />
            </button>
            <button
              className="btn btn-ghost btn-sm btn-circle"
              onClick={(event) => {
                event.stopPropagation();
                menu.deleteData(props.metadata.id);
              }}
            >
              <IoMdTrash />
            </button>
          </>
        )}
      </div>
    </div>
  );
};
