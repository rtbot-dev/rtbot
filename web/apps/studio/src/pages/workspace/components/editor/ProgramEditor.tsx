import Editor from "@monaco-editor/react";
import { useLayoutEffect, useState } from "react";
import editor from "@/store/editor";

export const ProgramEditor = () => {
  const [editorState, setEditorState] = useState(editor.getState());

  useLayoutEffect(() => {
    editor.subscribe(setEditorState({ program: "" }));
  }, []);

  return (
    <Editor
      defaultLanguage="yaml"
      defaultValue="# welcome to rtbot!"
      theme="vs-dark"
      value={editorState.program}
      onChange={(e) => {
        if (editorState.program !== e) {
          setEditorState({ ...editorState, program: e ?? "" });
        }
      }}
    />
  );
};
