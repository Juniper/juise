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
 *   Jono DiCarlo <jdicarlo@mozilla.com>
 *   Michael Yoshitaka Erlewine <mitcho@mitcho.com>
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

var codesource_EXPORTED_SYMBOLS = [
  "MixedCodeSource",
  "StringCodeSource",
  "RemoteUriCodeSource",
  "LocalUriCodeSource",
  "XhtmlCodeSource",
];

// const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

// Cu.import("/ubiquity/modules/utils.js");
// Cu.import("/ubiquity/modules/xml_script_commands_parser.js");

const VALID_SCHEMES_REMOTE = {http: 1, https: 1};
const VALID_SCHEMES_LOCAL  = {file: 1, chrome: 1, resource: 1, ubiquity: 1};

function MixedCodeSource(bodySource, headerSources, footerSources) {
  this.id = bodySource.id;
  this._bodySource = bodySource;
  this._sources = headerSources.concat(bodySource, footerSources);
}

MixedCodeSource.prototype = {
  getCode: function MCS_getCode() {
    var code = "", c;
    var codeSections = this.codeSections = [];
    this.updated = false;
    for each (let cs in this._sources) {
      let c = cs.getCode();
      code += c;
      if (cs.codeSections)
        codeSections.push.apply(codeSections, cs.codeSections);
      else
        codeSections.push({
          length: c.length,
          filename: cs.id,
          lineNumber: 1});
      if (cs.updated) this.updated = true;
    }
    this.dom = this._bodySource.dom;
    return code;
  }
};

function StringCodeSource(code, id, dom, codeSections) {
  this._code = code;
  this._count = 0;
  this.id = id;
  this.dom = dom;
  this.codeSections = codeSections;
}

StringCodeSource.prototype = {
  getCode: function SCS_getCode() {
    this.updated = !this._count++;
    return this._code;
  }
};

// timeoutInterval is the minimum amount of time to wait before
// re-requesting the content of a code source, in milliseconds.
// If negative, the request is never made (used for non-autoupdating feeds).
function RemoteUriCodeSource(feedInfo, timeoutInterval) {
  this.id = feedInfo.srcUri.spec;
  this.timeoutInterval = timeoutInterval;
  this._feedInfo = feedInfo;
  this._req = null;
  this._hasCheckedRecently = false;
  this._cache = null;
};

RemoteUriCodeSource.isValidUri =
function RUCS_isValidUri(uri) Utils.uri(uri).scheme in VALID_SCHEMES_REMOTE;

RemoteUriCodeSource.prototype = {
  getCode: function RUCS_getCode() {
    if (this.timeoutInterval >= 0 &&
        !this._req && !this._hasCheckedRecently) {
      this._hasCheckedRecently = true;

      // Queue another XMLHttpRequest to fetch the latest code.
      var self = this;
      var req = self._req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                            .createInstance(Ci.nsIXMLHttpRequest);
      req.mozBackgroundRequest = true;
      req.open("GET", Utils.gist.fixRawUrl(this._feedInfo.srcUri.spec), true);
      req.overrideMimeType("text/plain");
      req.onreadystatechange = function RUCS__onXhrChange() {
        if (req.readyState < 4) return;
        if (req.status === 200)
          // Update our cache.
          self._feedInfo.setCode(req.responseText);
        self._req = null;

        function clearTimeout() { self._hasCheckedRecently = false; }

        if (self.timeoutInterval)
          Utils.setTimeout(clearTimeout, self.timeoutInterval);
        else
          clearTimeout();
      };
      req.send(null);
    }

    // Return whatever we've got cached for now.
    var code = this._feedInfo.getCode();
    this.updated = this._cache !== code;
    return this._cache = code;
  }
};

function LocalUriCodeSource(uri, noReload) {
  this.id = uri;
  this.uri = Utils.uri(uri, "data:,");
  this.noReload = !!noReload;
  this._cachedCode = null;
  this._cachedTimestamp = 0;
  if (this.uri.scheme == "resource") try {
    this.uri = Utils.uri(Utils.ResProtocolHandler.resolveURI(this.uri));
  } catch ([]) {}
}

LocalUriCodeSource.isValidUri =
function LUCS_isValidUri(uri) Utils.uri(uri).scheme in VALID_SCHEMES_LOCAL;

LocalUriCodeSource.prototype = {
  // The throwNoError property will turn off the
  // error thrown when the code source could not be found.
  // This is used avoid an error in testLocalUriCodeSourceWorksWith.
  getCode: function LUCS_getCode(throwNoError) {
    if (this.noReload && this._cachedCode) {
      this.updated = false;
      return this._cachedCode;
    }
    try {
      if (this.uri.scheme === "file") {
        var {file} = this.uri.QueryInterface(Ci.nsIFileURL);
        if (file.exists()) {
          var {lastModifiedTime} = file;
          if (this._cachedCode != null &&
              this._cachedTimestamp === lastModifiedTime) {
            this.updated = false;
            return this._cachedCode;
          }
          this._cachedTimestamp = lastModifiedTime;
        }
        else return "";
      }

      var req = (Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                 .createInstance(Ci.nsIXMLHttpRequest));
      req.open("GET", this.id, false);
      req.overrideMimeType("text/plain");
      req.send(null);

      if (req.status !== 0)
        throw new Error("XHR returned status " + req.status);

      let code = req.responseText;
      if (/^ERROR:/.test(code)) throw new Error(code);
      this.updated = this._cachedCode !== code;
      return this._cachedCode = code;
    } catch (e) {
      if (!throwNoError)
        Cu.reportError("Retrieving " + this.id + " raised exception " + e);
      this.updated = false;
      return "";
    }
  }
};

function XhtmlCodeSource(codeSource) {
  var dom = null;
  var codeSections = null;
  var finalCode = "";

  this.__defineGetter__("dom", function XCS_dom() dom);
  this.__defineGetter__("id", function XCS_id() codeSource.id);
  this.__defineGetter__("codeSections",
                        function XCS_codeSections() codeSections);

  this.getCode = function XCS_getCode() {
    var code = codeSource.getCode();
    if (!(this.updated = codeSource.updated)) return finalCode;

    if (/^\s*</.test(code)) {
      // TODO: What if this fails?  Right now the behavior generally
      // seems ok simply because an exception doesn't get thrown here
      // if the XML isn't well-formed, we just get an error results
      // DOM back, which contains no command code.
      dom = (Cc["@mozilla.org/xmlextras/domparser;1"]
             .createInstance(Ci.nsIDOMParser)
             .parseFromString(code, "text/xml"));
      codeSections = [];
      finalCode = "";
      for each (let info in parseCodeFromXml(code)) {
        let c = info.code;
        finalCode += c;
        codeSections.push({
          length: c.length,
          filename: codeSource.id,
          lineNumber: info.lineNumber});
      }
    }
    else {
      dom = codeSections = null;
      finalCode = code;
    }
    return finalCode;
  };
}
