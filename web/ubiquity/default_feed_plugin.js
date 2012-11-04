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
 *   Blair McBride <unfocused@gmail.com>
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

// var EXPORTED_SYMBOLS = ["DefaultFeedPlugin"];
jQuery(function ($) {

// const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

// Cu.import("/ubiquity/modules/utils.js");
// Cu.import("/ubiquity/modules/codesource.js");
// Cu.import("/ubiquity/modules/sandboxfactory.js");
// Cu.import("/ubiquity/modules/feed_plugin_utils.js");

const Global = this;
const CONFIRM_URL = "/ubiquity/chrome/content/confirm-add-command.xhtml";
const DEFAULT_FEED_TYPE = "commands";
const TRUSTED_DOMAINS_PREF = "extensions.ubiquity.trustedDomains";
const REMOTE_URI_TIMEOUT_PREF = "extensions.ubiquity.remoteUriTimeout";
const COMMANDS_BIN_PREF = "extensions.ubiquity.commands.bin";

function DefaultFeedPlugin(feedManager, messageService, webJsm, baseUrl) {
  this.type = DEFAULT_FEED_TYPE;

  let builtins = makeBuiltins(baseUrl);
  let builtinGlobalsMaker = makeBuiltinGlobalsMaker(messageService, webJsm);
  let sandboxFactory = new SandboxFactory(builtinGlobalsMaker);

  for (let [title, url] in new Iterator(builtins.feeds))
    feedManager.addSubscribedFeed({
      url: url,
      sourceUrl: url,
      canAutoUpdate: true,
      isBuiltIn: true,
      title: title,
    });

  this.installDefaults = function DFP_installDefaults(baseUrl, infos) {
    for each (let info in infos) {
      let url = baseUrl + info.source;
      if (feedManager.isUnsubscribedFeed(url)) continue;
      feedManager.addSubscribedFeed({
        sourceUrl: url,
        sourceCode: new LocalUriCodeSource(url).getCode(),
        canAutoUpdate: true,
        title: info.title,
      });
    }
  };

  this.onSubscribeClick = function DFP_onSubscribeClick(pageUrl, targetLink) {
    var doc = targetLink.ownerDocument;
    var title = targetLink.title || Utils.gist.getName(doc) || doc.title;
    var commandsUrl = Utils.gist.fixRawUrl(targetLink.href);
    // Clicking on "subscribe" takes them to the warning page:
    var confirmUrl = CONFIRM_URL + Utils.paramsToString({
      url: pageUrl,
      sourceUrl: commandsUrl,
      title: title,
    });

    if (!LocalUriCodeSource.isValidUri(commandsUrl) &&
        !isTrustedUrl(commandsUrl, targetLink.type)) {
      Utils.openUrlInBrowser(confirmUrl);
      return;
    }

    function onSuccess(data) {
      feedManager.addSubscribedFeed({
        url: pageUrl,
        sourceUrl: commandsUrl,
        title: title,
        canAutoUpdate: true,
        sourceCode: data,
      });
      Utils.openUrlInBrowser(confirmUrl);
    }

    if (RemoteUriCodeSource.isValidUri(commandsUrl))
      webJsm.jQuery.ajax({
        url: commandsUrl,
        dataType: "text",
        success: onSuccess,
        error: Utils.log,
      });
    else
      onSuccess("");
  };

  this.makeFeed = function DFP_makeFeed(baseFeedInfo, hub)
    new DFPFeed(baseFeedInfo, hub, messageService, sandboxFactory,
                builtins.headers, builtins.footers, webJsm.jQuery);

  feedManager.registerPlugin(this);
}

function isTrustedUrl(commandsUrl, mimetype) {
  // Even if the command feed resides on a trusted host, if the
  // mime-type is application/x-javascript-untrusted or
  // application/xhtml+xml-untrusted, the host itself doesn't
  // trust it (perhaps because it's mirroring code from
  // somewhere else).
  if (mimetype === "application/x-javascript-untrusted" ||
      mimetype === "application/xhtml+xml-untrusted") return false;

  var {scheme, host} = Utils.uri(commandsUrl);
  if (scheme !== "https") return false;

  let domains = Utils.prefs.getValue(TRUSTED_DOMAINS_PREF, "").split(",");
  for each (let d in domains) if (d === host) return true;

  return false;
}

DefaultFeedPlugin.makeCmdForObj = makeCmdForObj;

function makeCmdForObj(sandbox, commandObject, feedUri) {
  // referenceName is set by CreateCommand, so this command must have
  // bypassed CreateCommand. Let's set the referenceName here.
  if (!("referenceName" in commandObject))
    commandObject.referenceName = commandObject.name;

  function proxy(type) function withContext(context) {
    if (context) {
      context.l10n = commandObject.referenceName + "." + type;
      sandbox.context = context;
    }
    return commandObject[type].apply(cmd, Array.slice(arguments, 1));
  };

  var cmd = {
    __proto__: commandObject,
    id: feedUri.spec + "#" + commandObject.referenceName,
    feedUri: feedUri,
    toString: function UC_toString()
      "[object UbiquityCommand<" + this.id + ">]",
    execute: proxy("execute"),
    preview: proxy("preview"),
  };

  if (!("serviceDomain" in commandObject)) {
    let domain = /\bhttps?:\/\/([\w.-]+)/;
    cmd.serviceDomain = (
      (domain.test(commandObject.url) ||
       domain.test(commandObject.execute) ||
       domain.test(commandObject.preview))
      ? RegExp.$1
      : null);
    // TODO: also check for serviceDomain in Utils.getCookie type code
  }

  return finishCommand(cmd);
}

function makeCodeSource(feedInfo, headerSources, footerSources) {
  var {srcUri} = feedInfo, codeSource;

  if (RemoteUriCodeSource.isValidUri(srcUri))
    codeSource = new RemoteUriCodeSource(
      feedInfo,
      (feedInfo.canAutoUpdate
       ? Utils.prefs.getValue(REMOTE_URI_TIMEOUT_PREF, 10)
       : -1));
  else if (LocalUriCodeSource.isValidUri(srcUri))
    codeSource = new LocalUriCodeSource(srcUri.spec);
  else
    throw new Error("Don't know how to make code source for " + srcUri.spec);

  codeSource = new XhtmlCodeSource(codeSource);

  codeSource = new MixedCodeSource(codeSource,
                                   headerSources,
                                   footerSources);

  return codeSource;
}

function DFPFeed(feedInfo, hub, messageService, sandboxFactory,
                 headerSources, footerSources, jQuery) {
  var self = {
    __proto__: feedInfo,
    _hub: hub,
    _messageService: messageService,
    _sandboxFactory: sandboxFactory,
    _codeSource: makeCodeSource(feedInfo, headerSources, footerSources),
    _jQuery: jQuery,
    _sandbox: {},
    _bin: null,
    commands: {},
    canAutoUpdate: true,
  };
  LocalUriCodeSource.isValidUri(feedInfo.srcUri) || delete self.canAutoUpdate;

  return Utils.extend(
    self, DFPFeed.prototype, feedInfo.isBuiltIn && BuiltInFeedProto);
}
DFPFeed.prototype = {
  constructor: DFPFeed,
  toString: function DFPF_toString() "[object DefaultFeed]",

  refresh: function DFPF_refresh(anyway) {
    var codeSource = this._codeSource;
    var code = codeSource.getCode();
    if (!anyway && !codeSource.updated) return;

    this.teardown();
    this.commands = {};
    var factory = this._sandboxFactory;
    var sandbox = this._sandbox = factory.makeSandbox(codeSource);
    sandbox.Bin = this._bin || (this._bin = this.makeBin());
    try {
      factory.evalInSandbox(code, sandbox, codeSource.codeSections);
    } catch (e) {
      this._messageService.displayMessage({
        text: "An exception occurred while loading a command feed.",
        exception: e,
      });
    }

    var {uri} = this;
    for each (let cmd in sandbox.commands) {
      let newCmd = makeCmdForObj(sandbox, cmd, uri);
      this.commands[newCmd.id] = newCmd;
    }
    for each (let p in ["pageLoadFuncs", "ubiquityLoadFuncs"])
      this[p] = sandbox[p];
    this.metaData = sandbox.feed;
    this._hub.notifyListeners("feed-change", uri);
  },

  checkForManualUpdate: function DFPF_checkForManualUpdate(cb) {
    if (LocalUriCodeSource.isValidUri(this.srcUri)) {
      cb(false);
      return;
    }

    var self = this;
    this._jQuery.ajax({
      url: this.srcUri.spec,
      dataType: "text",
      success: onSuccess,
      error: function onError() { cb(false) },
    });
    function onSuccess(data) {
      if (data === self.getCode())
        cb(false);
      else
        cb(true, CONFIRM_URL + Utils.paramsToString({
          url: self.uri.spec,
          sourceUrl: self.srcUri.spec,
          updateCode: data,
        }));
    }
  },
  purge: function DFPF_purge() {
    this.teardown();
    this.finalize();
    this.__proto__.purge();
  },
  teardown: function DFPF_teardown() {
    try {
      let box = this._sandbox;
      for (let key in new Iterator(box, true))
        if (~key.lastIndexOf("teardown_", 0) && typeof box[key] == "function")
          try { box[key]() } catch (e) { Cu.reportError(e) }
    } catch (e) { Cu.reportError(e) }
  },
  finalize: function DFPF_finalize() {
    // Not sure exactly why, but we get memory leaks if we don't
    // manually remove these.
    this._jQuery = this._sandbox.jQuery = this._sandbox.$ = null;
  },
};

function makeBuiltinGlobalsMaker(msgService, webJsm) {
  webJsm.importScript("/ubiquity/scripts/jquery.js");
  webJsm.importScript("/ubiquity/scripts/jquery_setup.js");
  webJsm.importScript("/ubiquity/scripts/template.js");
  webJsm.importScript("/ubiquity/scripts/date.js");

  var globalObjects = {__proto__: null};
  var jsmExports = ["Components", "atob", "btoa"];
  var webExports = [
    "jQuery", "$", "Date", "Application",
    "Audio", "DOMParser", "Image", "KeyEvent", "NodeFilter",
    "XMLHttpRequest", "XMLSerializer", "XPathResult"];
  function displayMessage(msg, cmd) {
    if (Utils.classOf(msg) !== "Object") msg = {text: msg};
    if (cmd) {
      msg.icon  = cmd.icon;
      msg.title = cmd.name;
    }
    msgService.displayMessage(msg);
  }

  return function makeGlobals(codeSource) {
    var {id} = codeSource, globals = {
      feed: {id: id, dom: codeSource.dom},
      context: {},
      commands: [],
      pageLoadFuncs: [],
      ubiquityLoadFuncs: [],
      globals: globalObjects[id] || (globalObjects[id] = {}),
      displayMessage: displayMessage,
      Template: webJsm.TrimPath,
    };
    for each (let key in jsmExports) globals[key] = Global[key];
    for each (let key in webExports) globals[key] = webJsm[key];
    return globals;
  };
}

function makeBuiltins(baseUrl) {
  var partsUrl = baseUrl + "feed-parts/";
  return {
    feeds: {"Builtin Commands": baseUrl + "builtin-feeds/builtincmds.js"},
    headers: [new LocalUriCodeSource(partsUrl + "header/initial.js", true)],
    footers: [new LocalUriCodeSource(partsUrl + "footer/final.js"  , true)],
  };
}

var BuiltInFeedProto = {
  getJSONStorage: function BF_getJSONStorage()
    JSON.parse(Utils.prefs.getValue(COMMANDS_BIN_PREF, "{}")),
  setJSONStorage: function BF_setJSONStorage(obj) {
    var data = JSON.stringify(obj);
    Utils.prefs.setValue(COMMANDS_BIN_PREF, data);
    return JSON.parse(data);
  },
};

   $.u.DefaultFeedPlugin = DefaultFeedPlugin;

});
