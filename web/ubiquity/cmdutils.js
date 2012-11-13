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
 *   Aza Raskin <aza@mozilla.com>
 *   Jono DiCarlo <jdicarlo@mozilla.com>
 *   Maria Emerson <memerson@mozilla.com>
 *   Blair McBride <unfocused@gmail.com>
 *   Abimanyu Raja <abimanyuraja@gmail.com>
 *   Christian Sonne <cers@geeksbynature.dk>
 *   Satoshi Murakami <murky.satyr@gmail.com>
 *   Michael Yoshitaka Erlewine <mitcho@mitcho.com>
 *   Louis-Remi Babe <lrbabe@gmail.com>
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

// = CmdUtils =
// A library of general utility functions for use by command code.
// Everything clients need is contained within the {{{CmdUtils}}} namespace.

jQuery(function ($) {

var EXPORTED_SYMBOLS = ["CmdUtils"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

//Cu.import("/ubiquity/modules/utils.js");
//Cu.import("/ubiquity/modules/setup.js");
//Cu.import("/ubiquity/modules/nounutils.js");
//Cu.import("/ubiquity/modules/contextutils.js");
//Cu.import("/ubiquity/modules/localization_utils.js");
//Cu.import("/ubiquity/modules/cmdmanager.js");

var L = $.u.LocalizationUtils.propertySelector(
  "/ubiquity/chrome/locale/coreubiquity.properties");

    var Utils = $.u.Utils;

var {commandSource, messageService} = $.u.UbiquitySetup.createServices();

var CmdUtils = {
  toString: function toString() "[object CmdUtils]",
  __globalObject: null,
  __nextId: null,

  // === {{{ CmdUtils.parserVersion }}} ===
  // This attribute contains the parser version that Ubiquity is
  // using. A command can provide different options to
  // {{{CmdUtils.CreateCommand()}}} and behave differently
  // depending on this value, allowing a single command feed to
  // cater to whatever parser the user is using.
  //
  // Ubiquity 0.1.x only supports parser version 1, while
  // Ubiquity 0.5.x supports parser versions 1 and 2.
  get parserVersion() Utils.prefs.get("extensions.ubiquity.parserVersion", 1),

  // === {{{ CmdUtils.maxSuggestions }}} ===
  // The current number of max suggestions.
  get maxSuggestions() $.u.CommandManager.maxSuggestions,
};

for each (let f in this) if (typeof f === "function") CmdUtils[f.name] = f;
for each (let g in ["document", "documentInsecure",
                    "window", "windowInsecure", "hiddenWindow",
                    "geoLocation"]) {
    CmdUtils.__defineGetter__(g, eval("get" + g[0].toUpperCase() + g.slice(1)));
}
delete CmdUtils.QueryInterface;

function doAt(code, keys) {
  for each (let key in keys) doAt.it += (";" + code).replace(/@/g, key);
};

// == From NounUtils ==
// {{{CmdUtils}}} inherits {{{NounUtils}}}.

for (let k in $.u.NounUtils) CmdUtils[k] = $.u.NounUtils[k];

// == From ContextUtils ==
// {{{CmdUtils}}} inherits {{{ContextUtils}}}.
// The methods are wrapped so that the {{{context}}} argument isn't needed.
// (i.e. {{{CmdUtils.getSelection("")}}} is equivalent to
// {{{ContextUtils.getSelection(context, "")}}})

doAt(<![CDATA[{
  CmdUtils["__define" + "@"[0].toUpperCase() + "etter__"](
    "@"[3].toLowerCase() + "@".slice(4),
    CmdUtils.@ = function @(x, y) {
      var c = this.__globalObject.context || {};
      "focusedWindow" in c && "focusedElement" in c ||
        (c = Utils.currentChromeWindow.document.commandDispatcher);
      return ContextUtils.@(c, x, y);
    });
}]]>, (name for (name in $.u.ContextUtils)));

// === {{{ CmdUtils.log(a, b, c, ...) }}} ===
// See {{{Utils}}}{{{.log}}}.

CmdUtils.log = Utils.log;

// === {{{ CmdUtils.getWindow() }}} ===
// === {{{ CmdUtils.getDocument() }}} ===
// Gets the window/document of the current tab in a secure way.

function getWindow() Utils.currentChromeWindow.content;
function getDocument() getWindow().document;

// === {{{ CmdUtils.getWindowInsecure() }}} ===
// === {{{ CmdUtils.getDocumentInsecure() }}} ===
// Gets the window/document object of the current tab, without the
// safe {{{XPCNativeWrapper}}}.
// While this allows access to scripts in the content,
// it is potentially **unsafe** and {{{getWindow()/getDocument()}}} should
// be used in place of this whenever possible.

function getWindowInsecure() getWindow().wrappedJSObject;
function getDocumentInsecure() getDocument().wrappedJSObject;

// === {{{ CmdUtils.getHiddenWindow() }}} ===

function getHiddenWindow() Utils.hiddenWindow;

// === {{{ CmdUtils.getCommand(id) }}} ===
// Gets a reference to a Ubiquity command by its ID or reference name
// (the first name in English).
// ID should be used whenever possible,
// as reference names aren't cannonical across feeds.
//
// {{{id}}} is the id or name of the command.

function getCommand(id) commandSource.getCommand(id);

// === {{{ CmdUtils.executeCommand(command, args) }}} ===
// === {{{ CmdUtils.previewCommand(command, pblock, args) }}} ===
// Executes/Previews an existing Ubiquity command.
//
// {{{command}}} is either the id or name of the Ubiquity command that will be
// executed or a direct reference to the command.
//
// {{{pblock}}} is the preview block.
//
// {{{args}}} is an object containing the modifiers values that will
// be passed to the execute function of the target command. e.g.:
// {{{
// {source: CmdUtils.makeSugg("English", null, "en"), goal: ...}
// }}}

// TODO: If the command doesn't exist, should we notify and/or fail gracefully?
doAt(<![CDATA[{
  CmdUtils.@Command = function @Command(command, x, y, z) {
    if (typeof command === "string") command = getCommand(command);
    return command.@(this.__globalObject.context, x, y, z);
  };
}]]>, ["execute", "preview"]);

// === {{{ CmdUtils.geocodeAddress(location, callback) }}} ===
// This function uses the Yahoo geocoding service to take a text
// string of an address/location and turn it into a structured
// geo-location.
//
// Returns an array of possible matches, where each match is
// an object that includes {{{latitude}}} , {{{longitude}}},
// {{{address}}}, {{{city}}}, {{{state}}}, {{{zip}}}, and {{{country}}}.
//
// {{{location}}} is a plaintext string of the address or location
// to be geocoded.
//
// {{{callback}}} is a function which gets passed the return of
// the geocoding.

function geocodeAddress(location, callback) this.__globalObject.jQuery.ajax({
  url: "http://local.yahooapis.com/MapsService/V1/geocode",
  data: {
    appid: "YD-9G7bey8_JXxQP6rxl.fBFGgCdNjoDMACQA--",
    location: location,
  },
  dataType: "xml",
  success: function gA_success(xml) {
    callback(Array.map(
      xml.getElementsByTagName("Result"),
      function gA_eachResult(result) {
        var dict = {};
        Array.forEach(
          result.getElementsByTagName("*"),
          function gA_eachItem(item) {
            dict[item.nodeName.toLowerCase()] = item.textContent;
          });
        dict.lat = dict.latitude;
        dict.lon = dict.longitude;
        return dict;
      }));
  },
});

const RE_URL_INJECTABLE = /^(?:https?|file|data|chrome|resource):/;

// === {{{ CmdUtils.injectCss(css, [document]) }}} ===
// Injects CSS into the current tab's document or {{{document}}}.
// Returns the injected element for later use.
//
// {{{css}}} is a string that can be either
// a CSS source code or a URL to a CSS file.

function injectCss(css, document) {
  var doc = document || getDocument(), lmn;
  if (RE_URL_INJECTABLE.test(css)) {
    lmn = doc.createElement("link");
    lmn.rel = "stylesheet";
    lmn.href = css;
  }
  else {
    lmn = doc.createElement("style");
    lmn.innerHTML = css;
  }
  return doc.body.appendChild(lmn);
}

// === {{{ CmdUtils.injectHtml(html, [document]) }}} ===
// Injects HTML source code at the end of the current tab's document or
// {{{document}}}. Returns the injected nodes as a jQuery object.
//
// {{{html}}} is the HTML source code to inject, in plain text.

function injectHtml(html, document) {
  const {jQuery} = this.__globalObject;
  var doc = document || getDocument();
  return jQuery("<div>" + html + "</div>").contents().appendTo(doc.body);
}

// === {{{ CmdUtils.injectJavascript(src, [callback], [document]) }}} ===
// Injects JavaScript into the current tab's document or {{{document}}},
// and calls an optional callback function once the script has loaded.
//
// Note that this is **not** intended to be used as a
// way of importing JavaScript into the command's sandbox.
//
// {{{src}}} is the source URL/code of the JavaScript to inject.
//
// {{{callback}}} is an optional callback function to be called once the script
// has loaded in the document. The 1st argument will be the global object
// of the document (i.e. window).

function injectJavascript(src, callback, document) {
  var doc = document || getDocument();
  var script = doc.createElement("script");
  script.type = "application/javascript;version=1.8";
  script.src = RE_URL_INJECTABLE.test(src)
               ? src
               : "data:;charset=utf-8," + encodeURIComponent(src);
  script.addEventListener("load", function onInjected() {
    doc.body.removeChild(script);
    if (typeof callback === "function")
      callback(doc.defaultView, doc);
  }, false);
  doc.body.appendChild(script);
}

// === {{{ CmdUtils.loadJQuery(callback, [document]) }}} ===
// Injects the jQuery javascript library into the current tab's document or
// {{{document}}}.
//
// {{{callback}}} gets passed back the {{{jQuery}}} object once it is loaded.

function loadJQuery(callback, document) {
  injectJavascript(
    "/ubiquity/scripts/jquery.js",
    callback && this.safeWrapper(function onJQuery(win) {
      callback(win.wrappedJSObject.jQuery);
    }),
    document);
}

// === {{{ CmdUtils.copyToClipboard(text) }}} ===
// This function places the passed-in text into the OS's clipboard.
// If the text is empty, the copy isn't performed.
//
// {{{text}}} is a plaintext string that will be put into the clipboard.

function copyToClipboard(text) (
  (text = String(text)) && (Utils.clipboard.text = text));

// === {{{ CmdUtils.onPageLoad(callback, includes = "*", excludes = []) }}} ===
// Sets up a function to be run whenever a page is loaded.
//
// {{{callback}}} is the callback function called each time a new page is
// loaded; it is passed a single argument, which is the page's document object.
//
// {{{includes}}}/{{{excludes}}} are optional regexps/strings
// (or arrays of them) that select pages to include/exclude by their URLs
// a la [http://wiki.greasespot.net/Include_and_exclude_rules|GreaseMonkey].
// * if string, it matches the whole URL with asterisks as wildcards.

function onPageLoad(callback, includes, excludes) {
  var {pageLoadFuncs} = this.__globalObject;
  if (!includes && !excludes) return pageLoadFuncs.push(callback);

  function enwild(a) Utils.regexp.quote(a).replace(/\\[*]/g, ".*?");
  function toRegExps(filters) [
    Utils.classOf(f) === "RegExp" ? f : RegExp("^" + enwild(f) + "$")
    for each (f in [].concat(filters || []))];
  [includes, excludes] = [includes || /^/, excludes].map(toRegExps);
  return pageLoadFuncs.push(function pageLoadProxy(document) {
    var {href} = document.location;
    for each (let r in excludes) if (r.test(href)) return;
    for each (let r in includes) if (r.test(href)) return callback(document);
  });
}

// === {{{ CmdUtils.onUbiquityLoad(callback) }}} ===
// Sets up a function to be run whenever a Ubiqutiy instance is created.
//
// {{{callback}}} is the callback function called each time a new
// [[#chrome/content/ubiquity.js|Ubiquity]] instance is created;
// it is passed two arguments, which are the Ubiquity instance and
// the chrome window associated with it.

function onUbiquityLoad(callback) {
  return this.__globalObject.ubiquityLoadFuncs.push(callback);
}

// ** {{{ CmdUtils.setLastResult(result) }}} **
// **//Deprecated. Do not use.//**
//
// Sets the last result of a command's execution, for use in command piping.

function setLastResult(result) {
  // TODO: This function was used for command piping, which has been
  // removed for the time being; we should probably consider removing this
  // function and anything that uses it.
  //globals.lastCmdResult = result;
}

// === {{{ CmdUtils.getGeoLocation([callback]) }}} ===
// Uses Geo-IP lookup to get the user's physical location.
// Will cache the result.
// If a result is already in the cache, this function works both
// asyncronously and synchronously (for backwards compatability).
// Otherwise it works only asynchronously.
//
// {{{callback}}} Optional callback function.  Will be called back
// with a geolocation object. If specified, {{{getGeoLocation}}} returns
// the requesting {{{XMLHttpRequest}}} instead.
//
// The geolocation object has at least the following properties:
//
// {{{ lat  lon  city  state  country  }}}
//
// Plus their short versions as {{{*Short}}} if available.
//
// * {{{countryShort}}} is aliased as {{{country_code}}} for backcompat.
// * See [[http://code.google.com/apis/maps/documentation/geocoding/#Types]]
//  for a list of possible properties.
//
// You can choose to use the function synchronously: do not pass in any
// callback, and the geolocation object will instead be returned
// directly.

function getGeoLocation(callback) {
    return undefined;

  if (!callback)
    return getGeoLocation.cache || getGeoLocation(function fetch() {});

  const GL = Cc["@mozilla.org/geolocation;1"]
             .getService(Ci.nsIDOMGeoGeolocation);
  try {
    GL.getCurrentPosition(function got({coords}) {
      var loc = {lat: coords.latitude, lon: coords.longitude};
      var url = "http://maps.google.com/maps/api/geocode/json?sensor=false" +
                "&latlng=" + loc.lat + "," + loc.lon;
      var xhr = new Utils.currentChromeWindow.XMLHttpRequest;
      xhr.mozBackgroundRequest = true;
      xhr.open("GET", url, true);
      xhr.onload = function loaded() {
        do {
          if (this.status != 200) break;
          let dat = JSON.parse(this.responseText);
          if (dat.status != "OK") break;
          for each (let result in dat.results)
            for each (let address in result.address_components)
              for each (let type in address.types) if (!loc[type])
                (loc[type] = address.long_name) === address.short_name ||
                (loc[type + 'Short'] = address.short_name);
          loc.country_code = loc.countryShort || loc.politicalShort;
          loc.country = loc.country || loc.political;
          loc.city    = loc.locality;
          loc.state   = loc.administrative_area_level_1;
        } while (0);
        callback(getGeoLocation.cache = loc);
      };
      xhr.send();
    }, null, {enableHighAccuracy: true});
  } catch (e) { Cu.reportError(e) }
}
getGeoLocation(); // prefetch

CmdUtils.UserCode = {
  //Copied with additions from /ubiquity/chrome/content/prefcommands.js
  COMMANDS_PREF : "extensions.ubiquity.commands",
  EDITOR_RE: RegExp(
    "^(?:" +
    ["/ubiquity/chrome/content/editor.xhtml",
     "about:ubiquity?editor"].map(Utils.regexp.quote).join("|") +
    ")$"),

  setCode: function UC_setCode(code) {
    Utils.prefs.setValue(this.COMMANDS_PREF, code);
    //Refresh any code editor tabs that might be open
    Utils.tabs.reload(this.EDITOR_RE);
  },

  getCode: function UC_getCode()
    Utils.prefs.getValue(this.COMMANDS_PREF, ""),

  appendCode: function UC_appendCode(code){
    this.setCode(this.getCode() + code);
  },

  prependCode: function UC_prependCode(code){
    this.setCode(code + this.getCode());
  }
};

// == SNAPSHOT ==

// === {{{ CmdUtils.getTabSnapshot(tab, [options]) }}} ===
// Creates a thumbnail image of the contents of a given tab.
//
// {{{tab}}} is a {{{BrowserTab}}} instance.
//
// See {{{getWindowSnapshot()}}} for {{{options}}}.

function getTabSnapshot(tab, options) (
  getWindowSnapshot(tab.document.defaultView, options));

// === {{{ CmdUtils.getWindowSnapshot(win, [options]) }}} ===
// Creates a thumbnail image of the contents of the given window.
//
// {{{window}}} is a {{{Window}}} object.
//
// {{{options}}} is an optional dictionary which can contain any or all
// of the following properties:
// *{{{width (= 200)}}}\\
// Height will be determined automatically to maintain the aspect ratio.
// *{{{type (= "jpeg")}}}\\
// *{{{quality (= 80)}}}\\
// *{{{background (= "rgb(255,255,255)")}}}\\

function getWindowSnapshot(win, options) {
  var opts = {
    width: 200,
    type: "jpeg",
    quality: 80,
    background: "rgb(255,255,255)",
  };
  for (let k in options) opts[k] = options[k];

  var {width} = opts, {innerWidth, innerHeight} = win;
  var canvas = getHiddenWindow().document.createElementNS(
    "http://www.w3.org/1999/xhtml", "canvas");
  canvas.mozOpaque = true;
  canvas.width = width;
  canvas.height = width * innerHeight / innerWidth;

  var widthScale =  width / innerWidth;
  var ctx = canvas.getContext("2d");
  ctx.scale(widthScale, widthScale);
  ctx.drawWindow(win, win.scrollX, win.scrollY, innerWidth, innerWidth,
                 opts.background);

  return canvas.toDataURL(
    "image/" + opts.type,
    opts.type === "jpeg" ? "quality=" + opts.quality : "");
}

// === {{{ CmdUtils.getImageSnapshot(url, callback) }}} ===
// Takes a snapshot of an image residing at the passed-in URL. This
// is useful for when you want to get the bits of an image when it
// is hosted elsewhere. The bits can then be manipulated at will
// without worry of same-domain restrictions.
//
// {{{url}}} is where the image is located.
//
// {{{callback}}} gets passed back the bits of the
// image, in the form of {{{data:image/png;base64,}}}.

function getImageSnapshot(url, callback) {
  var {document, Image} = getHiddenWindow();
  var canvas = document.createElementNS("http://www.w3.org/1999/xhtml",
                                        "canvas");
  var img = Image();
  img.src = url;
  img.addEventListener("load", function gIS_load(){
    canvas.width = img.width;
    canvas.height = img.height;
    var ctx = canvas.getContext("2d");
    ctx.drawImage(img, 0, 0);
    callback(canvas.toDataURL());
  }, true);
}

// == PASSWORDS AND OTHER SENSITIVE INFORMATION ==

// === {{{ CmdUtils.savePassword(opts) }}} ===
// Saves a pair of username/password (or username/api key) to the password
// manager.
//
// {{{opts}}} is a dictionary object which must have the following properties:
// *{{{name}}} : a unique string used to identify this username/password
// pair (for instance, you can use the name of your command)
// *{{{username}}} : the username to store
// *{{{password}}} : the password (or other private data, such as an API key)
// corresponding to the username

function savePassword(name, username, password) {
  if (typeof name != "string") var {name, username, password} = name;
  const {LoginManager} = Utils, Host = "chrome://ubiquity";
  var loginInfo = (Cc["@mozilla.org/login-manager/loginInfo;1"]
                   .createInstance(Ci.nsILoginInfo));
  loginInfo.init(Host, null, name, username, password, "", "");
  for each (let login in LoginManager.findLogins({}, Host, null, name)) {
    if (login.username != username) continue;
    LoginManager.modifyLogin(login, loginInfo);
    return;
  }
  LoginManager.addLogin(loginInfo);
}

// === {{{ CmdUtils.loadPassword(name, username) }}} ===
// Returns the saved password for the {{{name}}}space and {{{username}}}
// or {{{null}}} if not found.

function loadPassword(name, username) {
  if (typeof name != "string") var {name, username} = name;
  for each (let login in retrieveLogins(name))
    if (login.username == username) return login.password;
  return null;
}

// === {{{ CmdUtils.retrieveLogins(name) }}} ===
// Retrieves one or more username/password saved with
// {{{CmdUtils.savePassword()}}}
// as an array of objects, each of which takes the form
// {{{{username: "", password: ""}}}}.
//
// {{{name}}} is the identifier of the username/password pair to retrieve.
// This must match the {{{opts.name}}} that was passed in to
// {{{savePassword()}}} when the password was stored.

function retrieveLogins(name) {
  const {LoginManager} = Utils;
  var logins = LoginManager.findLogins({}, "chrome://ubiquity", null, name);
  // backward compatibility
  logins.push.apply(logins, LoginManager.findLogins(
    {}, "/ubiquity/chrome/content", "UbiquityInformation" + name, null));
  return Utils.uniq(logins, "username");
}

// == COMMAND CREATION ==

// === {{{ CmdUtils.CreateCommand(options) }}} ===
// Creates and registers a Ubiquity command.
//
// {{{options}}} is a dictionary object which
// ** must have the following properties: **
// *{{{name}}}/{{{names}}}\\
// The string or array of strings which will be the name or
// names of your command the user will type into the command line,
// or choose from the context menu, to activate it.
// *{{{execute}}}\\
// The function which gets run when the user executes your command,
// or the string which is notified or opened (if URL).
// If your command takes arguments (see below),
// your execute method will be passed an dictionary object containing
// the arguments assigned by the parser.
//
// ** The following properties are used if you want your command to
// accept arguments: **
// *{{{arguments}}}\\
// Defines the primary arguments of the command.
// See [[http://bit.ly/Ubiquity05_AuthorTutorial#Commands_with_Arguments]].
//
// ** The following properties are optional but strongly recommended to
// make your command easier for users to learn: **
//
// *{{{description}}}\\
// An XHTML string containing a short description of your command, to be displayed
// on the command-list page.
// *{{{help}}}\\
// An XHTML string containing a longer description of
// your command, also displayed on the command-list page, which can go
// into more depth, include examples of usage, etc.
//
// ** The following properties are optional: **
// *{{{icon}}}\\
// A URL string pointing to a small image (favicon-sized) to
// be displayed alongside the name of your command in the interface.
// *{{{author}}}/{{{authors}}}, {{{contributor}}}/{{{contributors}}}\\
// A plain text or dictionary object (which can have {{{name}}}, {{{email}}},
// and {{{homepage}}} properties, all plain text)
// describing the command's author/contributor.
// Can be an array of them if multiple.
// *{{{homepage}}}\\
// A URL string of the command's homepage, if any.
// *{{{license}}}\\
// A string naming the license under which your
// command is distributed, for example {{{"MPL"}}}.
// *{{{preview}}}\\
// A description of what your command will do,
// to be displayed to the user before the command is executed.  Can be
// either a string or a function.  If a string, it will simply be
// displayed as-is. If preview is a function, it will be called and
// passed a {{{pblock}}} argument, which is a reference to the
// preview display element.  Your function can generate and display
// arbitrary HTML by setting the value of {{{pblock.innerHTML}}}.
// Use {{{this.previewDefault(pblock)}}} to set the default preview.
// If your command takes arguments (see above), your preview method will
// be passed the dictionary as the second argument.
// *{{{previewDelay}}}\\
// Specifies the amount in time, in
// milliseconds, to wait before calling the preview function defined
// in {{{options.preview}}}. If the user presses a key before this
// amount of time has passed, then the preview function isn't
// called. This option is useful, for instance, if displaying the
// preview involves a round-trip to a server and you only want to
// display it once the user has stopped typing for a bit. If
// {{{options.preview}}} isn't a function, then this option is
// ignored.
// *{{{previewUrl}}}\\
// Specifies the URL which the preview
// pane's browser should load before calling the command's preview
// function. When the command's preview function is called, its
// {{{pblock}}} argument will be the {{{<body>}}} node of this URL's
// document. This can also be a relative URL, in which case it will be
// based off the URL from which the feed is being retrieved.

function CreateCommand(options) {
  var global = this.__globalObject;
  var command = {
    __proto__: options,
    proto: options,
    global: global,
    previewDefault: CreateCommand.previewDefault,
  };

  var me = this;
  function toNounType(obj, key) {
    var val = obj[key];
    if (!val) return;
    var noun = obj[key] = me.NounType(val);
    if (!noun.id) noun.id = global.feed.id + "#n" + me.__nextId++;
  }

  // ensure name and names
  { let names = options.names || options.name;
    if (!names)
      throw Error("CreateCommand: name or names is required");
    if (!Utils.isArray(names))
      names = String(names).split(/\s{0,}\|\s{0,}/);
    if (Utils.isArray(options.synonyms)) // for old API
      names.push.apply(names, options.synonyms);

    // We must keep the first name from the original feed around as an
    // identifier. This is used in the command id and in localizations
    command.referenceName = command.name = names[0];
    command.names = names;
  }
  OLD_DEPRECATED_ARGUMENT_API:
  { let {takes, modifiers} = options;
    command.oldAPI = !!(takes || modifiers);
    for (let label in takes) {
      command.DOLabel = label;
      command.DOType = takes[label];
      toNounType(command, "DOType");
      break;
    }
    for (let label in modifiers) toNounType(modifiers, label);
  }
  NEW_IMPROVED_ARGUMENT_API:
  { let args = options.arguments || options.argument;
    if (!args) {
      command.arguments = [];
      break NEW_IMPROVED_ARGUMENT_API;
    }
    // handle simplified syntax
    if (typeof args.suggest === "function")
      // argument: noun
      args = [{role: "object", nountype: args}];
    else if (!Utils.isArray(args)) {
      // arguments: {role: noun, ...}
      // arguments: {"role label": noun, ...}
      let a = [], re = /^[a-z]+(?=(?:[$_:\s]([^]+))?)/;
      for (let key in args) {
        let [role, label] = re.exec(key) || 0;
        if (role) a.push({role: role, label: label, nountype: args[key]});
      }
      args = a;
    }
    for each (let arg in args) toNounType(arg, "nountype");
    command.arguments = args;
  }
  { let {execute, preview} = options;
    if (typeof execute !== "function")
      command.execute = CreateCommand.executeDefault;
    if (typeof preview !== "function") {
      if (preview != null) command.previewHtml = String(preview);
      command.preview = CreateCommand.previewDefault;
    }
  }

  if ("previewUrl" in options && !options.__lookupGetter__("previewUrl"))
    // Call our "patched" Utils.uri(), which has the ability
    // to base a relative URL on the current feed's URL.
    command.previewUrl = global.Utils.uri(options.previewUrl);

  global.commands.push(command);
  return command;
}
CreateCommand.executeDefault = function executeDefault() {
  var {proto: {execute}, global} = this, uri;
  try { uri = global.Utils.uri(execute) } catch ([]) {}
  if (uri) return Utils.focusUrlInBrowser(uri.spec);
  if (execute == null) execute = L("ubiquity.cmdutils.noactiondef");
  global.displayMessage(execute, this);
};
CreateCommand.previewDefault = function previewDefault(pb) {
  var html = "";
  if ("previewHtml" in this) html = this.previewHtml;
  else {
    if ("description" in this)
      html += '<div class="description">' + this.description + '</div>';
    if ("help" in this)
      html += '<p class="help">' + this.help + '</p>';
    if (!html) html = L(
      "ubiquity.cmdutils.previewdefault",
      '<strong class="name">' + Utils.escapeHtml(this.name) + "</strong>");
    html = '<div class="default">' + html + '</div>';
  }
  return (pb || 0).innerHTML = html;
};

// == COMMAND ALIAS CREATION ==

// === {{{ CmdUtils.CreateAlias(options) }}} ===
// Creates and registers an alias to another (target) Ubiquity command. Aliases
// can be simple synonyms, but they can also specify certain pre-defined
// argument values to be used in the parse/preview/execution.
//
// {{{options}}} is a dictionary object with following properties.
// *{{{name}}}/{{{names}}}\\
// The name string or array of strings for the alias.
// Don't use the same name as the verb itself.
// *{{{verb}}}\\
// The canonical name of the verb which is being aliased.
// Note, the "canonical name" is the first element of the verb's {{{names}}}.
// *{{{givenArgs}}} (optional)\\
// Specifies pre-determined arguments for the target
// verb. This is a hash keyed by semantic roles. The values are the text input
// value for that argument. The parser will then run that value through the
// nountype associated with that semantic role for the target verb and use that
// argument in parse/preview/execution.
//
// See {{{CmdUtils.CreateCommand()}}} for other properties available.

function CreateAlias(options) {
  var {verb} = options, CU = this, cmd = CU.CreateCommand(options);
  Utils.defineLazyProperty(cmd, function alias_lazyArgs() {
    var target = getCommand(verb) || verb;
    if (!target) return [];
    var args = target.arguments;
    var {givenArgs} = cmd;
    if (givenArgs) {
      let as = [];
      for each (let arg in args) {
        let a = {};
        for (let k in arg) a[k] = arg[k];
        if (a.role in givenArgs) {
          a.input = givenArgs[a.role];
          a.hidden = true;
        }
        as.push(a);
      }
      args = as;
    }
    return args;
  }, "arguments");
  cmd.execute = function alias_execute(args, mods)
    CU.executeCommand(verb, args, mods);
  cmd.preview = function alias_preview(pb, args, mods)
    CU.previewCommand(verb, pb, args, mods);
  return cmd;
}

// === {{{ CmdUtils.makeSearchCommand(options) }}} ===
// A specialized version of {{{CmdUtils.CreateCommand()}}}. This lets
// you make commands that interface with search engines, without
// having to write so much boilerplate code.
// Also see https://wiki.mozilla.org/Labs/Ubiquity/Writing_A_Search_Command .
//
// {{{options}}} is same as the argument of {{{CmdUtils.CreateCommand()}}},
// except that instead of {{{options.arguments}}}, {{{options.execute}}},
// and {{{options.preview}}}, you only need a single property:
// *{{{url}}}\\
//  The URL of a search results page from the search engine of your choice.
//  Must contain the literal string {{{{QUERY}}}} or {{{%s}}}, which will be
//  replaced with the user's search term to generate a URL that should point to
//  the correct page of search results. (We're assuming that the user's search
//  term appears in the URL of the search results page, which is true for most
//  search engines.) For example: {{{http://www.google.com/search?q={QUERY}}}}
//
// If not specified, {{{options.name}}}, {{{options.icon}}},
// {{{options.description}}}, {{{options.execute}}} will be auto generated.
//
// Other optional parameters of {{{options}}} are:
// *{{{postData}}}\\
//  Makes the command use POST instead of GET, and the data
//  (key:value pairs or string) are all passed to the {{{options.url}}}.
//  Instead of including the search params in the URL, pass it
//  (along with any other params) like so:
//  {{{ {"q": "{QUERY}", "hl": "en"} }}} or {{{ "q={QUERY}&hl=en" }}}.
//  When this is done, the query will be substituted in as usual.
// *{{{defaultUrl}}}\\
//  A URL string that will be opened in the case
//  where the user has not provided a search string.
// *{{{charset}}}\\
//  A string specifying the character set of query.
//
// *{{{parser}}}\\
//  Generates keyboard navigatable previews by parsing the search results.
//  It is passed as an object containing following properties.
//  The ones marked as //path// expect either a jQuery selector string,
//  a JSON path string (like {{{"granma.mom.me"}}}). Each of them can also be
//  a filter function that receives a parent context and returns a result
//  of the same type.
// *{{{parser.type}}}\\
//  A string that's passed to {{{jQuery.ajax()}}}'s {{{dataType}}} parameter
//  when requesting. If {{{"json"}}}, the parser expects JSON paths.
// *{{{parser.title}}}\\
//  //Required//. The //path// to the title of each result.
// *{{{parser.container}}}\\
//  //Recommended//. A //path// to each container that groups each of
//  title/body/href/thumbnail result sets.
// *{{{parser.body}}}\\
//  A //path// to the content of each result.
// *{{{parser.href}}} / {{{parser.thumbnail}}}\\
//  //Path//s to the link/thumbnail URL of each result.
//  Should point to an {{{<a>}}}/{{{<img>}}} if jQuery mode.
// *{{{parser.url}}} / {{{parser.postData}}}\\
//  Specifies another versions of {{{options.url}}}/{{{options.postData}}},
//  in the case when a different request set is used for preview.
// *{{{parser.baseUrl}}}\\
//  A URL string that will be the base for relative links, such that they will
//  still work out of context. If not passed, it will be auto-generated from
//  {{{options.url}}} (and thus //may// be incorrect).
// *{{{parser.maxResults}}}\\
//  An integer specifying the max number of results. Defaults to 4.
// *{{{parser.plain}}}\\
//  An array of strings naming //path//s that should be treated as plain text
//  (and thus be HTML-escaped).
// *{{{parser.log}}}\\
//  A function to which the response data and parsed results are logged.
//  If non-function, {{{makeSearchCommand.log()}}} is used.

function makeSearchCommand(options) {
  if (!("url" in options)) options.url = options.parser.url;
  var [baseUrl, domain] = /^\w+:\/\/([^?#/]+)/.exec(options.url) || [""];
  var [name] = [].concat(options.names || options.name);
  if (!name) name = options.name = domain;
  var htmlName = Utils.escapeHtml(name);
  if (!("icon" in options)) options.icon = baseUrl + "/favicon.ico";
  if (!("description" in options))
    // "Searches %s for your words."
    options.description = L(
      "ubiquity.cmdutils.searchdescription",
      "defaultUrl" in options ? htmlName.link(options.defaultUrl) : htmlName);
  if (!("arguments" in options || "argument" in options))
    options.argument = this.__globalObject.noun_arb_text;
  if (!("execute" in options)) options.execute = makeSearchCommand.execute;
  if (!("preview" in options)) {
    options.preview = makeSearchCommand.preview;
    if ("parser" in options) let ({parser} = options) {
      function fallback(n3w, old) {
        if (n3w in parser || !(old in parser)) return;
        Utils.reportWarning(
          "makeSearchCommand: parser." + old + " is deprecated. " +
          "Use parser." + n3w + " instead.", 2);
        parser[n3w] = parser[old];
      }
      fallback("body", "preview");
      fallback("baseUrl", "baseurl");
      if (!("baseUrl" in parser)) parser.baseUrl = baseUrl;
      if ("type" in parser) parser.type = parser.type.toLowerCase();
      parser.keys = [
        key for each (key in ["title", "body", "href", "thumbnail"])
        if (key in parser)];
      if ("log" in parser && typeof parser.log !== "function")
        parser.log = makeSearchCommand.log;
    }
  }
  return this.CreateCommand(options);
}
makeSearchCommand.log = function searchLog(it, type) {
  Utils.log("SearchCommand: " + type + " =", it);
};
makeSearchCommand.query = function searchQuery(target, query, charset) {
  var re = /%s|{QUERY}/g, fn = encodeURIComponent;
  if (charset) {
    query = Utils.convertFromUnicode(charset, query);
    fn = escape;
  }
  return (
    typeof target === "object"
    ? [fn(key) + "=" + fn(val)
       for ([key, val] in new Iterator(target))].join("&")
    : target && target.replace(re, fn(query)));
};
makeSearchCommand.execute = function searchExecute({object: {text}}) {
  if (!text && "defaultUrl" in this)
    Utils.openUrlInBrowser(this.defaultUrl);
  else
    Utils.openUrlInBrowser(
      makeSearchCommand.query(this.url, text, this.charset),
      makeSearchCommand.query(this.postData, text, this.charset));
};
makeSearchCommand.preview = function searchPreview(pblock, {object: {text}}) {
  if (!text) return void this.previewDefault(pblock);

  function put() {
    pblock.innerHTML =
      "<div class='search-command'>" + Array.join(arguments, "") + "</div>";
  }
  var {parser, global} = this, queryHtml =
    "<strong class='query'>" + Utils.escapeHtml(text) + "</strong>";
  put(L("ubiquity.cmdutils.searchcmd", Utils.escapeHtml(this.name), queryHtml),
      !parser ? "" :
      "<p class='loading'>" + L("ubiquity.cmdutils.loadingresults") + "</p>");
  if (!parser) return;

  var {type, keys} = parser;
  var params = {
    url: makeSearchCommand.query(parser.url || this.url, text, this.charset),
    dataType: parser.type || "text",
    success: searchParse,
    error: function searchError(xhr) {
      put("<em class='error'>", xhr.status, " ", xhr.statusText, "</em>");
    },
  };
  var pdata = parser.postData || this.postData;
  if (pdata) {
    params.type = "POST";
    params.data = makeSearchCommand.query(pdata, text, this.charset);
  }
  global.CmdUtils.previewAjax(pblock, params);
  function searchParse(data) {
    if (!data) {
      put("<em class='error'>" + L("ubiquity.cmdutils.searchparserror") + "</em>");
      return;
    }
    if (parser.log) parser.log(data, "data");
    switch (type) {
      case "json": return parseJson(data);
      case "xml" : return parseDocument(data);
      default: return Utils.parseHtml(data, parseDocument);
    }
  }
  function parseJson(data) {
    // TODO: Deal with key names that include dots.
    function dig(dat, key) {
      var path = parser[key];
      if (path.call) return path.call(dat, dat);
      for each (let p in path && path.split(".")) dat = dat[p] || 0;
      return dat;
    }
    var results = [];
    if ("container" in parser)
      for each (let dat in dig(data, "container")) {
        let res = {};
        for each (let key in keys) res[key] = dig(dat, key);
        results.push(res);
      }
    else {
      let vals = [dig(data, k) for each (k in keys)];
      results = [keys.reduce(function (r, k, i) (r[k] = vals[i][j], r), {})
                 for (j in vals[0])];
    }
    onParsed(results);
  };
  function parseDocument(doc) {
    var {$} = global, results = [], $doc = $(doc);
    function find($_, key) let (path = parser[key])
      !path ? $() : path.call ? path.call($_, $_) : $_.find(path);
    if ("container" in parser)
      find($doc, "container").each(function eachContainer() {
        var res = {}, $this = $(this);
        for each (let k in keys) res[k] = find($this, k);
        results.push(res);
      });
    else {
      let qs = [find($doc, k) for each (k in keys)];
      results = [keys.reduce(function (r, k, i) (r[k] = qs[i].eq(j), r), {})
                 for (j in Utils.seq(qs[0].length))];
    }
    function pluck() this.innerHTML || this.textContent;
    function toCont(key) {
      for each (let r in results) r[key] = r[key].map(pluck).get().join(" ");
    }
    function toAttr(key, lnm, anm) {
      for each (let res in results) {
        let $_ = res[key], atr = ($_.is(lnm) ? $_ : $_.find(lnm)).attr(anm);
        res[key] = atr && Utils.escapeHtml(atr);
      }
    }
    "thumbnail" in parser && toAttr("thumbnail", "img", "src");
    "body" in parser && toCont("body");
    if (!("href" in parser)) for each (let r in results) r.href = r.title;
    toAttr("href", "a", "href");
    toCont("title");
    onParsed(results);
  }
  function onParsed(results) {
    if (parser.log) parser.log(results, "results");
    for each (let k in parser.plain)
      for each (let r in results) r[k] = r[k] && Utils.escapeHtml(r[k]);
    var list = "", i = 0, max = parser.maxResults || 4;
    for each (let {title, href, body, thumbnail} in results) if (title) {
      if (href) {
        let key = i < 35 ? (i+1).toString(36) : "-";
        title = ("<kbd>" + key + "</kbd> <a href='" + href +
                 "' accesskey='" + key + "'>" + title + "</a>");
      }
      list += "<dt class='title'>" + title + "</dt>";
      if (thumbnail)
        list += "<dd class='thumbnail'><img src='" + thumbnail + "'/></dd>";
      if (body)
        list += "<dd class='body'>" + body + "</dd>";
      if (++i >= max) break;
    }

    put(list
        ? ("<span class='found'>" +
           L("ubiquity.cmdutils.parsedresultsfound", queryHtml) +
           "</span><dl class='list'>" + list + "</dl>")
        : ("<span class='empty'>" +
           L("ubiquity.cmdutils.parsedresultsnotfound", queryHtml) +
           "</span>"));
    global.CmdUtils.absUrl(pblock, parser.baseUrl);
  }
};

// === {{{ CmdUtils.makeBookmarkletCommand(options) }}} ===
// Creates and registers a Ubiquity command based on a bookmarklet.
// When the command is run, it will invoke the bookmarklet.
//
// {{{options}}} as the argument of CmdUtils.CreateCommand, except that
// you must provide a property called:
// *{{{url}}}\\
// The URL of the bookmarklet code. Must start with {{{javascript:}}}.
//
// {{{options.execute}}} and {{{options.preview}}} are generated for you
// from the URL, so all you need to provide is {{{options.url}}} and
// {{{options.name}}}.
//
// You can choose to provide other optional properties, which work the
// same way as they do for {{{CmdUtils.CreateCommand()}}}, except that
// since bookmarklets can't take arguments, there's no reason to provide
// {{{options.arguments}}}.

function makeBookmarkletCommand(options) {
  options.execute = makeBookmarkletCommand.execute;
  var cmd = this.CreateCommand(options);
  if (!("description" in options)) options.description = L(
    "ubiquity.cmdutils.bookmarkletexec",
    '<strong class="name">' + Utils.escapeHtml(cmd.name) + '</strong>');
  return cmd;
}
makeBookmarkletCommand.execute = function bookmarklet_execute() {
  getWindow().location = this.url;
};

// == TEMPLATING ==

// === {{{ CmdUtils.renderTemplate(template, data) }}} ===
// Renders a {{{template}}} by substituting values from a dictionary.
// The templating language used is trimpath, which is defined at
// [[http://code.google.com/p/trimpath/wiki/JavaScriptTemplates]].
//
// {{{template}}} can be either a string, in which case the string is used
// for the template, or else it can be {file: "filename"}, in which
// case the following happens:
// * If the feed is on the user's local filesystem, the file's path
//   is assumed to be relative and the file's contents are read and
//   used as the template.
// * Otherwise, the file's path is assumed to be a key into a global
//   object called {{{Attachments}}}, which is defined by the feed.
//   The value of this key is used as the template.
//
// The reason this is done is so that a command feed can be developed
// locally and then easily deployed to a remote server as a single
// human-readable file without requiring any manual code
// modifications; with this system, it becomes straightforward to
// construct a post-processing tool for feed deployment that
// automatically generates the Attachments object and appends it to
// the command feed's code.
//
// {{{data}}} is a dictionary of values to be substituted.

function renderTemplate(template, data) {
  const {feed, Template, Attachments} = this.__globalObject;
  if (template.file)
    template = (
      Utils.uri(feed.id).scheme === "file"
      ? Utils.getLocalUrl(Utils.uri({uri: template.file, base: feed.id}).spec)
      : Attachments[template.file]);

  return Template.parseTemplate(template).process(data);
}

// == PREVIEW ==

// === {{{ CmdUtils.previewAjax(pblock, options) }}} ===
// Does an asynchronous request to a remote web service.  It is used
// just like {{{jQuery.ajax()}}}, which is documented at
// http://docs.jquery.com/Ajax/jQuery.ajax.
// The difference is that {{{CmdUtils.previewAjax()}}} is designed to handle
// command previews, which can be canceled by the user between the
// time that it's requested and the time it displays.  If the preview
// is canceled, no callbacks in the options object will be called.

function previewAjax(pblock, options) {
  const {jQuery} = this.__globalObject;
  var xhr;
  function abort() { if (xhr) xhr.abort() }

  var newOptions = {__proto__: options};
  for (var key in options) if (typeof options[key] === "function")
    newOptions[key] = previewCallback(pblock, options[key], abort);

  // see scripts/jquery_setup.js
  var wrappedXhr = newOptions.xhr || jQuery.ajaxSettings.xhr;
  newOptions.xhr = function backgroundXhr() {
    var newXhr = wrappedXhr.apply(this, arguments);
    newXhr.mozBackgroundRequest = true;
    return newXhr;
  }

  return xhr = jQuery.ajax(newOptions);
}

// === {{{ CmdUtils.previewGet(pblock, url, data, callback, type) }}} ===
// === {{{ CmdUtils.previewPost(pblock, url, data, callback, type) }}} ===
// Does an asynchronous request to a remote web service.
// It is used just like {{{jQuery.get()}}}/{{{jQuery.post()}}},
// which is documented at [[http://docs.jquery.com/Ajax]].
// The difference is that {{{previewGet()}}}/{{{previewPost()}}} is designed to
// handle command previews, which can be cancelled by the user between the
// time that it's requested and the time it displays.  If the preview
// is cancelled, the given callback will not be called.

doAt(<![CDATA[{
  CmdUtils.preview@ = function preview@(pblock, url, data, callback, type) {
    if (typeof data === "function") {
      callback = data;
      data = null;
    }
    return this.previewAjax(pblock, {
      type: "@",
      url: url,
      data: data,
      success: callback,
      dataType: type});
  };
}]]>, ["Get", "Post"]);

// === {{{ CmdUtils.previewCallback(pblock, callback, [abortCallback]) }}} ===
// Creates a 'preview callback': a wrapper for a function which
// first checks to see if the current preview has been canceled,
// and if not, calls the real callback.
//
// {{{pblock}}} is the preview display element (the same one which is
// passed in as the first argument to the {{{preview()}}} method of every
// command.
//
// {{{callback}}} is the function to be called if the preview is not
// cancelled.
//
// {{{abortCallback}}} is an optional function that will be called instead
// if the preview is cancelled.

function previewCallback(pblock, callback, abortCallback) {
  var previewChanged = false;
  function onPreviewChange() {
    pblock.removeEventListener("preview-change", onPreviewChange, false);
    previewChanged = true;
    if (abortCallback) abortCallback();
  }
  pblock.addEventListener("preview-change", onPreviewChange, false);

  return function wrappedCallback() {
    if (previewChanged) return null;

    pblock.removeEventListener("preview-change", onPreviewChange, false);
    return callback.apply(this, arguments);
  };
}

// === {{{ CmdUtils.previewList(block, htmls, [callback], [css]) }}} ===
// Creates a simple clickable list in the preview block and
// returns the list element.
// * Activating {{{accesskey="0"}}} rotates the accesskeys
//   in case the list is longer than the number of available keys.
// * The buttons are disabled upon activation to prevent duplicate calls.
//   To re-enable them, make {{{callback}}} return {{{true}}}.
//
// {{{block}}} is the DOM element the list will be placed into.
//
// {{{htmls}}} is the array/dictionary of HTML string to be listed.
//
// {{{callback(id, ev)}}} is the function called
// when one of the list item becomes focused.
// *{{{id}}} : one of the keys of {{{htmls}}}
// *{{{ev}}} : the event object
//
// {{{css}}} is an optional CSS string inserted along with the list.

function previewList(block, htmls, callback, css) {
  var {escapeHtml} = Utils, list = "", num = 0, CU = this;
  for (let key in htmls) {
    let k = ++num < 36 ? num.toString(36) : "-";
    list += ('<li><label for="' + num + '"><input type="button" id="' + num +
             '" class="button" value="' + k + '" accesskey="' + k +
             '" key="' + escapeHtml(key) + '"/>' + htmls[key] +
             '</label></li>');
  }
  block.innerHTML = (
    '<ol id="preview-list">' +
    '<style>' + previewList.CSS + (css || "") + '</style>' +
    '<input type="button" class="button" id="keyshifter"' +
    ' value="0" accesskey="0"/>' + list + '</ol>');
  var ol = block.firstChild, start = 0;
  callback && ol.addEventListener("click", function onPreviewListClick(ev) {
    var {target} = ev;
    if (target.type !== "button") return;
    ev.preventDefault();
    if (target.id === "keyshifter") {
      if (num < 36) return;
      let buttons = Array.slice(this.getElementsByClassName("button"), 1);
      start = (start + 35) % buttons.length;
      buttons = buttons.splice(start).concat(buttons);
      for (let i = 0, b; b = buttons[i];)
        b.value = b.accessKey = ++i < 36 ? i.toString(36) : "-";
      return;
    }
    target.disabled = true;
    if (callback.call(this, target.getAttribute("key"), ev))
      Utils.setTimeout(function reenableButton() { target.disabled = false });
  }, false);
  return ol;
}
previewList.CSS = "" + <![CDATA[
  #preview-list {margin: 0; padding-left: 1.5em; list-style-type: none}
  #preview-list > li {position: relative; min-height: 3ex}
  #preview-list > li:hover {outline: 1px solid; -moz-outline-radius: 8px}
  #preview-list label {display: block; cursor: pointer}
  #preview-list .button {
    position: absolute; left: -1.5em; height: 3ex;
    padding: 0; border-width: 1px;
    font: bold 108% monospace; text-transform: uppercase}
  #keyshifter {position:absolute; top:-9999px}
  ]]>;

// === {{{ CmdUtils.absUrl(data, baseUrl) }}} ===
// Fixes relative URLs in {{{data}}} (e.g. as returned by Ajax calls).
// Useful for displaying fetched content in command previews.
//
// {{{data}}} is the data containing relative URLs, which can be
// an HTML string or a jQuery/DOM/XML object.
//
// {{{baseUrl}}} is the URL used for base
// (that is to say; the URL that the relative paths are relative to).

function absUrl(data, baseUrl) {
  var {uri} = Utils;
  switch (typeof data) {
    case "string": return data.replace(
      /<[^>]+>/g,
      function absUrl_onTags(tag)
      tag.replace(
        /\b(href|src|action)=(?![\"\']?[a-z]+:\/\/)([\"\']?)([^\s>\"\']+)\2/i,
        function absUrl_onPath(_, a, q, path)
        a + "=" + q + uri({uri: path, base: baseUrl}).spec + q));
    case "object": {
      let $data = this.__globalObject.jQuery(data);
      for each (let name in ["href", "src", "action"]) {
        let sl = "*[" + name + "]", fn = function absUrl_each() {
          var {spec} = uri({uri: this.getAttribute(name), base: baseUrl});
          this.setAttribute(name, spec);
        };
        $data.filter(sl).each(fn).end().find(sl).each(fn);
      }
      return data;
    }
    case "xml": return XML(absUrl.call(this, data.toXMLString(), baseUrl));
  }
  return null;
}

// === {{{ CmdUtils.safeWrapper(func) }}} ===
// Wraps a function so that exceptions from it are suppressed and notified.

function safeWrapper(func) function safeWrapped() {
  try { return func.apply(this, arguments) } catch (e) {
    messageService.displayMessage({
      text: "An exception occurred while running " + func.name + "().",
      exception: e});
    return e;
  }
};

eval(doAt.it);
delete doAt;

    $.u.CmdUtils = CmdUtils;
    CmdUtils.getWindow = getWindow;
    CmdUtils.getDocument = getDocument;
    CmdUtils.getWindowInsecure = getWindowInsecure;
    CmdUtils.getDocumentInsecure = getDocumentInsecure;
    CmdUtils.getHiddenWindow = getHiddenWindow;
    CmdUtils.getCommand = getCommand;
    CmdUtils.geocodeAddress = geocodeAddress;
    CmdUtils.injectCss = injectCss;
    CmdUtils.injectHtml = injectHtml;
    CmdUtils.injectJavascript = injectJavascript;
    CmdUtils.loadJQuery = loadJQuery;
    CmdUtils.copyToClipboard = copyToClipboard;
    CmdUtils.onPageLoad = onPageLoad;
    CmdUtils.onUbiquityLoad = onUbiquityLoad;
    CmdUtils.setLastResult = setLastResult;
    CmdUtils.getGeoLocation = getGeoLocation;
    CmdUtils.getTabSnapshot = getTabSnapshot;
    CmdUtils.getWindowSnapshot = getWindowSnapshot;
    CmdUtils.getImageSnapshot = getImageSnapshot;
    CmdUtils.savePassword = savePassword;
    CmdUtils.loadPassword = loadPassword;
    CmdUtils.retrieveLogins = retrieveLogins;
    CmdUtils.CreateCommand = CreateCommand;
    CmdUtils.CreateAlias = CreateAlias;
    CmdUtils.makeSearchCommand = makeSearchCommand;
    CmdUtils.makeBookmarkletCommand = makeBookmarkletCommand;
    CmdUtils.renderTemplate = renderTemplate;
    CmdUtils.previewAjax = previewAjax;
    CmdUtils.previewCallback = previewCallback;
    CmdUtils.previewList = previewList;
    CmdUtils.absUrl = absUrl;
    CmdUtils.safeWrapper = safeWrapper;

});
