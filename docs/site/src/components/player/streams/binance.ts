import { Subject } from "rxjs";

export const getBinanceStream = () => {
  const data$ = new Subject();
  const ws = new WebSocket(
    // we will streaming the live market data for the pair btc/usdt
    // feel free to change this direction to stream whatever you want!
    "wss://stream.binance.com:443/ws/ethusdt@kline_1s"
  );

  ws.onerror = (error) =>
    console.error(`Unable to open websocket connection`, error);

  ws.onmessage = (event) => {
    // decode the message
    const msg = JSON.parse(event.data);
    // emit only the price property
    data$.next({
      time: Math.round(msg.k.T / 1000),
      value: parseFloat(msg.k.c),
    });
  };
  return data$;
};
