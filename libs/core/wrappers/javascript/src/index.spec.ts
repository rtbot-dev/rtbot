import { RtBot } from "@rtbot/api";

test("RtBot Class", () => {
  const rtBot = new RtBot();
  console.log("rtbot", rtBot);
  expect("a").toBe("a");
});
