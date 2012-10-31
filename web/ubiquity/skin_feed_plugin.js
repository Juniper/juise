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

// = SkinFeedPlugin =
// The boss of {{{SkinFeed}}}s, aka {{{skinService}}}.

var EXPORTED_SYMBOLS = ["SkinFeedPlugin"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("/ubiquity/modules/utils.js");
Cu.import("/ubiquity/modules/codesource.js");
Cu.import("/ubiquity/modules/localization_utils.js");

const L = LocalizationUtils.propertySelector(
  "/ubiquity/chrome/locale/coreubiquity.properties");
const SSS = (Cc["@mozilla.org/content/style-sheet-service;1"]
             .getService(Ci.nsIStyleSheetService));
const PREF_SKIN = "extensions.ubiquity.skin";
const PREF_CUSTOM = "extensions.ubiquity.customCss";
const PREF_REMOTE_TIMEOUT = "extensions.ubiquity.remoteUriTimeout";
const URL_ROOT = "/ubiquity/chrome/skin/skins/";
const URL_CUSTOM = "ubiquity://custom-skin-css";
const URL_DEFAULT = URL_ROOT + "experimental.css";

var gCurrentCssUri = Utils.uri("data:text/css,");

function SkinFeedPlugin(feedManager, msgService, webJsm) {
  SFP._feedManager = feedManager;
  SFP._msgService = msgService;
  SFP._webJsm = webJsm;
  feedManager.registerPlugin(SFP);
  for each (let url in [
    URL_CUSTOM, URL_DEFAULT, URL_ROOT + "default.css", URL_ROOT + "old.css"])
    feedManager.addSubscribedFeed({
      type: "ubiquity-skin",
      url: url, sourceUrl: url, title: url,
      isBuiltIn: true,
      canAutoUpdate: true,
    });
  SFP.customSkin.__defineSetter__("css", function SF_setCustomCss(css) {
    Utils.prefs.set(PREF_CUSTOM, css);
  });
  SFP.currentSkin.pick(true);
  return SFP;
}
var SFP = Utils.extend(SkinFeedPlugin.prototype, {
  type: "ubiquity-skin",
  notifyMessage: L("ubiquity.skinsvc.newskinfound"),

  makeFeed: function SFP_makeFeed(baseFeed, eventHub)
    SkinFeed(baseFeed, eventHub, this._msgService),
  onSubscribeClick: function SFP_onSubscribeClick(pageUrl, link) {
    var cssUrl = Utils.gist.fixRawUrl(link.href), me = this;
    me._webJsm.jQuery.ajax({
      url: cssUrl,
      dataType: "text",
      success: function yay(css) {
        me._feedManager.addSubscribedFeed({
          type: "ubiquity-skin",
          url: pageUrl,
          sourceUrl: cssUrl,
          sourceCode: css,
          canAutoUpdate: true,
        }).getFeedForUrl(cssUrl).pick();
        Utils.tabs.reload(/^about:ubiquity\?settings\b/);
      },
      error: Utils.log,
    });
  },

  // === {{{ SkinFeedPlugin.skins }}} ===
  // Installed {{{SkinFeed}}}s as array.
  get skins() [
    feed for each (feed in this._feedManager.getSubscribedFeeds())
    if (feed.type === "ubiquity-skin")],

  // === {{{ SkinFeedPlugin.customSkin }}} ===
  get customSkin()
    this._feedManager.getFeedForUrl(URL_CUSTOM),

  // === {{{ SkinFeedPlugin.defaultSkin }}} ===
  get defaultSkin()
    this._feedManager.getFeedForUrl(URL_DEFAULT),

  // === {{{ SkinFeedPlugin.currentSkin }}} ===
  get currentSkin() (
    this._feedManager.getFeedForUrl(Utils.prefs.get(PREF_SKIN, URL_DEFAULT)) ||
    this.defaultSkin),

  // === {{{ SkinFeedPlugin.saveAs(cssText, defaultName) }}} ===
  // Saves {{{cssText}}} to a file and subscribes to it.
  saveAs: function SFP_saveAs(cssText, defaultName) {
    const {nsIFilePicker} = Ci;
    var fp = Cc["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    fp.init(Utils.currentChromeWindow,
            L("ubiquity.skinsvc.saveyourskin"),
            nsIFilePicker.modeSave);
    fp.defaultString = defaultName || "";
    fp.appendFilter("CSS (*.css)", "*.css");
    var rv = fp.show();
    if (rv !== nsIFilePicker.returnOK &&
        rv !== nsIFilePicker.returnReplace) return "";
    var {file, fileURL: {spec}} = fp;
    try {
      let fos = Cc["@mozilla.org/network/file-output-stream;1"]
                .createInstance(Ci.nsIFileOutputStream);
      fos.init(file, 0x02 | 0x08 | 0x20, 0644, 0);
      fos.write(cssText, cssText.length);
      fos.close();
    } catch (e) {
      e.message =
        "Error writing Ubiquity skin to " + file.path + ": " + e.message;
      Cu.reportError(e);
    }
    this.onSubscribeClick(spec, {href: spec});
    return file.path;
  },
  toString: function SFP_toString() "[object SkinFeedPlugin]",
});

// == SkinFeed ==

function SkinFeed(baseFeed, eventHub, msgService) Utils.extend({
  __proto__: baseFeed,
  _msgService: msgService,
  _codeSource:
    RemoteUriCodeSource.isValidUri(baseFeed.uri)
    ? new RemoteUriCodeSource(baseFeed,
                              Utils.prefs.get(PREF_REMOTE_TIMEOUT, 3e5))
    : new LocalUriCodeSource(baseFeed.uri.spec),
  _dataCache: null,
}, SkinFeed.prototype);
Utils.extend(SkinFeed.prototype, {
  // === {{{ SkinFeed#css }}} ===
  // CSS code of this skin. Settable if custom.
  get css() {
    var code = this._codeSource.getCode();
    if (this._codeSource.updated) this._dataCache = null;
    return code;
  },

  // === {{{ SkinFeed#dataUri }}} ===
  // Data URI object used to register this skin.
  get dataUri()
    Utils.uri("data:text/css;charset=utf-8,/*ubiquity-skin*/" +
              encodeURIComponent(this.css)),

  // === {{{ SkinFeed#metaData }}} ===
  // Contents of the meta data block ({{{ =skin= ~ =/skin= }}}).
  get metaData() {
    if (this._dataCache) return this._dataCache;
    var {css} = this, data = {};
    var [, block] = /=skin=\s*([^]+)\s*=\/skin=/.exec(css) || 0;
    if (block) {
      let re = /^[ \t]*@(\w+)[ \t]+(.+)/mg, m;
      while ((m = re.exec(block))) data[m[1]] = m[2].trim();
    }
    if (!("name" in data)) let ({spec} = this.uri)
      data.name = spec.slice(spec.lastIndexOf("/") + 1);
    if (!("homepage" in data))
      data.homepage = this.pageUri.spec;
    return this._dataCache = data;
  },

  // === {{{ SkinFeed#pick(silently = false) }}} ===
  // Applies this skin. Won't notify user if {{{silently}}}.
  pick: function SF_pick(silently) {
    try {
      (SSS.sheetRegistered(gCurrentCssUri, SSS.USER_SHEET) &&
       SSS.unregisterSheet(gCurrentCssUri, SSS.USER_SHEET));
      var {dataUri, uri} = this;
      SSS.loadAndRegisterSheet(dataUri, SSS.USER_SHEET);
      gCurrentCssUri = dataUri;
      Utils.prefs.set(PREF_SKIN, uri.spec);
    } catch (e) {
      this._msgService.displayMessage(
        "Error applying Ubiquity skin from " + uri.spec);
      Cu.reportError(e);
    }
    silently ||
      this._msgService.displayMessage(L("ubiquity.skinsvc.skinchanged"));
    return this;
  },
  refresh: function SF_refresh() this,
  toString: function SF_toString() "[object SkinFeed<" + this.uri.spec + ">]",
});

Cu.import("/ubiquity/modules/ubiquity_protocol.js", null).setPath(
  /[^/]*$/.exec(URL_CUSTOM)[0], function customSkinUri() {
    var css = Utils.prefs.get(PREF_CUSTOM);
    if (!css) css = Utils.getLocalUrl(URL_ROOT + "custom.css");
    return "data:text/css;charset=utf-8," + encodeURIComponent(css);
  });
