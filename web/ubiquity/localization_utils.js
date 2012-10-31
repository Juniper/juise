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

// = LocalizationUtils =

// var EXPORTED_SYMBOLS = ["LocalizationUtils"];

// const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

// Cu.import("/ubiquity/modules/utils.js");
// Cu.import("/ubiquity/scripts/gettext/lib/Gettext.js")

const LocalizableProperties = ["names", "help", "description"];
const DefaultLanguageCodes  = ["en", "$"];
var loadedPo = {};
var feedGlobalsDict = {
  // feedUrl: [key0, key1, ...], ...
};

Gettext.prototype.get_lang_refs = function () [];

var LocalizationUtils = {
  GETTEXT: new Gettext(),

  get loadedPo() loadedPo,

  isLocalizableLang: function LU_isLocalizableLang(langCode)
    DefaultLanguageCodes.indexOf(langCode) < 0,

  isLocalizableFeed: function LU_isLocalizableFeed(feedUrl)
    /^resource:\/\/ubiquity\/(?:builtin|standard)-feeds\b/.test(feedUrl),

  loadLocalPo: function LU_loadLocalPo(feedUrl, langCode) {
    if (!LocalizationUtils.isLocalizableLang(langCode) ||
        !LocalizationUtils.isLocalizableFeed(feedUrl)) return false;

    var feedKey = this.getLocalFeedKey(feedUrl, langCode);
    if (feedKey in loadedPo) return true;

    var poUrl = "/ubiquity/localization/" + feedKey + ".po";
    try { var po = Utils.getLocalUrl(poUrl, "utf-8"); } catch (e) {}
    if (!po) return false;

    try {
      let parsed = this.GETTEXT.parse_po(po);
      let rv = {};
      rv[feedKey] = parsed;
      this.GETTEXT.parse_locale_data(rv);
      loadedPo[feedKey] = parsed;
    } catch (e) {
      Utils.dump("couldn't parse " + poUrl);
      return false;
    }
    Utils.dump("loaded " + poUrl);
    return true;
  },

  getLocalFeedKey: function LU_getLocalFeedKey(path, langCode)
    langCode + /\/\w+(?=\.\w+$)/.exec(path),

  getLocalizedString: function LU_getLocalizedString(feedKey, key) {
    try {
      return this.GETTEXT.dgettext(feedKey, key);
    } catch (ex) {
      return key;
    }
  },

  getLocalizedStringFromContext:
  function LU_getLocalizedStringFromContext(feedKey, context, key) {
    try {
      let rv = this.GETTEXT.dpgettext(feedKey, context, key);
      if (rv === key)
        // nothing was found in this context. try the general context
        rv = this.GETTEXT.dgettext(feedKey, key);
      return rv;
    } catch (ex) {
      return key;
    }
  },

  getLocalized: function LU_getLocalized(string) {
    var context = (this.commandContext +
                   (this.displayContext ? "." + displayContext : ""));
    var feedKey = this.getLocalFeedKey(this.feedContext.asciiSpec);
    return this.getLocalizedStringFromContext(feedKey, context, string);
  },

  getFeedGlobals:
  function LU_getFeedGlobals(feedUrl)
    feedGlobalsDict[feedUrl] || (feedGlobalsDict[feedUrl] = []),

  registerFeedGlobal: function LU_registerFeedGlobal(feedUrl, key)
    LocalizationUtils.getFeedGlobals(feedUrl).push(key),

  // === {{{ LocalizationUtils.localizeCommand(cmd, langCode) }}} ===
  //
  // Only works with Parser 2 commands.
  // It might magically work with Parser 1, but it's not built to, and not
  // tested that way.

  localizeCommand: function LU_localizeCommand(cmd, langCode) {
    if (!LocalizationUtils.isLocalizableLang(langCode)) return cmd;

    var url = cmd.feedUri.spec;
    if (!LocalizationUtils.isLocalizableFeed(url)) return cmd;

    var feedKey = LocalizationUtils.getLocalFeedKey(url, langCode);
    for each (let key in LocalizableProperties) if (cmd[key]) {
      let val = getLocalizedProperty(feedKey, cmd, key);
      if (val) cmd[key] = val;
    }

    if ("previewHtml" in cmd) {
      let key = cmd.previewHtml;
      let rv = LocalizationUtils.getLocalizedStringFromContext(
        feedKey, cmd.referenceName + ".preview", key);
      if (rv !== key) cmd.previewHtml = rv;
    }

    cmd.name = cmd.names[0];

    return cmd;
  },

  // === {{{ LocalizationUtils.propertySelector(properties) }}} ===
  //
  // Creates a {{{nsIStringBundle}}} for the .{{{properties}}} file and
  // returns a wrapper function which calls {{{GetStringFromName()}}}
  // (or {{{formatStringFromName()}}} if extra argument is passed)
  // for the given name string. e.g.:
  // {{{
  // (foo.properties)
  // some.key=%S-%S
  // --------------
  // var L = propertySelector("foo.properties");
  // L("some.key") //=> "%S-%S"
  // L("some.key", "A", "Z") //=> "A-Z"
  // }}}

  propertySelector: function LU_propertySelector(properties) {
    if (properties in LU_propertySelector)
      return LU_propertySelector[properties];
//     var bundle = (Cc["@mozilla.org/intl/stringbundle;1"]
//                   .getService(Ci.nsIStringBundleService)
//                   .createBundle(properties));
    return LU_propertySelector[properties] = function stringFor(name) (
        // Perhaps use getFormattedString() here?
        name);
//       arguments.length > 1
//       ? bundle.formatStringFromName(name,
//                                     Array.slice(arguments, 1),
//                                     arguments.length - 1)
//       : bundle.GetStringFromName(name));
  },
};

function getLocalizedProperty(feedKey, cmd, property) {
  var context = cmd.referenceName + "." + property;
  var key = cmd[property];
  var propIsArray = Utils.isArray(key);
  if (propIsArray) key = key.join("|");
  var rv =
    LocalizationUtils.getLocalizedStringFromContext(feedKey, context, key);
  return rv !== key && (propIsArray ? rv.split(/\s{0,}\|\s{0,}/) : rv);
}
