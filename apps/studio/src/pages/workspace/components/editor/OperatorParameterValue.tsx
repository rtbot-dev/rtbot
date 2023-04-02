import React from "react";

export type OperatorParameterValueProps = {
  parameter: string;
  value: string;
};

export const OperatorParameterValue = ({ parameter, value }: OperatorParameterValueProps) => {
  // any key having color on its name
  if (parameter.indexOf("color") > -1) {
    return (
      <div className="flex items-center">
        {parameter}=
        <span className="color-selected flex justify-center align-middle">
          <div
            className="color-selected-inner-div"
            style={{ boxShadow: `${value} 0px 0px 0px 15px inset`, transform: `scale(0.5)` }}
          ></div>
        </span>
      </div>
    );
  } else {
    return (
      <div key={parameter}>
        {parameter}={value}
      </div>
    );
  }
};
