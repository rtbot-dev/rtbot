import { Subject } from "rxjs";

export const getNoisySinSignal = (
  ms: number,
  w: number,
  mainAmplitude: number,
  noiseAmplitude: number,
  speedFactor = 1
) => {
  const data$ = new Subject<{ time: number; value: number }>();
  let value = 0;
  let time = 0;
  setInterval(() => {
    // on each interval we emit several points to not to overload
    // the browser and still show a smooth curve
    for (let j = 0; j < speedFactor; j++) {
      time += Math.floor(ms / speedFactor);
      value = noiseAmplitude * Math.random();
      value += mainAmplitude * Math.sin(w * time);
      data$.next({
        time,
        value,
      });
    }
  }, ms);

  return data$;
};
