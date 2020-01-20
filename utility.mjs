/* jshint esversion: 6 */

// get key from value (first found) in an object (inObject)
// returns 'defReturn' in case of no hit
function v2k(val, inObject, defReturn) {
  for (let k of Object.keys(inObject)) {
    if (inObject[k] === val) {
      return k;
    }
  }
  return defReturn === undefined ? null : defReturned;
}

function shallowCopy(inObject) {
  return Object.assign({}, inObject);
}

function printObj(wrappedObject, bPrint) {
  let keyArray;
  if (typeof wrappedObject !== "object" || (keyArray = Object.keys(wrappedObject)).length !== 1) {
    throw "printObj needs an object to print which is simply wrapped into a single key object: {object}";
  }
  const key = keyArray[0];
  const str = `${key}: ${JSON.stringify(wrappedObject[key], null, " ")}`;
  if (bPrint === false) {
    return str;
  }
  console.log(str);
}

export default {
  v2k,
  shallowCopy,
  printObj,
};