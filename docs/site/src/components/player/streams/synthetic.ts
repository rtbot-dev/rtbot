import { Subject } from "rxjs";

export const createSyntheticSignal = (
  ms: number,
  w: number,
  mainAmplitude: number,
  noiseAmplitude: number
) => {
  const data$ = new Subject<{ time: number; value: number }>();
  let i = 0;
  let value = 0;
  setInterval(() => {
    // on each interval we emit several points to not to overload
    // the browser and still show a smooth curve
    // for (let j = 0; j < 3; j++) {
    i++;
    value = noiseAmplitude * Math.random();
    value += mainAmplitude * Math.sin(w * i);
    data$.next({
      time: i,
      value,
    });
    // }
  }, ms);
  return data$;
};
