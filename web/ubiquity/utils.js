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
 *   Blair McBride <unfocused@gmail.com>
 *   Jono DiCarlo <jdicarlo@mozilla.com>
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

// = Utils =
// A small (?) library of all-purpose, general utility functions
// for use by chrome code.  Everything clients need is contained within
// the {{{Utils}}} namespace.

// var EXPORTED_SYMBOLS = ["Utils"];
jQuery(function ($) {

// const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
// const {nsISupportsString, nsITransferable} = Ci;
const LOG_PREFIX = "Ubiquity: ";
const TO_STRING = Object.prototype.toString;

// Cu.import("resource://gre/modules/XPCOMUtils.jsm");
// Cu.import("resource://gre/modules/Services.jsm");

var Utils = {

  toString: function toString() "[object UbiquityUtils]",

  NS_MATHML: "http://www.w3.org/1998/Math/MathML",
  NS_XHTML: "http://www.w3.org/1999/xhtml",
  NS_SVG: "http://www.w3.org/2000/svg",
  NS_EM: "http://www.mozilla.org/2004/em-rdf#",
  NS_XUL: "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",

  // === {{{ Utils.currentChromeWindow }}} ===
  // A reference to the application chrome window that currently has focus.
  get currentChromeWindow()
    Utils.WindowMediator.getMostRecentWindow(Utils.appWindowType),

  // === {{{ Utils.chromeWindows }}} ===
  // An array of application chrome windows currently opened.
  get chromeWindows() {
    var wins = [];
    var nmrt = Utils.WindowMediator.getEnumerator(Utils.appWindowType);
    while (nmrt.hasMoreElements()) wins.push(nmrt.getNext());
    return wins;
  },

  // === {{{ Utils.currentTab }}} ===
  // A reference to the focused tab as {{{Utils.BrowserTab}}}.
  get currentTab()
    BrowserTab(Utils.currentChromeWindow.gBrowser.mCurrentTab),

  // === {{{ Utils.currentTabs }}} ===
  // An array of tabs within the current chrome window.
  get currentTabs() gTabs.from(Utils.currentChromeWindow),

  // === {{{ Utils.loggedIn }}} ===
  // Whether or not the user has logged-in to the browser with master pass.
  get loggedIn() {
    var token = (Cc["@mozilla.org/security/pk11tokendb;1"]
                 .getService(Ci.nsIPK11TokenDB)
                 .getInternalKeyToken());
    return !token.needsLogin() || token.isLoggedIn();
  },

  __globalObject: this,
};

// for each (let f in this) if (typeof f === "function") Utils[f.name] = f;
// delete Utils.QueryInterface;
[
  // === {{{ Utils.Application }}} ===
  // Shortcut to [[https://developer.mozilla.org/en/FUEL/Application]].
  function Application()
  Cc["@mozilla.org/fuel/application;1"].getService(Ci.fuelIApplication),

  // === {{{ Utils.LoginManager }}} ===
  // Shortcut to {{{nsILoginManager}}}.
  function LoginManager()
  Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager),

  // === {{{ Utils.ResProtocolHandler }}} ===
  // Shortcut to {{{nsIResProtocolHandler}}}.
  function ResProtocolHandler()
  Utils.IOService.getProtocolHandler("resource")
  .QueryInterface(Ci.nsIResProtocolHandler),

  // === {{{ Utils.UnescapeHTML }}} ===
  // Shortcut to {{{nsIScriptableUnescapeHTML}}}.
  function UnescapeHTML()
  Cc["@mozilla.org/feed-unescapehtml;1"]
  .getService(Ci.nsIScriptableUnescapeHTML),

  // === {{{ Utils.UnicodeConverter }}} ===
  // Shortcut to {{{nsIScriptableUnicodeConverter}}}.
  function UnicodeConverter()
  Cc["@mozilla.org/intl/scriptableunicodeconverter"]
  .getService(Ci.nsIScriptableUnicodeConverter),

  // === {{{ Utils.hiddenWindow }}} ===
  // The application's global hidden window.
  function hiddenWindow()
  Cc["@mozilla.org/appshell/appShellService;1"]
  .getService(Ci.nsIAppShellService).hiddenDOMWindow,

  // === {{{ Utils.appName }}} ===
  // The chrome application name found in {{{nsIXULAppInfo}}}.
  // Example values are {{{"Firefox"}}}, {{{"Songbird"}}}, {{{"Thunderbird"}}}.
    function appName() { return "Firefox" },

  // === {{{ Utils.appWindowType }}} ===
  // The name of "main" application windows for the chrome application.
  // Example values are {{{"navigator:browser"}}} for Firefox/Thunderbird
  // and {{{"Songbird:Main"}}} for Songbird.
  function appWindowType()
  ({Songbird: "Songbird:Main"})[Utils.appName] || "navigator:browser",

  // === {{{ Utils.OS }}} ===
  // The platform name found in {{{nsIXULRuntime}}}.
  // See [[https://developer.mozilla.org/en/OS_TARGET]].
  function OS()
  Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS,

].reduce(defineLazyProperty, Utils);
({
  // === {{{ Utils.ConsoleService }}} ===
  // Shortcut to {{{nsIConsoleService}}}.
  console: Utils.ConsoleService,
  // === {{{ Utils.DirectoryService }}} ===
  // Shortcut to {{{nsIDirectoryService}}}.
  dirsvc: Utils.DirectoryService,
  // === {{{ Utils.IOService }}} ===
  // Shortcut to {{{nsIIOService}}}.
  io: Utils.IOService,
  // === {{{ Utils.PromptService }}} ===
  // Shortcut to {{{nsIPromptService}}}.
  prompt: Utils.PromptService,
  // === {{{ Utils.WindowMediator }}} ===
  // Shortcut to {{{nsIWindowMediator}}}.
  wm: Utils.WindowMediator,
}); // = Services;

// Cu.import("resource://gre/modules/AddonManager.jsm", Utils);

// === {{{ Utils.log(a, b, c, ...) }}} ===
// One of the most useful functions to know both for development and debugging.
// This logging function takes an arbitrary number of arguments and
// will log them to the most appropriate output. If you have Firebug,
// the output will go to its console.
// Otherwise, the output will be routed to the Javascript Console.
//
// {{{Utils.log()}}} implements smart pretty print, so you
// can use it for inspecting arrays and objects.
// See http://getfirebug.com/console.html for details.
//
// {{{a, b, c, ...}}} is an arbitrary list of things to be logged.

function log(what) {
  if (!arguments.length) return;

  var args = Array.slice(arguments), formatting = typeof what === "string";

  var {Firebug} = Utils.currentChromeWindow;
  if (Firebug && Firebug.Console.isEnabled() &&
      Firebug.toggleBar(true, "console")) {
    if (formatting) args[0] = LOG_PREFIX + what;
    else args.unshift(LOG_PREFIX);
    Firebug.Console.logFormatted(args);
    return;
  }

  function log_pp(o) {
    try { var p = uneval(o) } catch ([]) {}
    return p && p !== "({})" && (p !== "null" || o === null) ? p : o;
  }
  var lead = !formatting ? log_pp(args.shift()) :
  args.shift().replace(/%[sdifo]/g, function log_format($) {
    if (!args.length) return $;
    var a = args.shift();
    switch ($) {
      case "%s": return a;
      case "%d":
      case "%i": return parseInt(a);
      case "%f": return parseFloat(a);
    }
    return log_pp(a);
  });
  Utils.reportInfo(
    args.reduce(function log_acc(msg, arg) msg + " " + log_pp(arg), lead));
}

// === {{{ Utils.dump(a, b, c, ...) }}} ===
// A nicer {{{dump()}}} variant that
// displays caller's name, concats arguments and appends a line feed.

Utils.dump = function niceDump() {
//  if (!gPrefs.get("extensions.ubiquity.dump", false)) return;

  var {caller} = arguments.callee;
  $.dbgpr((caller ? caller.name + ": " : "") + Array.join(arguments, " "));
};

// === {{{ Utils.reportError(error) }}} ===
// Given an {{{Error}}} object, reports it to the JS Error Console
// as if it was thrown from the original location.

function reportError(error) {
    $.dbgpr("file: " + error.fileName + ":" + error.lineNumber
            + ": " + error);
//  var scriptError =
//    Cc["@mozilla.org/scripterror;1"].createInstance(Ci.nsIScriptError);
//  scriptError.init(error, error.fileName, null, error.lineNumber,
//                   null, scriptError.errorFlag, null);
//  Utils.ConsoleService.logMessage(scriptError);
}

// === {{{ Utils.reportWarning(aMessage, stackFrameNumber) }}} ===
// Reports a warning to the JS Error Console, which can be displayed in Firefox
// by choosing "Error Console" from the "Tools" menu.
//
// {{{aMessage}}} is a plaintext string corresponding to the warning
// to provide.
//
// {{{stackFrameNumber}}} is an optional number specifying how many
// frames back in the call stack the warning message should be
// associated with. Its default value is 0, meaning that the line
// number of the caller is shown in the JS Error Console.  If it's 1,
// then the line number of the caller's caller is shown.

function reportWarning(aMessage, stackFrameNumber) {
  var stackFrame = Components.stack.caller;
  for (let i = stackFrameNumber | 0; i --> 0;) stackFrame = stackFrame.caller;

  var scriptError =
    Cc["@mozilla.org/scripterror;1"].createInstance(Ci.nsIScriptError);
  var aSourceName = stackFrame.filename;
  var aSourceLine = stackFrame.sourceLine;
  var aLineNumber = stackFrame.lineNumber;
  var aColumnNumber = null;
  var aFlags = scriptError.warningFlag;
  var aCategory = "ubiquity javascript";
  scriptError.init(aMessage, aSourceName, aSourceLine, aLineNumber,
                   aColumnNumber, aFlags, aCategory);
  Utils.ConsoleService.logMessage(scriptError);
}

// === {{{ Utils.reportInfo(message) }}} ===
// Reports a purely informational {{{message}}} to the JS Error Console.
// Source code links aren't provided for informational messages, so
// unlike {{{Utils.reportWarning()}}}, a stack frame can't be passed
// in to this function.

function reportInfo(message) {
    $.dbgpr(LOG_PREFIX + message);
//  Utils.ConsoleService.logStringMessage(LOG_PREFIX + message);
}

// === {{{Utils.ellipsify(node, characters, [ellipsis])}}} ===
// Given a DOM {{{node}}} (or string) and a maximum number of {{{characters}}},
// returns a new DOM node or string that has the same contents truncated to
// that number of characters. If any truncation was performed,
// an {{{ellipsis}}} is placed at the end of the content.

function ellipsify(node, chars, ellipsis) {
  if (ellipsis == null) ellipsis = gPrefs.get("intl.ellipsis", "\u2026");
  if (typeof node != "object") {
    if (chars < 1) return "";
    let str = String(node);
    return str.length > chars ? str.slice(0, chars - 1) + ellipsis : str;
  }
  var copy = node.cloneNode(false);
  switch (node.nodeType) {
    case node.TEXT_NODE:
    case node.CDATA_SECTION_NODE:
    copy.nodeValue = ellipsify(node.nodeValue, chars, ellipsis);
    break;
    case node.ELEMENT_NODE:
    case node.DOCUMENT_NODE:
    case node.DOCUMENT_FRAGMENT_NODE:
    let child = node.firstChild;
    for (; child && chars > 0; child = child.nextSibling) {
      let key = (child.nodeType == node.ELEMENT_NODE && "textContent" ||
                 (child.nodeType == node.TEXT_NODE ||
                  child.nodeType == node.CDATA_SECTION_NODE) && "nodeValue");
      if (key) {
        let childCopy = copy.appendChild(ellipsify(child, chars, ellipsis));
        chars -= childCopy[key].length;
      }
      else copy.appendChild(child.cloneNode(false));
    }
  }
  return copy;
}

// === {{{ Utils.absolutifyUrlAttribute(element) }}} ===
// Takes the URL specified as an attribute in the given DOM {{{element}}}
// and convert it to an absolute URL.

const URL_ATTRS = ["href", "src", "action"];

function absolutifyUrlAttribute(element) {
  for each (let attr in URL_ATTRS) if (attr in element) {
    element.setAttribute(attr, element[attr]);
    break;
  }
  return element;
}

// === {{{ Utils.isTextBox(node) }}} ===
// Returns whether or not the given DOM {{{node}}} is a textbox.

function isTextBox(node) {
  try { return node.selectionEnd >= 0 } catch (_) { return false }
}

// === {{{ Utils.setTimeout(callback, delay = 0, arg0, arg1, ...) }}} ===
// Works just like the {{{window.setTimeout()}}} method
// in content space, but it can only accept a function (not a string)
// as the callback argument.
//
// This function returns a timer ID, which can later be given to
// {{{Utils.clearTimeout()}}} if the client decides that it wants to
// cancel the callback from being triggered.
//
// {{{callback}}} is the callback function to call when the given
// delay period expires.  It will be called only once (not at a regular
// interval).
//
// {{{delay}}} is the delay, in milliseconds, after which the callback
// will be called once.
//
// {{{arg0, arg1, ...}}} are optional arguments that will be passed to
// the callback.

function setTimeout(callback, delay /*, arg0, arg1, ...*/) {
  // emulate window.setTimeout() by incrementing next ID
  var timerID = gNextTimerID++;
  var timerClass = Cc["@mozilla.org/timer;1"];
  (gTimers[timerID] = timerClass.createInstance(Ci.nsITimer)).initWithCallback(
    new __TimerCallback(timerID, callback, Array.slice(arguments, 2)),
    delay,
    timerClass.TYPE_ONE_SHOT);
  return timerID;
}

// === {{{ Utils.clearTimeout(timerID) }}} ===
// Given a {{{timerID}}} returned by {{{Utils.setTimeout()}}},
// prevents the callback from ever being called.
// Returns {{{true}}} if the timer is cancelled, {{{false}}} if already fired.

function clearTimeout(timerID) {
  if (!(timerID in gTimers)) return false;
  gTimers[timerID].cancel();
  delete gTimers[timerID];
  return true;
}

// Support infrastructures for the timeout-related functions.
var gTimers = {};
var gNextTimerID = 1;
function __TimerCallback(id, cb, args) {
  this._id = id;
  this._callback = typeof cb === "function" ? cb : Function("_", cb);
  this._args = args;
}
__TimerCallback.prototype = {
  notify: function TC_notify(timer) {
    delete gTimers[this._id];
    this._callback.apply(null, this._args);
  },
  QueryInterface: undefined, // XPCOMUtils.generateQI([Ci.nsITimerCallback]),
};

// === {{{ Utils.uri(spec, defaultUrl) }}} ===
// Given a string representing an absolute URL or a {{{nsIURI}}}
// object, returns an equivalent {{{nsIURI}}} object.  Alternatively,
// an object with keyword arguments as keys can also be passed in; the
// following arguments are supported:
// * {{{uri}}} is a string or {{{nsIURI}}} representing an absolute or
//   relative URL.
// * {{{base}}} is a string or {{{nsIURI}}} representing an absolute
//   URL, which is used as the base URL for the {{{uri}}} keyword argument.
//
// An optional second argument may also be passed in, which specifies
// a default URL to return if the given URL can't be parsed.

function uri(spec, defaultUri) {
  var base = null;
  if (typeof spec === "object") {
    if (spec instanceof Ci.nsIURI)
      // nsIURI object was passed in, so just return it back
      return spec;

    // Assume jQuery-style dictionary with keyword args was passed in.
    base = "base" in spec ? uri(spec.base, defaultUri) : null;
    spec = spec.uri || null;
  }
  try {
      return { }
    return Utils.IOService.newURI(spec, null, base);
  } catch (e if defaultUri) {
    return uri(defaultUri);
  }
}
// === {{{ Utils.url(spec, defaultUrl) }}} ===
// Alias of {{{Utils.uri()}}}.
Utils.url = uri;

// === {{{ Utils.openUrlInBrowser(urlString, postData) }}} ===
// Opens the given URL in the user's browser, using
// their current preferences for how new URLs should be opened (e.g.,
// in a new window vs. a new tab, etc).
// Returns the opened page as {{{Utils.BrowserTab}}} or {{{ChromeWindow}}}.
//
// {{{urlString}}} is a string corresponding to the URL to be opened.
//
// {{{postData}}} is an optional argument that allows HTTP POST data
// to be sent to the newly-opened page.  It may be a string, an Object
// with keys and values corresponding to their POST analogues, or an
// {{{nsIInputStream}}}.

function openUrlInBrowser(urlString, postData) {
  var postInputStream = null;
  if (postData) {
    if (postData instanceof Ci.nsIInputStream)
      postInputStream = postData;
    else {
      if (typeof postData === "object")
        postData = paramsToString(postData, "");

      var stringStream = (Cc["@mozilla.org/io/string-input-stream;1"]
                          .createInstance(Ci.nsIStringInputStream));
      stringStream.data = postData;

      postInputStream = (Cc["@mozilla.org/network/mime-input-stream;1"]
                         .createInstance(Ci.nsIMIMEInputStream));
      postInputStream.addHeader("Content-Type",
                                "application/x-www-form-urlencoded");
      postInputStream.addContentLength = true;
      postInputStream.setData(stringStream);
    }
  }

  var browserWindow = Utils.currentChromeWindow;
  var browser = browserWindow.gBrowser;
  var openPref = gPrefBranch.getIntPref("browser.link.open_newwindow");

  //3 (default in Firefox 2 and above): In a new tab
  //2 (default in SeaMonkey and Firefox 1.5): In a new window
  //1 (or anything else): In the current tab or window
  if (browser.mCurrentBrowser.currentURI.spec !== "about:blank" ||
      browser.webProgress.isLoadingDocument) {
    if (openPref === 3) {
      let tab = browser.addTab(
        urlString, null, null, postInputStream, false, false);
      let fore = !gPrefBranch.getBoolPref(
        "browser.tabs.loadDivertedInBackground");
      let {shiftKey} = (browserWindow.gUbiquity || 0).lastKeyEvent || 0;
      if (fore ^ shiftKey) browser.selectedTab = tab;
      return BrowserTab(tab);
    }
    if (openPref === 2) {
      return browserWindow.openDialog(
        "chrome://browser/content", "_blank", "all,dialog=no",
        urlString, null, null, postInputStream);
    }
  }
  browserWindow.loadURI(urlString, null, postInputStream, false);
  return BrowserTab(browser.mCurrentTab);
}

// === {{{ Utils.focusUrlInBrowser(urlString) }}} ===
// Focuses a tab with the given URL if one exists;
// otherwise, it delegates the opening of the URL in a
// new window or tab to {{{Utils.openUrlInBrowser()}}}.

function focusUrlInBrowser(urlString) {
  var it = gTabs.get(urlString)[0] || openUrlInBrowser(urlString);
  if (it instanceof BrowserTab) setTimeout(function focusLater() {
    it.chromeWindow.focus();
    it.focus();
  });
  return it;
}

// === {{{ Utils.getCookie(domain, name) }}} ===
// Returns the cookie for the given {{{domain}}} and with the given {{{name}}}.
// If no matching cookie exists, {{{null}}} is returned.

function getCookie(domain, name) {
  var cookieManager =
    Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);
  var {nsICookie} = Ci, iter = cookieManager.enumerator;
  while (iter.hasMoreElements()) {
    var cookie = iter.getNext();
    if (cookie instanceof nsICookie &&
        cookie.host === domain &&
        cookie.name === name)
      return cookie.value;
  }
  // if no matching cookie:
  return null;
}

// === {{{ Utils.paramsToString(params, prefix = "?") }}} ===
// Takes the given object containing keys and values into a query string
// suitable for inclusion in an HTTP GET or POST request.
//
// {{{params}}} is the object of key-value pairs.
//
// {{{prefix}}} is an optional string prepended to the result,
// which defaults to {{{"?"}}}.

function paramsToString(params, prefix) {
  var stringPairs = [];
  function addPair(key, value) {
    // explicitly ignoring values that are functions/null/undefined
    if (typeof value !== "function" && value != null)
      stringPairs.push(
        encodeURIComponent(key) + "=" + encodeURIComponent(value));
  }
  for (var key in params)
    if (Utils.isArray(params[key]))
      params[key].forEach(function p2s_each(item) { addPair(key, item) });
    else
      addPair(key, params[key]);
  return (prefix == null ? "?" : prefix) + stringPairs.join("&");
}

// === {{{ Utils.urlToParams(urlString) }}} ===
// Given a {{{urlString}}}, returns an object containing keys and values
// retrieved from its query-part.

function urlToParams(url) {
  var params = {}, dict = {__proto__: null};
  for each (let param in /^(?:[^?]*\?)?([^#]*)/.exec(url)[1].split("&")) {
    let [key, val] = /[^=]*(?==?(.*))/.exec(param);
    val = val.replace(/\+/g, " ");
    try { key = decodeURIComponent(key) } catch (e) {};
    try { val = decodeURIComponent(val) } catch (e) {};
    params[key] = key in dict ? [].concat(params[key], val) : val;
    dict[key] = 1;
  }
  return params;
}

// === {{{ Utils.getLocalUrl(urlString, charset) }}} ===
// Synchronously retrieves the content of the given local URL,
// such as a {{{file:}}} or {{{chrome:}}} URL, and returns it.
//
// {{{url}}} is the URL to retrieve.
//
// {{{charset}}} is an optional string to specify the character set.

function getLocalUrl(url, charset) {
    return "";
  var req = (Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
             .createInstance(Ci.nsIXMLHttpRequest));
  req.open("GET", url, false);
  req.overrideMimeType("text/plain" + (charset ? ";charset=" + charset : ""));
  req.setRequestHeader("If-Modified-Since", "Thu, 01 Jun 1970 00:00:00 GMT");
  req.send(null);
  if (req.status !== 0) throw Error("failed to get " + url);
  return req.responseText;
}

// ==={{{ Utils.parseHtml(htmlText, callback) }}}===
// Parses {{{htmlText}}} to a DOM document and passes it to {{{callback}}}.

function parseHtml(htmlText, callback) {
  var {document} = Utils.currentChromeWindow;
  var iframe = document.createElement("iframe");
  iframe.setAttribute("collapsed", true); // hide
  iframe.setAttribute("type", "content"); // secure
  var {docShell} = document.documentElement.appendChild(iframe);
  docShell.QueryInterface(Ci.nsIWebNavigation)
    .stop(Ci.nsIWebNavigation.STOP_NETWORK); // stop loading about:blank
  // turn off unneeded/unwanted/bad things
  docShell.allowJavascript    =
  docShell.allowAuth          =
  docShell.allowPlugins       =
  docShell.allowMetaRedirects =
  docShell.allowSubframes     =
  docShell.allowImages        = false;
  docShell.loadURI(
    'data:text/html;charset=UTF-8,' + encodeURIComponent(htmlText),
    Ci.nsIWebNavigation, null, null, null);
  listenOnce(iframe, "DOMContentLoaded", function onParsed() {
    callback(iframe.contentDocument);
    iframe.parentNode.removeChild(iframe);
  }, true);
}

// ** {{{ Utils.trim(str) }}} **
// **//Deprecated.//** Use native {{{trim()}}} instead.
Utils.trim = String.trim;

// === {{{ Utils.sort(array, key, descending = false) }}} ===
// Sorts an {{{array}}} without implicit string conversion and returns it,
// optionally performing Schwartzian Transformation
// by specified {{{key}}}. e.g.:
// {{{
// [42, 16, 7].sort() //=> [16, 42, 7]
// sort([42, 16, 7])  //=> [7, 16, 42]
// sort(["abc", "d", "ef"], "length") //=> ["d", "ef", "abc"]
// sort([1, 2, 3], function (x) -x)   //=> [3, 2, 1]
// }}}
//
// {{{array}}} is the target array.
//
// {{{key}}} is an optional string specifying the key property
// or a function that maps each of {{{array}}}'s item to a sort key.
//
// Sorts descending if {{{descending}}}.

function sort(array, key, descending) {
  array.forEach(function transform(v, i, a) a[i] = {key: this(v), val: v},
                typeof key === "function" ? key :
                (key != null
                 ? function pry(x) x[key]
                 : function idt(x) x));
  // Because our Monkey uses Merge Sort, "swap the values if plus" works.
  array.sort(descending
             ? function dsc(a, b) a.key < b.key
             : function asc(a, b) a.key > b.key);
  array.forEach(function mrofsnart(v, i, a) a[i] = v.val);
  return array;
}
Utils.sortBy = Utils.sort;

// === {{{ Utils.uniq(array, key, strict = false) }}} ===
// Removes duplicates from an array by comparing string versions of them
// (or strict equality if {{{strict}}} evaluates to {{{true}}}) and returns it.
// If {{{key}}} is provided, the comparison is made to mappings of it instead.
// {{{
// uniq([1, 1.0, "1", [1]])         //=> [1, "1", [1]]
// uniq([1, 1.0, "1", [1]], Number) //=> [1]
// uniq([{}, {}, {}], null)         //=> [{}]
// uniq([{}, {}, {}], null, true)   //=> [{}, {}, {}]
// }}}
//
// {{{array}}} is the target array.
//
// {{{key}}} is an optional function or string
// ({{{"foo"}}} is identical to {{{function(x) x.foo}}} for this purpose)
// that maps each of {{{array}}}'s item to a comparison key.
//
// {{{strict}}} is an optional flag to make {{{uniq()}}} use
// **{{{===}}}** instead of {{{toString()}}}. Accurate, but slower.

function uniq(array, key, strict) {
  var f = (key == null
           ? function identity(x) x
           : typeof key === "function" ? key : function pluck(x) x[key]);
  var args = [0, 1/0];
  if (strict) {
    let keys = [];
    for each (let x in array) let (k = f(x)) {
      if (~keys.indexOf(k)) continue;
      keys.push(k);
      args.push(x);
    }
  }
  else {
    let dict = {__proto__: null};
    for each (let x in array) let (k = f(x)) {
      if (k in dict) continue;
      dict[k] = 1;
      args.push(x);
    }
  }
  array.splice.apply(array, args);
  return array;
}

// === {{{ Utils.isArray(value) }}} ===
// Returns whether or not the {{{value}}} is an {{{Array}}} instance.

Utils.isArray = Array.isArray;

// === {{{ Utils.isEmpty(value) }}} ===
// Returns whether or not the {{{value}}} has no own properties.

function isEmpty(val) !keys(val).length;

// === {{{ Utils.classOf(value) }}} ===
// Returns the internal {{{[[Class]]}}} property of the {{{value}}}.
// See [[http://bit.ly/CkhjS#instanceof-considered-harmful]].

function classOf(val) TO_STRING.call(val).slice(8, -1);

// === {{{ Utils.count(object) }}} ===
// Returns the number of {{{object}}}'s own properties,
// emulating (now obsolete) {{{__count__}}}. See [[http://bugzil.la/551529]].

function count(obj) keys(obj).length;

// === {{{ Utils.keys(object) }}} ===
// Returns an array of all own, enumerable property names of {{{object}}}.

function keys(obj) Object.keys(Object(obj));

// === {{{ Utils.powerSet(set) }}} ===
// Creates a [[http://en.wikipedia.org/wiki/Power_set|power set]] of
// an array like {{{set}}}. e.g.:
// {{{
// powerSet([0,1,2]) // [[], [0], [1], [0,1], [2], [0,2], [1,2], [0,1,2]]
// powerSet("ab")    // [[], ["a"], ["b"], ["a","b"]]
// }}}

function powerSet(arrayLike) {
  var ps = [[]];
  for (let i = 0, l = arrayLike.length; i < l; ++i) {
    let next = [arrayLike[i]];
    for each (let a in ps) ps.push(a.concat(next));
  }
  return ps;
}

// === {{{ Utils.seq(lead_or_count, end, step = 1) }}} ===
// Creates an iterator of simple number sequence.
// {{{
// [i for (i in seq(1, 3))]     // [1, 2, 3]
// [i for (i in seq(3))]        // [0, 1, 2]
// [i for (i in seq(4, 2, -1))] // [4, 3, 2]
// seq(-7).slice(2, -2)         // [4, 3, 2]
// }}}

Utils.seq = Sequence;
function Sequence(lead, end, step) {
  if (end == null && lead)
    [lead, end, step] = lead < 0 ? [~lead, 0, -1] : [0, ~-lead];
  return {
    __proto__: Sequence.prototype,
    lead: +lead, end: +end, step: +step || 1,
  };
}
extend(Sequence.prototype, {
  __iterator__: function seq_iter() {
    var {lead: i, end, step} = this;
    if (step < 0)
      for (; i >= end; i += step) yield i;
    else
      for (; i <= end; i += step) yield i;
  },
  __noSuchMethod__:
  function seq_pass(name, args) args[name].apply(this.toJSON(), args),
  get length() (this.end - this.lead) / this.step + 1 | 0,
  toJSON: function seq_toJSON() [x for (x in this)],
  toString: function seq_toString()
    "[object Sequence(" + this.lead + "," + this.end + "," + this.step + ")]",
});

// === {{{ Utils.computeCryptoHash(algo, str) }}} ===
// Computes and returns a cryptographic hash for a string given an
// algorithm.
//
// {{{algo}}} is a string corresponding to a valid hash algorithm.  It
// can be any one of {{{MD2}}}, {{{MD5}}}, {{{SHA1}}}, {{{SHA256}}},
// {{{SHA384}}}, or {{{SHA512}}}.
//
// {{{str}}} is the string to be hashed.

function computeCryptoHash(algo, str) {
  var converter = Utils.UnicodeConverter;
  converter.charset = "UTF-8";
  var data = converter.convertToByteArray(str, {});
  var crypto = (Cc["@mozilla.org/security/hash;1"]
                .createInstance(Ci.nsICryptoHash));
  crypto.initWithString(algo);
  crypto.update(data, data.length);
  var hash = crypto.finish(false);
  var hexHash = "";
  for each (var c in hash)
    hexHash += (c.charCodeAt(0) | 256).toString(16).slice(-2);
  return hexHash;
}

// === {{{ Utils.signHmac(algo, key, str) }}} ===
// Computes and returns a cryptographicly signed hash for a string given an
// algorithm. It is derived from a given key.
//
// {{{algo}}} is a string corresponding to a valid hash algorithm.  It
// can be any one of {{{MD2}}}, {{{MD5}}}, {{{SHA1}}}, {{{SHA256}}},
// {{{SHA384}}}, or {{{SHA512}}}.
//
// {{{key}}} is a key string to sign with.
//
// {{{str}}} is the string to be hashed.

function signHmac(algo, key, str) {
  var converter = Utils.UnicodeConverter;
  converter.charset = "UTF-8";
  var data = converter.convertToByteArray(str, {});
  var crypto = (Cc["@mozilla.org/security/hmac;1"]
                .createInstance(Ci.nsICryptoHMAC));
  var keyObject = (Cc["@mozilla.org/security/keyobjectfactory;1"]
                   .getService(Ci.nsIKeyObjectFactory)
                   .keyFromString(Ci.nsIKeyObject.HMAC, key));
  crypto.init(Ci.nsICryptoHMAC[algo], keyObject);
  crypto.update(data, data.length);
  return crypto.finish(true);
}
Utils.signHMAC = signHmac;

// === {{{ Utils.escapeHtml(string) }}} ===
// Returns a version of the {{{string}}} safe for insertion into HTML.
// Useful when you just want to concatenate a bunch of strings into
// an HTML fragment and ensure that everything's escaped properly.

function escapeHtml(s) String(s).replace(escapeHtml.re, escapeHtml.fn);
escapeHtml.re = /[&<>\"\']/g;
escapeHtml.fn = function escapeHtml_sub($) {
  switch ($) {
    case "&": return "&amp;";
    case "<": return "&lt;";
    case ">": return "&gt;";
    case '"': return "&quot;";
    case "'": return "&#39;";
  }
};

// === {{{ Utils.unescapeHtml(string) }}} ===
// Returns a version of the {{{string}}} with all occurrences of HTML character
// references (e.g. &spades; &#x2665; &#9827; etc.) in it decoded.

function unescapeHtml(s)
  String(s).replace(unescapeHtml.re, Utils.UnescapeHTML.unescape);
unescapeHtml.re = /(?:&#?\w+;)+/g;

// === {{{ Utils.convertFromUnicode(toCharset, text) }}} ===
// Encodes the given unicode text to a given character set.
//
// {{{toCharset}}} is a string corresponding to the character set
// to encode to.
//
// {{{text}}} is a unicode string.

function convertFromUnicode(toCharset, text) {
  var converter = Utils.UnicodeConverter;
  converter.charset = toCharset;
  return converter.ConvertFromUnicode(text) + converter.Finish();
}

// === {{{ Utils.convertToUnicode(fromCharset, text) }}} ===
// Decodes the given text from a character set to unicode.
//
// {{{fromCharset}}} is a string corresponding to the character set to
// decode from.
//
// {{{text}}} is a string encoded in the character set {{{fromCharset}}}.

function convertToUnicode(fromCharset, text) {
  var converter = Utils.UnicodeConverter;
  converter.charset = fromCharset;
  return converter.ConvertToUnicode(text);
}

// === {{{ Utils.notify(label, value, image, priority, buttons, target) }}} ===
// A wrapper function for
// [[http://bit.ly/mdc_notificationbox#m-appendNotification|notificationbox]].
//
// * {{{label}}} : The text to show.
// * {{{value}}} : An optional identifier string. Defaults to {{{label}}}.
// * {{{image}}} : An optional URL string for the icon.
// * {{{priority}}} : An optional string to specify the priority.
//   Defaults to {{{"INFO_LOW"}}}.
// * {{{buttons}}} : The button descriptions. See the above link for details.
// * {{{target}}} : An optional browser/window/document element to which
//   the notification is appended. Defaults to the current page.
//
// For ease of use, the arguments can be passed as a dictionary in place of
// {{{label}}}. e.g.:
// {{{
// notify({label: "!", value: "foo", priority: "Critical_Block"});
// }}}

function notify(label, value, image, priority, buttons, target) {
  if (classOf(label) === "Object")
    // pseudo keyword arguments
    var {label, value, image, priority, buttons, target} = label;
  if (!target) {
    var tabbrowser = Utils.currentChromeWindow.gBrowser;
    var foundBrowser = tabbrowser.selectedBrowser;
  }
  else {
    taget = target.contentDocument || target.document || target;
    // Find the <browser> which contains notifyWindow, by looking
    // through all the open windows and all the <browsers> in each.
    var enumerator = Utils.WindowMediator.getEnumerator(Utils.appWindowType);
    while (!foundBrowser && enumerator.hasMoreElements()) {
      tabbrowser = enumerator.getNext().gBrowser;
      foundBrowser = tabbrowser.getBrowserForDocument(target);
    }
  }
  if (!foundBrowser) {
    Cu.reportError("Couldn't find tab for document");
    return null;
  }
  value || (value = label);
  var box = tabbrowser.getNotificationBox(foundBrowser);
  var oldNotification = box.getNotificationWithValue(value);
  if (oldNotification) box.removeNotification(oldNotification);
  return box.appendNotification(
    label,
    value,
    image || "/ubiquity/chrome/skin/icons/favicon.ico",
    box[("PRIORITY_" + priority).toUpperCase()] || box.PRIORITY_INFO_LOW,
    buttons);
}

// === {{{ Utils.listenOnce(element, eventType, listener, useCapture) }}} ===
//
// Same as [[https://developer.mozilla.org/en/DOM/element.addEventListener]],
// except that the {{{listener}}} will be automatically removed on its
// first execution.
// Returns the listening wrapper function which can be called/removed
// manually if needed.

function listenOnce(element, eventType, listener, useCapture) {
  function listener1(event) {
    element.removeEventListener(eventType, listener1, useCapture);
    if (typeof listener === "function")
      listener.call(this, event);
    else
      listener.handleEvent(event);
  }
  element.addEventListener(eventType, listener1, useCapture);
  return listener1;
}

// === {{{ Utils.defineLazyProperty(obj, func, name) }}} ===
// Defines a temporary getter {{{name}}} (or {{{func.name}}} if omitted)
// to {{{obj}}}, which will be replaced with the return value of {{{func}}}
// after the first access.

function defineLazyProperty(obj, func, name) {
  if (typeof name !== "string") name = func.name;
  obj.__defineGetter__(name, function lazyProperty() {
    delete obj[name];
    return obj[name] = func.call(obj);
  });
  return obj;
}

// === {{{ Utils.extend(target, object1, [objectN ...]) }}} ===
// Extends {{{target}}} by copying properties from the rest of arguments.
// Deals with getters/setters properly. Returns {{{target}}}.

function extend(target) {
  for (let i = 1, l = arguments.length; i < l; ++i) {
    let obj = arguments[i];
    for each (let key in keys(obj)) {
      let g, s;
      (g = obj.__lookupGetter__(key)) && target.__defineGetter__(key, g);
      (s = obj.__lookupSetter__(key)) && target.__defineSetter__(key, s);
      g || s || (target[key] = obj[key]);
    }
  }
  return target;
}

// == {{{ Utils.prefs }}} ==
// Proxy to {{{nsIPrefBranch2}}} set to root.

const {PREF_STRING, PREF_BOOL, PREF_INT} = [ ]; // Ci.nsIPrefBranch;
var gPrefBranch = {
    getPrefType: function (name) {
        $.dbgpr("Utils.prefs: getPrefType: " + name);
        return { };
    },
    getComplexValue: function (name) {
        $.dbgpr("Utils.prefs: getComplexPref: " + name);
        return { };
    },
    getBoolPref: function (name) {
        $.dbgpr("Utils.prefs: getBoolPref: " + name);
        return { };
    },
    getIntPref: function (name) {
        $.dbgpr("Utils.prefs: getIntPref: " + name);
        return { };
    },
}

var myprefs = { };

var gPrefs = Utils.prefs = {
  // === {{{ Utils.prefs.getValue(name, value = undefined) }}} ===
  // === {{{ Utils.prefs.setValue(name, value) }}} ===
  // Copycats of
  // [[https://developer.mozilla.org/en/Toolkit_API/extIPreferenceBranch]]'s
  // namesakes. Also available in the names of {{{get()}}} and {{{set()}}}.
  get: function prefs_get(name, value) {
      return value;

    switch (gPrefBranch.getPrefType(name)) {
      case PREF_STRING:
      try {
        return gPrefBranch.getComplexValue(
          name, Ci.nsIPrefLocalizedString).data;
      } catch ([]) {}
      return gPrefBranch.getComplexValue(name).data;
      case PREF_BOOL:
      return gPrefBranch.getBoolPref(name);
      case PREF_INT:
      return gPrefBranch.getIntPref(name);
    }
    return value;
  },
  set: function prefs_set(name, value) {
      myprefs[value] = name;
      return value;

    switch (typeof value) {
      case "string": {
        let ss = (Cc["@mozilla.org/supports-string;1"]
                  .createInstance(nsISupportsString));
        ss.data = value;
        gPrefBranch.setComplexValue(name, nsISupportsString, ss);
      } break;
      case "boolean": gPrefBranch.setBoolPref(name, value); break;
      case "number": gPrefBranch.setIntPref(name, value); break;
      default: throw TypeError("invalid pref value");
    }
    return value;
  },
  // === {{{ Utils.prefs.reset(name) }}} ===
  // Resets the {{{name}}}d preference to the default value.
  // Returns a boolean indicating whether or not the reset succeeded.
  reset: function prefs_reset(name)
    gPrefBranch.prefHasUserValue(name) && !gPrefBranch.clearUserPref(name),
  // === {{{ Utils.prefs.resetBranch(name) }}} ===
  // Resets all preferences that start with {{{name}}} to the default values.
  // Returns an array of preference names that were reset.
  resetBranch: function prefs_resetBranch(name) {
    var names = (gPrefBranch.getChildList(name, {})
                 .filter(gPrefBranch.prefHasUserValue));
    names.forEach(gPrefBranch.clearUserPref);
    return names;
  },
  __noSuchMethod__:
  function prefs_pass(name, args) gPrefBranch[name].apply(gPrefBranch, args),
};
gPrefs.getValue = gPrefs.get;
gPrefs.setValue = gPrefs.set;

// === {{{ Utils.BrowserTab(tabbrowser_tab) }}} ===
// Wrapper for browser tabs. Supports roughly the same features as
// https://developer.mozilla.org/en/FUEL/BrowserTab
// (minus {{{events}}}, to avoid memory leaks).

function BrowserTab(tabbrowser_tab) ({
  __proto__: BrowserTab.prototype,
  raw: tabbrowser_tab,
});
extend(BrowserTab.prototype, {
  get browser() this.raw.linkedBrowser,
  get tabbrowser() this.browser.getTabBrowser(),
  get uri() this.browser.currentURI,
  get title() this.browser.contentTitle,
  get window() this.browser.contentWindow,
  get document() this.browser.contentDocument,
  get icon() this.raw.image,
  get index() {
    var {browser} = this, {mTabs} = browser.getTabBrowser();
    for (let i = 0, l = mTabs.length; i < l; ++i)
      if (mTabs[i].linkedBrowser === browser) return i;
    return -1;
  },
  get chromeWindow() {
    var {tabbrowser} = this;
    for each (let win in Utils.chromeWindows)
      if (win.gBrowser === tabbrowser) return win;
    return null;
  },
  toJSON: function BT_toJSON() this.uri.spec,
  toString: function BT_toString() "[object BrowserTab]",
  valueOf: function BT_valueOf() this.index,
  load: function BT_load(uri, referrer, charset) {
    this.browser.loadURI(uri.spec || uri, referrer, charset);
    return this;
  },
  focus: function BT_focus() {
    var {chromeWindow, tabbrowser} = this;
    tabbrowser.selectedTab = this.raw;
    chromeWindow.focus();
    tabbrowser.focus();
    chromeWindow.content.focus();
    return this;
  },
  close: function BT_close() {
    this.tabbrowser.removeTab(this.raw);
    return this;
  },
  moveBefore: function BT_moveBefore(target) {
    this.tabbrowser.moveTabTo(this.raw, (target || 0).index || target);
    return this;
  },
  moveToEnd: function BT_moveToEnd() {
    var {tabbrowser} = this;
    tabbrowser.moveTabTo(this.raw, tabbrowser.browsers.length);
    return this;
  },
});

// == {{{ Utils.tabs }}} ==
// Iterates all tabs currently opened as {{{Utils.BrowserTab}}}.
// Also contains functions related to them.
var gTabs = Utils.tabs = {
  toString: function tabs_toString() "[object tabs]",
  valueOf: function tabs_valueOf() this.length,
  __iterator__: function tabs_iter() {
    for each (var win in Utils.chromeWindows) if ("gBrowser" in win) {
      let {mTabs} = win.gBrowser, i = -1, l = mTabs.length;
      while (++i < l) yield BrowserTab(mTabs[i]);
    }
  },

  // === {{{ Utils.tabs.length }}} ===
  // The total number of opened tabs.
  get length() {
    var num = 0;
    for each (var win in Utils.chromeWindows) if ("gBrowser" in win)
      num += win.gBrowser.mTabs.length;
    return num;
  },

  // === {{{ Utils.tabs.get(name) }}} ===
  // Returns an array of opened tabs. If {{{name}}} is supplied, returns only
  // the ones whose title or URL exactly match with it.
  get: function tabs_get(name)
    name == null
    ? [t for (t in gTabs)]
    : [t for (t in gTabs)
       if (let (d = t.document)
           d.title === name || d.location.href === name)],

  // === {{{ Utils.tabs.search(matcher, maxResults) }}} ===
  // Searches for tabs by title or URL and returns an array of tab references.
  // The match result is set to {{{tab.match}}}.
  //
  // {{{matcher}}} is a string or {{{RegExp}}} object to match with.
  //
  // {{{maxResults}}} is an optinal integer specifying
  // the maximum number of results to return.
  search: function tabs_search(matcher, maxResults) {
    var results = [], matcher = regexp(matcher, "i");
    if (maxResults == null) maxResults = 1/0;
    for (let tab in gTabs) {
      let {document} = tab;
      let match = matcher.exec(document.title) ||
                  matcher.exec(document.location.href);
      if (!match) continue;
      tab.match = match;
      if (results.push(tab) >= maxResults) break;
    }
    return results;
  },

  // === {{{ Utils.tabs.reload(matcher) }}} ===
  // Reloads all tabs {{{matcher}}} matches.
  reload: function tabs_reload(matcher) {
    for each (let tab in gTabs.search(matcher)) tab.browser.reload();
    return this;
  },

  // === {{{ Utils.tabs.from(container) }}} ===
  // Returns tabs within {{{container}}}, which can be
  // either a {{{ChromeWindow}}} or {{{tabbrowser}}}.
  from: function tabs_from(container)
    Array.map((container.gBrowser || container).mTabs || [], BrowserTab),
};

// == {{{ Utils.clipboard }}} ==
// Provides clipboard access.

var gClipboard = Utils.clipboard = {
  flavors: {
    text: "text/unicode",
    html: "text/html",
    image: "image/png",
  },

  // === {{{ Utils.clipboard.get(flavor) }}} ===
  // Gets the clipboard content(s) of specified flavor(s).\\
  // {{{flavor}}} can be either a string or array of strings. e.g.:
  // {{{
  // var txt = Utils.clipboard.get("text/unicode");
  // var [txt, htm] = Utils.clipboard.get(["text", "html"]);
  // }}}
  get: function clipboard_get(flavor) {
    const {service, service: {kGlobalClipboard}, flavors} = gClipboard;
    function get(flavor) {
      flavor = flavors[flavor] || flavor;
      if (!service.hasDataMatchingFlavors([flavor], 1, kGlobalClipboard))
        return "";
      var data = {}, trans = (Cc["@mozilla.org/widget/transferable;1"]
                              .createInstance(nsITransferable));
      'init' in trans && trans.init(null);
      trans.addDataFlavor(flavor);
      service.getData(trans, kGlobalClipboard);
      trans.getTransferData(flavor, data, {});
      if (~flavor.lastIndexOf("text/", 0))
        return data.value.QueryInterface(nsISupportsString).data;
      var bis = (Cc["@mozilla.org/binaryinputstream;1"]
                 .createInstance(Ci.nsIBinaryInputStream));
      bis.setInputStream(data.value.QueryInterface(Ci.nsIInputStream));
      return "data:" + flavor + ";base64," +
        btoa(String.fromCharCode.apply(0, bis.readByteArray(bis.available())));
    }
    return (
      arguments.length > 1
      ? Array.map(arguments, get)
      : Utils.isArray(flavor) ? flavor.map(get) : get(flavor));
  },

  // === {{{ Utils.clipboard.set(dict) }}} ===
  // Sets the clipboard content(s) of specified flavor(s).\\
  // {{{dict}}} should be a dictionary of flavor:data pairs.
  set: function clipboard_set(dict) {
    const {service, flavors} = gClipboard;
    var trans = (Cc["@mozilla.org/widget/transferable;1"]
                 .createInstance(nsITransferable));
    'init' in trans && trans.init(null);
    for (let [flavor, data] in new Iterator(dict)) {
      if (~flavor.lastIndexOf("image/", 0)) {
        gClipboard.image = data;
        return;
      }
      let ss = (Cc["@mozilla.org/supports-string;1"]
                .createInstance(nsISupportsString));
      ss.data = data = String(data);
      trans.addDataFlavor(flavor = flavors[flavor] || flavor);
      trans.setTransferData(flavor, ss, data.length * 2);
    }
    service.setData(trans, null, service.kGlobalClipboard);
  },
};
defineLazyProperty(gClipboard, function service() {
  return Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard);
});
for (let n in gClipboard.flavors) let (name = n) {
  gClipboard.__defineGetter__(name, function getCB() gClipboard.get(name));
}
// === {{{ Utils.clipboard.text }}} ===
// Gets or sets the clipboard text.
gClipboard.__defineSetter__(
  "text", function clipboard_setText(txt) gClipboard.set({text: txt}));
// === {{{ Utils.clipboard.html }}} ===
// Gets or sets the clipboard HTML text.
// Also accepts a DOM node when setting.
gClipboard.__defineSetter__("html", function clipboard_setHtml(htm) {
  gClipboard.set(htm instanceof Ci.nsIDOMNode ? {
    text: (htm.documentElement || htm).textContent || htm.nodeValue || '',
    html: new Utils.hiddenWindow.XMLSerializer().serializeToString(htm),
  } : {text: htm, html: htm});
});
// === {{{ Utils.clipboard.image }}} ===
// Gets or sets the clipboard image as data URL.
// Also accepts an {{{<img>}}} element when setting.
gClipboard.__defineSetter__("image", function clipboard_setImage(img) {
  var win = Utils.currentChromeWindow, doc = win.document;
  if (typeof img === "string") {
    let i = new win.Image;
    i.src = img;
    img = i;
  }
  img.complete ? onload() : img.onload = onload;
  function onload() {
    const CMD_CIC = "cmd_copyImageContents";
    var {popupNode} = doc;
    doc.popupNode = img;
    var controller = doc.commandDispatcher.getControllerForCommand(CMD_CIC);
    if (controller.isCommandEnabled(CMD_CIC)) controller.doCommand(CMD_CIC);
    doc.popupNode = popupNode;
  }
});

// == {{{ Utils.history }}} ==
// Contains functions that make it easy to access
// information about the user's browsing history.

Utils.history = {
  // === {{{ Utils.history.visitsToDomain(domain) }}} ===
  // Returns the number of times the user has visited
  // the given {{{domain}}} string.
  visitsToDomain: function history_visitsToDomain(domain) {
    var hs = (Cc["@mozilla.org/browser/nav-history-service;1"]
              .getService(Ci.nsINavHistoryService));
    var query = hs.getNewQuery();
    var options = hs.getNewQueryOptions();
    query.domain = domain;
    options.maxResults = 10;
    // execute query
    var count = 0;
    var {root} = hs.executeQuery(query, options);
    root.containerOpen = true;
    for (let i = root.childCount; i--;)
      count += root.getChild(i).accessCount;
    root.containerOpen = false;
    return count;
  },

  // === {{{ Utils.history.search(query, callback) }}} ===
  // Searches the pages the user has visited.
  // Given a query string and a callback function, passes an array of results
  // (objects with {{{url}}}, {{{title}}} and {{{favicon}}} properties)
  // to the callback.
  //
  // {{{query}}} is the query string.
  //
  // {{{callback}}} is the function called when the search is complete.
  search: function history_search(query, callback) {
    var awesome = (Cc["@mozilla.org/autocomplete/search;1?name=history"]
                   .getService(Ci.nsIAutoCompleteSearch));
    awesome.startSearch(query, "", null, {
      onSearchResult: function hs_onSearchResult(search, result) {
        switch (result.searchResult) {
          case result.RESULT_SUCCESS:
          callback([{
            url: result.getValueAt(i),
            title: result.getCommentAt(i),
            favicon: result.getImageAt(i).replace(/^moz-anno:favicon:/, ""),
          } for (i in Sequence(result.matchCount))]);
          break;

          case result.RESULT_IGNORED:
          case result.RESULT_FAILURE:
          case result.RESULT_NOMATCH:
          callback([]);
        }
      }
    });
  },
};

// == {{{ Utils.regexp(pattern, flags) }}} ==
// Creates a regexp just like {{{RegExp}}}, except that it:
// * falls back to a quoted version of {{{pattern}}} if the compile fails
// * returns the {{{pattern}}} as is if it's already a regexp
//
// {{{
// RegExp("[")          // SyntaxError("unterminated character class")
// RegExp(/:/, "y")     // TypeError("can't supply flags when ...")
// regexp("[")          // /\[/
// regexp(/:/, "y")     // /:/
// }}}
// Also contains regexp related functions.

function regexp(pattern, flags) {
  if (classOf(pattern) === "RegExp") return pattern;
  try {
    return RegExp(pattern, flags);
  } catch (e if e instanceof SyntaxError) {
    return RegExp(regexp.quote(pattern), flags);
  }
}

// === {{{ Utils.regexp.quote(string) }}} ===
// Returns the {{{string}}} with all regexp meta characters in it backslashed.

regexp.quote = function re_quote(string)
  String(string).replace(/[.?*+^$|()\{\[\\]/g, "\\$&");

// === {{{ Utils.regexp.Trie(strings, asPrefixes) }}} ===
// Creates a {{{RegexpTrie}}} object that builds an efficient regexp
// matching a specific set of {{{strings}}}.
// This is a JS port of
// [[http://search.cpan.org/~dankogai/Regexp-Trie-0.02/lib/Regexp/Trie.pm]]
// with a few additions.
//
// {{{strings}}} is an optional array of strings to {{{add()}}}
// (or {{{addPrefixes()}}} if {{{asPrefixes}}} evaluates to {{{true}}})
// on initialization.

regexp.Trie = function RegexpTrie(strings, asPrefixes) {
  var me = {$: {__proto__: null}, __proto__: RegexpTrie.prototype};
  if (strings) {
    let add = asPrefixes ? "addPrefixes" : "add";
    for each (let str in strings) me[add](str);
  }
  return me;
};
extend(regexp.Trie.prototype, {
  // ** {{{ RegexpTrie#add(string) }}} **\\
  // Adds {{{string}}} to the Trie and returns self.
  add: function RegexpTrie_add(string) {
    var ref = this.$;
    for each (let char in string)
      ref = ref[char] || (ref[char] = {__proto__: null});
    ref[""] = 1; // {"": 1} as terminator
    return this;
  },
  // ** {{{ RegexpTrie#addPrefixes(string) }}} **\\
  // Adds every prefix of {{{string}}} to the Trie and returns self. i.e.:
  // {{{
  // RegexpTrie().addPrefixes("ab") == RegexpTrie().add("a").add("ab")
  // }}}
  addPrefixes: function RegexpTrie_addPrefixes(string) {
    var ref = this.$;
    for each (let char in string)
      ref = ref[char] || (ref[char] = {"": 1, __proto__: null});
    return this;
  },
  // ** {{{ RegexpTrie#toString() }}} **\\
  // Returns a string representation of the Trie.
  toString: function RegexpTrie_toString() this._regexp(this.$),
  // ** {{{ RegexpTrie#toRegExp(flag) }}} **\\
  // Returns a regexp representation of the Trie with {{{flag}}}.
  toRegExp: function RegexpTrie_toRegExp(flag) RegExp(this, flag),
  _regexp: function RegexpTrie__regexp($) {
    LEAF_CHECK: if ("" in $) {
      for (let k in $) if (k) break LEAF_CHECK;
      return "";
    }
    var {quote} = regexp, alt = [], cc = [], q;
    for (let char in $) {
      if ($[char] !== 1) {
        let recurse = RegexpTrie__regexp($[char]);
        (recurse ? alt : cc).push(quote(char) + recurse);
      }
      else q = 1;
    }
    var cconly = !alt.length;
    if (cc.length) alt.push(1 in cc ?  "[" + cc.join("") + "]" : cc[0]);
    var result = 1 in alt ? "(?:" + alt.join("|") + ")" : alt[0];
    if (q) result = cconly ? result + "?" : "(?:" + result + ")?";
    return result || "";
  },
});

// == {{{ Utils.gist }}} ==
// [[http://gist.github.com/|Gist]] related functions.

Utils.gist = {
  // === {{{ Utils.gist.paste(files, id) }}} ===
  // Pastes code to Gist.
  //
  // {{{files}}} is the dictionary of name:code pairs.
  //
  // {{{id}}} is an optional number that specifies target Gist.
  // The user needs to be the owner of that Gist.
  paste: function gist_paste(files, id) {
    var data = id ? ["_method=put"] : [], i = 1;
    for (let name in files) {
      for (let [k, v] in new Iterator({
        name: name, contents: files[name] || "", ext: ""}))
        data.push("file_" + k + "[gistfile" + i + "]=" +
                  encodeURIComponent(v));
      ++i;
    }
    Utils.openUrlInBrowser("http://gist.github.com/gists/" + (id || ""),
                           data.join("&"));
  },

  // === {{{ Utils.gist.getName(document) }}} ===
  // Extracts the name of a Gist via its DOM {{{document}}}.
  getName: function gist_getName(document) {
    try { var {hostname, pathname, search} = document.location } catch (e) {}
    if (hostname !== "gist.github.com") return "";

    if (search.length > 1)
      try { return decodeURIComponent(search).slice(1) } catch (e) {}

    var name = "gist:" + /\w+/.exec(pathname), sep = " \u2013 ";

    var desc = document.getElementById("gist-text-description");
    if (desc && /\S/.test(desc.textContent))
      return name + sep + desc.textContent.trim();

    var info = document.querySelector(".file .info");
    if (info) return name + sep + info.textContent.trim().slice(0, -2);

    return name;
  },

  // === {{{ Utils.gist.fixRawUrl(url) }}} ===
  // Maps <http://gist.github.com/X.txt> to <https://raw.github.com/gist/X>.
  fixRawUrl: function gist_fixRawUrl(url) {
    var match = /^https?:\/\/gist\.github\.com(\/\d+)\.txt\b/.exec(url);
    return match ? "https://raw.github.com/gist" + match[1] : url;
  },
};

    $.u.Utils = Utils;
    $.u.Utils.defineLazyProperty = defineLazyProperty;
    Utils.log = log;
    Utils.reportError = reportError;
    Utils.reportWarning = reportWarning;
    Utils.reportInfo = reportInfo;
    Utils.ellipsify = ellipsify;
    Utils.absolutifyUrlAttribute = absolutifyUrlAttribute;
    Utils.isTextBox = isTextBox;
    Utils.setTimeout = setTimeout;
    Utils.clearTimeout = clearTimeout;
    Utils.__TimerCallback = __TimerCallback;
    Utils.uri = uri;
    Utils.openUrlInBrowser = openUrlInBrowser;
    Utils.focusUrlInBrowser = focusUrlInBrowser;
    Utils.getCookie = getCookie;
    Utils.paramsToString = paramsToString;
    Utils.urlToParams = urlToParams;
    Utils.getLocalUrl = getLocalUrl;
    Utils.parseHtml = parseHtml;
    Utils.sort = sort;
    Utils.uniq = uniq;
    Utils.isEmpty = isEmpty;
    Utils.classOf = classOf;
    Utils.count = count;
    Utils.keys = keys;
    Utils.powerSet = powerSet;
    Utils.Sequence = Sequence;
    Utils.computeCryptoHash = computeCryptoHash;
    Utils.signHmac = signHmac;
    Utils.escapeHtml = escapeHtml;
    Utils.unescapeHtml = unescapeHtml;
    Utils.convertFromUnicode = convertFromUnicode;
    Utils.convertToUnicode = convertToUnicode;
    Utils.notify = notify;
    Utils.listenOnce = listenOnce;
    Utils.defineLazyProperty = defineLazyProperty;
    Utils.extend = extend;
    Utils.BrowserTab = BrowserTab;
    Utils.regexp = regexp;

});
