/* jshint esversion: 6 */

import ansiEsc from "ansi-escapes";
import Fifo from "tiny-queue";
import pad from "pad";
import style from "ansi-styles";

import iss from "./usb-iss.mjs";
import cfg from "./configuration.mjs";
import util from "./utility.mjs";

const printObj = util.printObj;

import { addresses as adsAddresses, configReg as adsConfigReg, numeric as adsUtil } from "@labzdjee/ads1115-config";

const sequences = cfg.sequences;

const results = [];

const adsConfigObject = adsConfigReg.alterObject(["860SPS", "continuousConversion"], {});

function buildFollowupObject(deviceIndex, channelIndex, command) {
  return { deviceIndex, channelIndex, command };
}

const issFifo = new Fifo();

let continuationDone = false;

let currentFollowupObject;

let issBusy = false;

function processFifo() {
  if (!issBusy && issFifo.length > 0) {
    const followupObject = issFifo.shift();
    const issFct = continuationDone ? iss.nextStep : iss.setContinuation;
    const sequenceChannel = sequences[followupObject.deviceIndex][followupObject.channelIndex];
    continuationDone = true;
    currentFollowupObject = followupObject;
    switch (followupObject.command) {
      case "configuration":
        adsConfigRegister = adsConfigReg.build(
          adsConfigReg.alterObject([sequenceChannel.mux, sequenceChannel.gain], adsConfigObject)
        );
        issBusy = true;
        issFct(
          "config-ads",
          [
            iss.i2cMode.I2C_AD1,
            adsAddresses.setSlaveAddress(deviceIndex, false),
            adsAddresses.setPointerRegister("configuration"),
            2,
            adsConfigRegister.highByte,
            adsConfigRegister.lowByte,
          ],
          onIssReply
        );
        break;
      case "data":
        issBusy = true;
        issFct(
          "read-ads-conversion",
          [
            iss.i2cMode.I2C_AD1,
            adsAddresses.setSlaveAddress(followupObject.deviceIndex, true),
            adsAddresses.setPointerRegister("conversion"),
            2,
          ],
          onIssReply
        );
        break;
    }
  }
}

function onIssDelayExpired(followupObject) {
  issFifo.push(followupObject);
  processFifo();
}

function onIssReply(step, data) {
  const followupObject = util.shallowCopy(currentFollowupObject);
  const resultAsArray = iss.bufferToArray(data);
  const sequence = sequences[followupObject.deviceIndex];
  issBusy = false;
  switch (step) {
    case "config-ads":
      if (resultAsArray[0] === 0) {
        results[deviceIndex].forEach((channel) => {
          channel[filteredValue] = null;
        });
        setTimeout(onIssDelayExpired, 1000, followupObject);
      } else {
        followupObject.command = "data";
        setTimeout(onIssDelayExpired, 1000, followupObject);
      }
      break;
    case "read-ads-conversion":
      const result = results[followupObject.deviceIndex][followupObject.channelIndex];
      adsUtil.iirLowPassFilter(adsUtil.twoBytes2SignedInt16(resultAsArray), result);
      result.filteredValue = Math.round(result.filteredValue);
      printResults(); // to do
      followupObject.command = "configuration";
      followupObject.channelIndex = (followupObject.channelIndex + 1) % sequence.length;
      issFiFo.push(followupObject);
      break;
  }
  processFifo();
  return false;
}

let adsConfigRegister;

const nbSequenceElements = sequences.reduce((acc, arr) => acc + arr.length, 0);

for (let i = 0; i < 4; i++) {
  const sequence = sequences[i];
  const resultArray = [];
  const invalid = sequence.length === 0;
  results.push(resultArray);
  if (!invalid) {
    issFifo.push(buildFollowupObject(i, 0, "config"));
  }
  for (let j = 0; j < sequence.length; j++) {
    resultArray.push({
      filteredValue: null,
      takeInPerCent: sequence[j].iirTakeIn,
    });
  }
}
processFifo();

printObj({
  sequences,
});
console.log(results);

console.log(`nb of elements: ${nbSequenceElements}`);

if (nbSequenceElements === 0) {
  console.log("no sequence to work upon: stop!");
  process.exit();
}

let initialPrintResults = true;

function printResults() {
  const defIn = style.green.open;
  const defOut = style.green.close;
  const errIn = style.red.open;
  const errOut = style.red.close;
  const valIn = style.yellow.open;
  const valOut = style.yellow.close;
  const disIn = style.blue.open;
  const disOut = style.blue.close;
  let prolog =
    initialPrintResults === false
      ? `${ansiEsc.cursorUp(5)}${ansiEsc.cursorLeft}${ansiEsc.eraseLines(4)}`
      : `${ansiEsc.clearScreen}${ansiEsc.cursorHide}`;
  for (let dev = 0; dev < 4; dev++) {
    const result = results[dev];
    let str = `${prolog}${defIn}@0x${adsAddresses.setSlaveAddress(dev).toString(16)} ${defOut}`;
    prolog = "";
    if (result.length === 0) {
      str += `${disIn}not enabled${disOut}`;
    } else if (result[0].filteredValue === null) {
      str += `${errIn}unreachable${errOut}`;
    } else {
      for (let meas = 0; meas < results.length; meas++) {
        const valStr = pad(5, result[meas].filteredValue);
        str += `${defIn}${meas + 1}: ${defOut}${valIn}${valStr}${valOut} `;
      }
    }
    console.log(str);
  }
  initialPrintResults = false;
}

process.on("SIGINT", function() {
  console.log(`Bye!${ansiEsc.cursorShow}`);
  process.exit();
});
