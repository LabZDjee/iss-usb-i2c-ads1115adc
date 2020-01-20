/* jshint esversion: 6 */

import {
  configReg as adsConfigReg
} from "@labzdjee/ads1115-config";

const AnixCfgs = [{
    profile: 4,
    gains: ["4.096", "4.096", "4.096", "4.096"],
    iirTakeIns: [50, 60, 70, 80],
  },
  {
    profile: 0,
    gains: ["0.256", "0.512", "4.096", "4.096"],
    iirTakeIns: [51, 52, 50, 50],
  },
  {
    profile: 0,
    gains: ["4.096", "2.048", "1.024", "4.096"],
    iirTakeIns: [53, 54, 55, 50],
  },
  {
    profile: 0,
    gains: ["0.256", "0.512", "1.024", "4.096"],
    iirTakeIns: [56, 57, 58, 50],
  },
];

function makeSequences(anixCfgArray) {
  const sequences = [
    [],
    [],
    [],
    []
  ];

  function setSequences(index, muxes) {
    muxes.forEach((mux, i) => {
      const anixCfg = anixCfgArray[index];
      sequences[index].push({
        mux,
        gain: anixCfg.gains[i],
        iirTakeIn: anixCfg.iirTakeIns[i],
      });
    });
  }
  for (let slaveNb = 0; slaveNb < 4; slaveNb++) {
    const curAnixCfg = anixCfgArray[slaveNb];
    switch (curAnixCfg.profile) {
      case 2:
        setSequences(slaveNb, ["in0in1", "in2in3"]);
        break;
      case 4:
        setSequences(slaveNb, ["in0gnd", "in1gnd", "in2gnd", "in3gnd"]);
        break;
      case 12:
        setSequences(slaveNb, ["in0gnd", "in1gnd", "in2in3"]);
        break;
      case 21:
        setSequences(slaveNb, ["in0in1", "in2gnd", "in3gnd"]);
        break;
    }
  }
  return sequences;
}

const sequences = makeSequences(AnixCfgs);

export default {
  sequences
};