export const simpleProgram = `
// RtBot tutorial
// follow the instructions in the comments carefully!

{
  "entryOperator": "in1",
  "operators": [
    { "id": "in1", "type": "Input" },
    { "id": "ma1", "type": "MovingAverage", "n": 2 },
    // 1- try changing the value of n from 2 to 3, up to 8
    // 2- uncomment the following and the correspondent line in the connections
    //{ "id": "peak1", "type": "PeakDetector", "n": 16 },
  ],
  "connections": [
    { "from": "in1", "to": "ma1" },
    //{ "from": "ma1", "to": "peak1" },
  ],
}`;
