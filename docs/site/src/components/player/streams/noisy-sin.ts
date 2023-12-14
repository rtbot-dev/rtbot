import { Subject } from "rxjs";

export const getNoisySinSignal = (
  ms: number,
  w: number,
  mainAmplitude: number,
  noiseAmplitude: number
) => {
  const data$ = new Subject<{ time: number; value: number }>();
  let i = 0;
  let value = 0;
  let time = 0;
  setInterval(() => {
    // on each interval we emit several points to not to overload
    // the browser and still show a smooth curve
    // for (let j = 0; j < 3; j++) {
    i++;
    time = i * ms;
    value = noiseAmplitude * Math.random();
    value += mainAmplitude * Math.sin(w * time);
    data$.next({
      time,
      value,
    });
    // }
  }, ms);

  return data$;
};
