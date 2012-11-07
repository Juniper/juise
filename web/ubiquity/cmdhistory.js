/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Ubiquity.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Satoshi Murakami <murky.satyr@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

Components.utils.import("/ubiquity/modules/utils.js");

const EXPORTED_SYMBOLS = ["CommandHistory"];
const PREF_BIN = "extensions.ubiquity.history.bin";
const PREF_MAX = "extensions.ubiquity.history.max";
const SEPARATOR = "\n";

var CommandHistory = this, cursor = -1, _bin = null;

function get() _bin || (
  _bin = Utils.prefs.get(PREF_BIN).split(SEPARATOR).filter(Boolean));
function set(arr) {
  _bin = arr;
  return save();
}
function add(txt) {
  if (!(txt = txt.trim())) return this;
  var bin = get(), idx = bin.indexOf(txt);
  if (~idx) bin.unshift(bin.splice(idx, 1)[0]);
  else {
    let max = Utils.prefs.get(PREF_MAX);
    if (bin.unshift(txt) > max) bin.length = max;
  }
  return save();
}
function save() {
  Utils.prefs.set(PREF_BIN, _bin.join(SEPARATOR));
  return this;
}
function go(num, U) {
  var {textBox} = U = U || Utils.currentChromeWindow.gUbiquity;
  var bin = get();
  if (cursor < 0 && textBox.value) {
    add(textBox.value);
    cursor = 0;
  }
  cursor -= num;
  if (cursor < -1 || bin.length <= cursor) cursor = -1;
  U.preview(bin[cursor] || "");
  return this;
}
function complete(rev, U) {
  var {textBox} = U = U || Utils.currentChromeWindow.gUbiquity;
  var {value: txt, selectionStart: pos} = textBox, bin = get();
  if (rev) bin = bin.slice().reverse();
  pos -= txt.length - (txt = txt.trimLeft()).length;
  var key = txt.slice(0, pos), re = RegExp("^" + Utils.regexp.quote(key), "i");
  for (let h, i = bin.indexOf(txt); h = bin[++i];) if (re.test(h)) {
    U.preview(h);
    textBox.setSelectionRange(key.length, textBox.textLength);
    return true;
  }
  return false;
}
