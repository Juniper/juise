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

// = FeedManager =
// The {{{FeedManager}}} class is used to manage Ubiquity's subscribed
// and unsubscribed feeds. It's responsible for communicating with
// feed plugins to reload feeds when they change and expose the
// functionality of all feeds to client code as {{{Feed}}} objects.

var EXPORTED_SYMBOLS = ["FeedManager"];

const Cu = Components.utils;

Cu.import("/ubiquity/modules/utils.js");
Cu.import("/ubiquity/modules/eventhub.js");
Cu.import("/ubiquity/modules/localization_utils.js");

var L = LocalizationUtils.propertySelector(
  "/ubiquity/chrome/locale/coreubiquity.properties");

const FEED_SRC_ANNO          = "ubiquity/source";
const FEED_TYPE_ANNO         = "ubiquity/type";
const FEED_AUTOUPDATE_ANNO   = "ubiquity/autoupdate";
const FEED_BUILTIN_ANNO      = "ubiquity/builtin";
const FEED_SUBSCRIBED_ANNO   = "ubiquity/confirmed";
const FEED_UNSUBSCRIBED_ANNO = "ubiquity/removed";
const FEED_SRC_URL_ANNO      = "ubiquity/commands";
const FEED_URL_ANNO          = "ubiquity/url";
const FEED_TITLE_ANNO        = "ubiquity/title";
const FEED_DATE_ANNO         = "ubiquity/date";
const FEED_BIN_ANNO          = "ubiquity/bin";

const FEED_ANNOS = [this[v] for (v in this) if (/^FEED_\w+_ANNO$/.test(v))];

const DEFAULT_FEED_TYPE = "commands";

// == The FeedManager Class ==
// The constructor for this class takes an instance of an annotation
// service, which has an interface that's virtually identical to
// {{{nsIAnnotationService}}}. For an example implementation, see
// {{{AnnotationService}}}.

function FeedManager(annSvc) {
  this._annSvc = annSvc;
  this._plugins = {__proto__: null};
  this._feeds = {__proto__: null};
  this._hub = new EventHub();
  this._hub.attachMethods(this);
}

var FMgrProto = FeedManager.prototype;

// === {{{FeedManager#registerPlugin()}}} ===
// Registers a feed plugin with the feed manager. For an example feed
// plugin, see {{{LockedDownFeedPlugin}}}.

FMgrProto.registerPlugin = function FMgr_registerPlugin(plugin) {
  if (plugin.type in this._plugins)
    throw new Error("Feed plugin for type '" + plugin.type +
                    "' already registered.");

  this._plugins[plugin.type] = plugin;
};

FMgrProto.__getFeedsForAnnotation = function FMgr__gFFA(anno) {
  var feeds = [];
  for each (let uri in this._annSvc.getPagesWithAnnotation(anno)) {
    let feed = this.__getFeed(uri);
    if (feed) feeds.push(feed);
  }
  return feeds;
};

// === {{{FeedManager#getAllFeeds()}}} ===
// Returns an array of all known {{{Feed}}} objects.

FMgrProto.getAllFeeds = function FMgr_getAllFeeds() {
  return this.__getFeedsForAnnotation(FEED_TYPE_ANNO);
};

// === {{{FeedManager#getUnsubscribedFeeds()}}} ===
// Returns an array of {{{Feed}}} objects that represent all feeds
// that were once subscribed, but are currently unsubscribed.

FMgrProto.getUnsubscribedFeeds = function FMgr_getUnsubscribedFeeds() {
  return this.__getFeedsForAnnotation(FEED_UNSUBSCRIBED_ANNO);
};

// === {{{FeedManager#getSubscribedFeeds()}}} ===
// Returns an array of {{{Feed}}} objects that represent all feeds
// that are currently subscribed.

FMgrProto.getSubscribedFeeds = function FMgr_getSubscribedFeeds() {
  return this.__getFeedsForAnnotation(FEED_SUBSCRIBED_ANNO);
};

// === {{{FeedManager#getFeedForUrl(url)}}} ===
// Returns the feed for the given URL, if it exists. If it doesn't,
// this function returns {{{null}}}.

FMgrProto.getFeedForUrl = function FMgr_getFeedForUrl(url) {
  return this.__getFeed(Utils.uri(url));
};

// === {{{FeedManager#addSubscribedFeed()}}} ===
// Adds a feed with the given information to the {{{FeedManager}}}. The
// information should be passed as a single {{{Object}}} with keys that
// correspond to values:
//
//   * {{{isBuiltIn}}} is a boolean that indicates whether the feed is
//     to be treated as a built-in feed. A built-in feed should not be
//     able to be unsubscribed-from by the user, and the lifetime of
//     its subscription does not persist across application restarts.
//   * {{{type}}} is the type of the feed; this is usually specified by
//     the {{{rel}}} attribute contained in a HTML page's {{{<link>}}}
//     tag, and determines what feed plugin is used to load and process
//     the feed.
//   * {{{url}}} is the URL of the feed.
//   * {{{sourceUrl}}} is the URL of the source code of the feed.
//   * {{{sourceCode}}} is the actual source code for the feed,
//     which is cached.
//   * {{{canAutoUpdate}}} specifies whether or not the latest version
//     of the feed's source code should be fetched from the
//     network. If this isn't {{{true}}}, then the feed manager will
//     only ever use the cached version of the source code.
//   * {{{title}}} is the human-readable name for the feed.

FMgrProto.addSubscribedFeed = function FMgr_addSubscribedFeed(info) {
  let annSvc = this._annSvc;
  let uri = Utils.uri(info.sourceUrl);
  let expiration = annSvc[info.isBuiltIn ? "EXPIRE_SESSION" : "EXPIRE_NEVER"];

  annSvc.removePageAnnotation(uri, FEED_UNSUBSCRIBED_ANNO);

  annSvc.setPageAnnotation(uri, FEED_TYPE_ANNO,
                           info.type || DEFAULT_FEED_TYPE, 0, expiration);
  annSvc.setPageAnnotation(uri, FEED_SRC_URL_ANNO,
                           info.sourceUrl, 0, expiration);
  annSvc.setPageAnnotation(uri, FEED_SUBSCRIBED_ANNO,
                           "true", 0, expiration);
  if (info.sourceCode)
    annSvc.setPageAnnotation(uri, FEED_SRC_ANNO,
                             info.sourceCode, 0, expiration);
  if (info.canAutoUpdate)
    annSvc.setPageAnnotation(uri, FEED_AUTOUPDATE_ANNO,
                             !!info.canAutoUpdate, 0, expiration);
  if (info.url)
    annSvc.setPageAnnotation(uri, FEED_URL_ANNO,
                             info.url, 0, expiration);
  if (info.title)
    annSvc.setPageAnnotation(uri, FEED_TITLE_ANNO,
                             info.title, 0, expiration);
  if (info.isBuiltIn)
    annSvc.setPageAnnotation(uri, FEED_BUILTIN_ANNO,
                             "true", 0, expiration);
  else
    annSvc.setPageAnnotation(uri, FEED_DATE_ANNO,
                             new Date().toUTCString(), 0, expiration);

  this._hub.notifyListeners("subscribe", uri);
  return this;
};

// === {{{FeedManager#isSubscribedFeed()}}} ===
// Returns whether or not the given feed URL is currently being
// subscribed to.

FMgrProto.isSubscribedFeed = function FMgr_isSubscribedFeed(uri) (
  this._annSvc.pageHasAnnotation(Utils.uri(uri), FEED_SUBSCRIBED_ANNO));

// === {{{FeedManager#isUnsubscribedFeed()}}} ===
// Returns whether or not the given feed URL was once subscribed
// to, but is no longer.

FMgrProto.isUnsubscribedFeed = function FMgr_isUnsubscribedFeed(uri) (
  this._annSvc.pageHasAnnotation(Utils.uri(uri), FEED_UNSUBSCRIBED_ANNO));

// === {{{FeedManager#installToWindow()}}} ===
// This function installs the feed manager user interface to the
// given chrome window that represents a web browser.
//
// Whenever the window loads a web page containing a {{{<link>}}} tag
// that identifies it as a feed that can be loaded by one of the feed
// manager's registered plugins, the feed manager displays a
// notification box informing the user that they can subscribe to the
// feed.
//
// If the user clicks on the notification box's "Subscribe..." button,
// the feed manager passes control to the feed plugin responsible for
// loading the feed.

FMgrProto.installToWindow = function FMgr_installToWindow(window) {
  var self = this;

  function onRelatedPage(plugin, pageUrl, link) {
    if (self.isSubscribedFeed(pageUrl) ||
        self.isSubscribedFeed(link.href)) return;

    Utils.notify({
      target: link.ownerDocument,
      label: (plugin.notifyMessage ||
              L("ubiquity.feedmanager.newcommandfound")),
      value: "ubiquity_notify_feed_available",
      priority: "INFO_MEDIUM",
      buttons: [{
        accessKey: "S",
        callback: function onSubscribeClick(notification, button) {
          plugin.onSubscribeClick(pageUrl, link);
        },
        label: L("ubiquity.feedmanager.subscribe"),
      }]});
  }

  // Watch for any tags of the form <link rel="...">
  // on pages and add annotations for them if they exist.
  window.addEventListener("DOMLinkAdded", function onLinkAdded(event) {
    var {target} = event;
    if (!(target.rel in self._plugins) || !target.href) return;

    var pageUrl = target.baseURI;
    var hashIndex = pageUrl.indexOf("#");
    if (hashIndex !== -1) pageUrl = pageUrl.slice(0, hashIndex);

    onRelatedPage(self._plugins[target.rel], pageUrl, target);
  }, false);

  for each (let plugin in this._plugins)
    if ("installToWindow" in plugin) plugin.installToWindow(window);
};

// === {{{FeedManager#finalize()}}} ===
// Performs any necessary cleanup on the feed manager. Should be
// called when the feed manager no longer needs to be used.

FMgrProto.finalize = function FMgr_finalize() {
  for each (var feed in this._feeds) feed.finalize();
};

FMgrProto.__getFeed = function FMgr___getFeed(uri) {
  var {spec} = uri, self = this, feeds = self._feeds;
  if (!(spec in feeds)) {
    try { feeds[spec] = self.__makeFeed(uri) } catch (e) {
      Utils.reportError(e);
      Utils.reportInfo(
        "An error occurred when retrieving the feed for " + spec);
      // remove the mal-URI here, since we can't "purge" it as feed
      self._annSvc.removePageAnnotations(uri);
      return null;
    }
    self.addListener("purge", function onPurge(eventName, aUri) {
      if (aUri.spec !== spec) return;
      feeds[spec].finalize();
      delete feeds[spec];
      self.removeListener("purge", onPurge);
    });
  }
  return feeds[spec];
};

// == The Feed Class ==
// Instances of {{{Feed}}} classes are generated by the feed manager
// as necessary; there's no public constructor for them.

function Feed(uri, annSvc, hub) {
  // === {{{Feed#uri}}} ===
  // An {{{nsIURI}}} identifying this feed. Read-only.
  this.uri = uri;

  // === {{{Feed#srcUri}}} ===
  // An {{{nsIURI}}} corresponding to the URL for the feed's source code.
  // Read-only.
  this.srcUri = Utils.uri(
    annSvc.getPageAnnotation(uri, FEED_SRC_URL_ANNO, uri), uri);

  // === {{{Feed#pageUri}}} ===
  // An {{{nsIURI}}} corresponding to the feed's URL. This is the
  // human-readable page that the end-user clicked the "Subscribe..."
  // button on; it is not necessarily the same page that contains the
  // feed's actual source code. Read-only.
  this.pageUri = Utils.uri(
    annSvc.getPageAnnotation(uri, FEED_URL_ANNO, uri), uri);

  this._annSvc = annSvc;
  this._hub = hub;
  this._expiration =
    annSvc[this.isBuiltIn ? "EXPIRE_SESSION" : "EXPIRE_NEVER"];
}
Feed.prototype = {
  constructor: Feed,
  toString: function Feed_toString() "[object Feed]",

  // === {{{Feed#viewSourceUri}}} ===
  // Returns the {{{nsIURI}}} for the feed's source code. If the source
  // code only exists as cached data, this may be a data URI.
  get viewSourceUri()
    this.canAutoUpdate
    ? this.srcUri
    : Utils.uri("data:;charset=utf-8," + encodeURIComponent(this.getCode())),

  // === {{{Feed#type}}} ===
  // A string identifying the type of the feed. This is usually the
  // same as the {{{rel}}} attribute contained in a HTML page's
  // {{{<link>}}} tag, and determines what feed plugin is used to load
  // and process the feed. Read-only.
  get type()
    this._annSvc.getPageAnnotation(this.uri, FEED_TYPE_ANNO, DEFAULT_FEED_TYPE),

  // === {{{Feed#title}}} ===
  // The human-readable name for the feed. Read-only.
  get title()
    this._annSvc.getPageAnnotation(this.uri, FEED_TITLE_ANNO, this.uri.spec),

  // === {{{Feed#date}}} ===
  // Subscribed {{{Date}}} of the feed. {{{new Date(0)}}} for builtin feeds.
  // Read-only.
  get date()
    new Date(this._annSvc.getPageAnnotation(this.uri, FEED_DATE_ANNO, 0)),

  // === {{{Feed#isBuiltIn}}} ===
  // This is a boolean that indicates whether the feed is to be treated
  // as a built-in feed. See the documentation for
  // {{{FeedManager#addSubscribedFeed()}}} for more
  // information. Read-only.
  get isBuiltIn()
    this._annSvc.pageHasAnnotation(this.uri, FEED_BUILTIN_ANNO),

  // === {{{Feed#isSubscribed}}} ===
  // Whether the feed is currently being subscribed to or not. Read-only.
  get isSubscribed()
    this._annSvc.pageHasAnnotation(this.uri, FEED_SUBSCRIBED_ANNO),

  // === {{{Feed#canAutoUpdate}}} ===
  // Whether or not the latest version of the feed's source code should
  // be fetched from the network. See
  // {{{FeedManager#addSubscribedFeed()}}} for more information. Read-only.
  get canAutoUpdate()
    this._annSvc.getPageAnnotation(
      this.uri, FEED_AUTOUPDATE_ANNO, "") === "true",

  // === {{{Feed#purge()}}} ===
  // Permanently deletes the feed.
  purge: function Feed_purge() {
    this._annSvc.removePageAnnotations(this.uri);
    this._hub.notifyListeners("purge", this.uri);
  },

  // === {{{Feed#remove()}}} ===
  // If the feed is currently being subscribed to, unsubscribes it.
  // This isn't permanent; the feed can be resubscribed later
  // with {{{Feed#unremove()}}}.
  remove: function Feed_remove() {
    var annSvc = this._annSvc;
    if (annSvc.pageHasAnnotation(this.uri, FEED_SUBSCRIBED_ANNO)) {
      annSvc.removePageAnnotation(this.uri, FEED_SUBSCRIBED_ANNO);
      annSvc.setPageAnnotation(this.uri, FEED_UNSUBSCRIBED_ANNO, "true", 0,
                               this._expiration);
      this._hub.notifyListeners("unsubscribe", this.uri);
    }
  },

  // === {{{Feed#unremove()}}} ===
  // If the feed is currently unsubscribed, re-subscribes it.
  unremove: function Feed_unremove() {
    var annSvc = this._annSvc;
    if (annSvc.pageHasAnnotation(this.uri, FEED_UNSUBSCRIBED_ANNO)) {
      annSvc.removePageAnnotation(this.uri, FEED_UNSUBSCRIBED_ANNO);
      annSvc.setPageAnnotation(this.uri, FEED_SUBSCRIBED_ANNO, "true", 0,
                               this._expiration);
      this._hub.notifyListeners("subscribe", this.uri);
    }
  },

  // === {{{Feed#getCode()}}} ===
  // Returns the cached source code for the feed, if any.
  getCode: function Feed_getCode()
    this._annSvc.getPageAnnotation(this.uri, FEED_SRC_ANNO, ""),

  // === {{{Feed#setCode(code)}}} ===
  // Sets the cached source code for the feed.
  setCode: function Feed_setCode(code) {
    this._annSvc.setPageAnnotation(this.uri, FEED_SRC_ANNO, code, 0,
                                   this._expiration);
  },

  // === {{{Feed#getJSONStorage()}}} ===
  // Gets the persistent JSON storage for the feed.
  getJSONStorage: function Feed_getJSONStorage()
    JSON.parse(
      this._annSvc.getPageAnnotation(this.uri, FEED_BIN_ANNO, "{}")),

  // === {{{Feed#setJSONStorage(object)}}} ===
  // Sets a JSON-encodable {{{object}}} as the persistent JSON storage
  // for the feed and returns the stored result as a new object.
  setJSONStorage: function Feed_setJSONStorage(obj) {
    var data = JSON.stringify(obj);
    this._annSvc.setPageAnnotation(this.uri, FEED_BIN_ANNO, data, 0,
                                   this._expiration);
    return JSON.parse(data);
  },

  // === {{{Feed#makeBin()}}} ===
  // Creates the feed's {{{Bin}}}.
  makeBin: function Feed_makeBin() ({
    __proto__: Bin,
    __parent__: this,
    __bin__: this.getJSONStorage(),
  }),

  // === {{{Feed#checkForManualUpdate()}}} ===
  // Checks to see whether an update for the feed is available; if it
  // is, then the given callback is called and passed {{{true}}} as an
  // argument. Otherwise, the given callback is called and passed
  // {{{false}}} as an argument.
  checkForManualUpdate: function Feed_checkForManualUpdate(cb) { cb(false) },

  // === {{{Feed#finalize()}}} ===
  // Performs any needed cleanup on the feed before it's destroyed.
  finalize: function Feed_finalize() {},
};

FMgrProto.__makeFeed = function FMgr___makeFeed(uri) {
  var feed = new Feed(uri, this._annSvc, this._hub);

  // == Subclassing Feed ==
  // The {{{Feed}}} object created by {{{FeedManager}}} instances is
  // only used as a base class; the appropriate feed plugin
  // dynamically subclasses it and adds more functionality when its
  // {{{makeFeed()}}} method is called. For an example of this, see
  // {{{LockedDownFeedPlugin}}}.
  var plugin = this._plugins[feed.type];
  if (!plugin)
    throw new Error("No feed plugin registered for type '" + feed.type + "'.");

  return plugin.makeFeed(feed, this._hub);
};

// == Bin ==
// A simple interface to access the feed's persistent JSON storage.
// {{{
// Bin.myKey("some value"); // set a value for the key
// let val = Bin.myKey();   // get the value for a key
// Bin.myKey(null);         // delete the key
// let num = +Bin;          // count keys
// for each (let [key, val] in Bin) ... // iterate over keys and values
// }}}

var Bin = {
  __proto__: null,

  // === {{{ Bin.__noSuchMethod__() }}} ===
  // {{{Bin}}} allows arbitrary keys
  // (except for the ones already in use, such as {{{"toString"}}})
  // to be called as methods for getting/setting/deleting their values.
  // Returns the value stored for the key.
  //
  // {{{value}}} is an optional JSON-encodable value to be set for the key.
  // If this equals to {{{null}}}, the key is deleted.
  __noSuchMethod__: function __noSuchMethod__(key, [val]) {
    var bin = this.__bin__;
    if (val === void 0) return bin[key];
    if (val === null) {
      var old = bin[key];
      delete bin[key];
    }
    else bin[key] = val;
    bin = this.__bin__ = this.__parent__.setJSONStorage(bin);
    return key in bin ? bin[key] : old;
  },

  // === {{{ Bin.__iterator__() }}} ===
  // Returns an {{{Iterator}}} for the inner dictionary.
  __iterator__: function __iterator__(wk) new Iterator(this.__bin__, wk),

  // === {{{ Bin.valueOf() }}} ===
  // Returns the number of keys currently stored.
  valueOf: function valueOf() Utils.count(this.__bin__),

  toJSON: function toJSON() this.__bin__,
  toString: function toString() "[object Bin]",
};
