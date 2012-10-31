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
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Atul Varma <atul@mozilla.com>
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

const EXPORTED_SYMBOLS = ["SandboxFactory"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("/ubiquity/modules/utils.js");

var defaultTarget = this;

function SandboxFactory(globals, target, ignoreForcedProtection) {
  maybeInitialize();
  globals = globals || {};
  this._target = target || defaultTarget;
  this._ignoreForcedProtection = ignoreForcedProtection;
  this._makeGlobals = typeof globals == "function" ?
    globals : function defaultMakeGlobals(id) globals;
}

SandboxFactory.protectedFileUriPrefix = "";
SandboxFactory.fileUri = "";
SandboxFactory.isInitialized = false;

SandboxFactory.unmungeUrl = function unmungeUrl(url) {
  if (this.isInitialized && ~url.lastIndexOf(this.protectedFileUriPrefix, 0))
    return url.slice(this.protectedFileUriPrefix.length);
  return url;
};

function maybeInitialize() {
  if (SandboxFactory.isInitialized) return;

  SandboxFactory.fileUri =
    Utils.ResProtocolHandler.resolveURI(
      Utils.uri("/ubiquity/modules/sandboxfactory.js"));
  // We need to prefix any source code URI's with a known
  // "protected" file URI so that XPConnect wrappers are implicitly
  // made for them.
  SandboxFactory.protectedFileUriPrefix = SandboxFactory.fileUri + "#";

  SandboxFactory.isInitialized = true;
}

Utils.extend(SandboxFactory.prototype, {
  makeSandbox: function makeSandbox(codeSource) {
    var sandbox = Cu.Sandbox(this._target);
    var globals = this._makeGlobals(codeSource);
    for (let key in globals) sandbox[key] = globals[key];
    return sandbox;
  },

  evalInSandbox: function evalInSandbox(code, sandbox, codeSections) {
    codeSections = codeSections ||
      [{filename: "<string>", lineNumber: 0, length: code.length}];
    var retVal, currIndex = 0;
    for each (let section in codeSections) {
      let sourceCode = code.slice(currIndex, currIndex += section.length);
      let filename = section.filename;
      if (!this._ignoreForcedProtection)
        filename = SandboxFactory.protectedFileUriPrefix + filename;
      retVal = Cu.evalInSandbox(sourceCode, sandbox,
                                1.8, filename, section.lineNumber);
    }
    return retVal;
  }
});
