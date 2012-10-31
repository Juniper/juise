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
 *   Maria Emerson <memerson@mozilla.com>
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

// var EXPORTED_SYMBOLS = ["UbiquitySetup"];

// const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

// Cu.import("resource://gre/modules/Services.jsm");
// Cu.import("/ubiquity/modules/utils.js");
// Cu.import("/ubiquity/modules/msgservice.js");
// Cu.import("/ubiquity/modules/feedmanager.js");
// Cu.import("/ubiquity/modules/default_feed_plugin.js");
// Cu.import("/ubiquity/modules/annotation_memory.js");
// Cu.import("/ubiquity/modules/suggestion_memory.js");
// Cu.import("/ubiquity/modules/feedaggregator.js");
// Cu.import("/ubiquity/modules/webjsm.js");
// Cu.import("/ubiquity/modules/prefcommands.js");
// Cu.import("/ubiquity/modules/skin_feed_plugin.js");

var gServices, gWebJsModule, gPrefs = Utils.prefs;

const RESET_SCHEDULED_PREF = "extensions.ubiquity.isResetScheduled";
const VERSION_PREF ="extensions.ubiquity.lastversion";
const ANN_DB_FILENAME = "ubiquity_ann.sqlite";

var UbiquitySetup = {
  isNewlyInstalledOrUpgraded: false,

  STANDARD_FEEDS_URI: "/ubiquity/standard-feeds/",
  STANDARD_FEEDS: [{
    source: "firefox.js",
    title: "Mozilla Browser Commands",
  }, {
    source: "social.js",
    title: "Mozilla Social Networking Commands",
  }, {
    source: "developer.js",
    title: "Mozilla Developer Commands",
  }, {
    source: "pageedit.js",
    title: "Mozilla Page Editing Commands",
  }, {
    source: "general.js",
    title: "Mozilla General Utility Commands",
  }, {
    source: "email.js",
    title: "Mozilla Email Commands",
  }, {
    source: "calendar.js",
    title: "Mozilla Calendar Commands",
  }, {
    source: "map.js",
    title: "Mozilla Map Commands",
  }, {
    source: "search.xhtml",
    title: "Mozilla Web Search Commands",
  }],

  __maybeReset: function __maybeReset() {
    if (!this.isResetScheduled) return;

    // Reset feed subscriptions, preferences and suggestion memory.
    let annDb = AnnotationService.getProfileFile(ANN_DB_FILENAME);
    if (annDb.exists()) annDb.remove(false);
    gPrefs.resetBranch("extensions.ubiquity.");
    new SuggestionMemory("main_parser").wipe();
  },

  getBaseUri: function getBaseUri() this.baseUrl,

  isInstalledAsXpi: function isInstalledAsXpi() {
    let profileDir = Utils.DirectoryService.get("ProfD", Ci.nsIFile);
    let profileUrl = Utils.IOService.newFileURI(profileDir).spec;
    return !this.baseUrl.lastIndexOf(profileUrl, 0);
  },

  preload: function preload(callback) {
    if (gWebJsModule) {
      callback();
      return;
    }

    this.__maybeReset();

    Utils.AddonManager.getAddonByID(
      "ubiquity@labs.mozilla.com",
      function setAddonInfo(addon) {
        this.version = addon.version;
        this.baseUrl = addon.getResourceURI("").spec;
        gWebJsModule = new WebJsModule(callback);
      }.bind(this));
  },

  get isResetScheduled()
    gPrefs.get(RESET_SCHEDULED_PREF, false),
  set isResetScheduled(value)
    gPrefs.set(RESET_SCHEDULED_PREF, value),

  __removeExtinctStandardFeeds: function __rmExtinctStdFeeds(feedManager) {
    var OLD_STD_FEED_TESTERS = [
      /^https:\/\/ubiquity\.mozilla\.com\/standard-feeds\/./,
      RegExp("^" + Utils.regexp.quote(this.baseUrl) + "standard-feeds/"),
      /^resource:\/\/ubiquity\/standard-feeds\/.+\.html$/];

    feedManager.getAllFeeds().forEach(function removeExtinct(feed) {
      if (OLD_STD_FEED_TESTERS.some("".match, feed.uri.spec)) feed.purge();
    });
  },

  get services() this.createServices(),
  createServices: function createServices() {
    if (!gServices) {
      // Compare the version in our preferences from our version in the
      // install.rdf.
      var currVersion = gPrefs.getValue(VERSION_PREF, "firstrun");
      if (currVersion != this.version) {
        gPrefs.setValue(VERSION_PREF, this.version);
        this.isNewlyInstalledOrUpgraded = true;
      }

      var annDbFile = AnnotationService.getProfileFile(ANN_DB_FILENAME);
      var annDbConn = AnnotationService.openDatabase(annDbFile);
      var annSvc = new AnnotationService(annDbConn);

      var feedManager = new FeedManager(annSvc);
      var msgService = new CompositeMessageService();

      msgService.add(new AlertMessageService());
      msgService.add(new ErrorConsoleMessageService());

      var disabledStorage =
        new DisabledCmdStorage("extensions.ubiquity.disabledCommands");

      var defaultFeedPlugin =
        new DefaultFeedPlugin(
          feedManager, msgService, gWebJsModule, "/ubiquity/");

      var cmdSource =
        new FeedAggregator(
          feedManager, msgService, disabledStorage.getDisabledCommands());
      disabledStorage.attach(cmdSource);

      gServices = {
        commandSource: cmdSource,
        feedManager: feedManager,
        messageService: msgService,
        skinService: SkinFeedPlugin(feedManager, msgService, gWebJsModule),
        webJsm: gWebJsModule,
      };

      Services.obs.addObserver({
        observe: function fm_fin() feedManager.finalize(),
      }, "quit-application", false);

      PrefCommands.init(feedManager);

      if (this.isNewlyInstalledOrUpgraded) {
        this.__removeExtinctStandardFeeds(feedManager);

        // For some reason, the following function isn't executed
        // atomically by Javascript; perhaps something being called is
        // getting the '@mozilla.org/thread-manager;1' service and
        // spinning via a call to processNextEvent() until some kind of
        // I/O is finished?
        defaultFeedPlugin.installDefaults(this.STANDARD_FEEDS_URI,
                                          this.STANDARD_FEEDS);
      }

      cmdSource.refresh();
    }

    return gServices;
  },

  setupWindow: function setupWindow(window) {
    gServices.feedManager.installToWindow(window);

    function onPageLoad(event) {
      if (gPrefs.get("extensions.ubiquity.enablePageLoadHandlers", true) &&
          event.originalTarget.location)
        gServices.commandSource.onPageLoad(event.originalTarget);
    }

    for each (let id in ["appcontent", "sidebar"]) {
      let browser = window.document.getElementById(id);
      if (browser)
        browser.addEventListener("DOMContentLoaded", onPageLoad, true);
    }
  },

  get languageCode()
    gPrefs.getValue("extensions.ubiquity.language", "en"),

  get parserVersion()
    gPrefs.getValue("extensions.ubiquity.parserVersion", 2),
};
function DisabledCmdStorage(prefName) {
  var disabledCommands = JSON.parse(gPrefs.getValue(prefName, "{}"));

  this.getDisabledCommands = function getDisabledCommands() {
    return disabledCommands;
  };

  function onDisableChange(eventName) {
    gPrefs.setValue(prefName, JSON.stringify(disabledCommands));
  }

  this.attach = function attach(cmdSource) {
    cmdSource.addListener("disabled-command-change", onDisableChange);
  };
}
