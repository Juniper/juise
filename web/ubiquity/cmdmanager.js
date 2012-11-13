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

jQuery(function ($) {
var EXPORTED_SYMBOLS = ["CommandManager"];

// const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

// Cu.import("/ubiquity/modules/utils.js");
// Cu.import("/ubiquity/modules/setup.js");
// Cu.import("/ubiquity/modules/parser/parser.js");
// Cu.import("/ubiquity/modules/preview_browser.js");
// Cu.import("/ubiquity/modules/localization_utils.js");

var L = $.u.LocalizationUtils.propertySelector(
  "/ubiquity/chrome/locale/coreubiquity.properties");

const {prefs} = $.u.Utils;
const DEFAULT_PREVIEW_URL = "/ubiquity/preview.html";
const DEFAULT_MAX_SUGGESTIONS = 5;
const PREF_MAX_SUGGESTIONS = "extensions.ubiquity.maxSuggestions";
const MIN_MAX_SUGGS = 1;
const MAX_MAX_SUGGS = 42;
const {NS_XHTML, NS_XUL} = $.u.Utils;
const DEFAULT_HELP = (
  '<div class="default" xmlns="' + NS_XHTML + '">' +
  L("ubiquity.cmdmanager.defaulthelp") +
  '</div>');
const NULL_QUERY = {suggestionList: [], finished: true, hasResults: false};

var gDomNodes = {};

CommandManager.__defineGetter__(
  "maxSuggestions", function CM_getMaxSuggestions() {
    return prefs.getValue(PREF_MAX_SUGGESTIONS,
                          DEFAULT_MAX_SUGGESTIONS);
  });
CommandManager.__defineSetter__(
  "maxSuggestions", function CM_setMaxSuggestions(value) {
    var num = Math.max(MIN_MAX_SUGGS, Math.min(value | 0, MAX_MAX_SUGGS));
    prefs.setValue(PREF_MAX_SUGGESTIONS, num);
  });

function CommandManager(cmdSource, msgService, parser,
                        suggsNode, previewPaneNode, helpNode) {
  var suggDoc = suggsNode.getElementsByTagName("iframe")[0].contentDocument;

  this.__cmdSource = cmdSource;
  this.__msgService = msgService;
  this.__hilitedIndex = 0;
  this.__lastInput = "";
  this.__lastSuggestion = null;
  this.__queuedExecute = null;
  this.__lastAsyncSuggestionCb = Boolean;
  this.__nlParser = parser;
  this.__dynaLoad = !parser;
  this.__parserType = null;
  this.__activeQuery = NULL_QUERY;
  this.__domNodes = {
    suggs: suggsNode,
    suggsDocument: suggDoc,
    preview: previewPaneNode,
    help: helpNode,
  };
  this.__previewer = new PreviewBrowser(
    previewPaneNode.getElementsByTagNameNS(NS_XUL, "browser")[0],
    DEFAULT_PREVIEW_URL);

  var self = this;
  function onFeedsReloaded() { self._loadCommands() }
  cmdSource.addListener("feeds-reloaded", onFeedsReloaded);
  this.finalize = function CM_finalize() {
    cmdSource.removeListener("feeds-reloaded", onFeedsReloaded);
    self.__previewer.finalize();
    for (let key in new Iterator(this, true)) delete this[key];
  };

  if (parser) this._loadCommands();
  this.setPreviewState("no-suggestions");

  suggDoc.addEventListener("click", this, false);
  suggDoc.addEventListener("DOMMouseScroll", this, false);
}

CommandManager.prototype = {
  handleEvent: function CM_handleEvent(event) {
    switch (event.type) {
      case "click": {
        if (event.button === 2) return;
        let {target} = event;
        do {
          if (!("hasAttribute" in target)) return;
          if (target.hasAttribute("index")) break;
        } while ((target = target.parentNode));
        let index = +target.getAttribute("index");
        if (this.__hilitedIndex === index) return;
        this.__hilitedIndex = index;
        this.__lastAsyncSuggestionCb();
      } break;
      case "DOMMouseScroll": {
        this[event.detail < 0 ? "moveIndicationUp" : "moveIndicationDown"]();
        this.__lastAsyncSuggestionCb();
      } break;
      default: return;
    }
    event.preventDefault();
    event.stopPropagation();
  },

  setPreviewState: function CM_setPreviewState(state) {
    var {suggs, preview, help} = this.__domNodes;
    switch (state) {
      case "computing-suggestions":
      case "with-suggestions": {
        suggs.style.display = "block";
        preview.style.display = "block";
        help.style.display = "none";
        break;
      }
      case "no-suggestions": {
        suggs.style.display = "none";
        preview.style.display = "none";
        this._setHelp(help);
        help.style.display = "block";
        if (this.__previewer.isActive)
          this.__previewer.queuePreview(
            null,
            0,
            function clearPreview(pblock) { pblock.innerHTML = ""; });
        break;
      }
      default: throw new Error("Unknown state: " + state);
    }
  },

  refresh: function CM_refresh() {
    if (this.__dynaLoad) {
      let {parserVersion, languageCode} = UbiquitySetup;
      if ((this.__parserType) !==
          (this.__parserType = parserVersion + languageCode)) {
        if (this.__nlParser) {
          this.__nlParser = null;
          this.__cmdSource.refresh(true);
        }
        this.__nlParser = (
          NLParserMaker(parserVersion)
          .makeParserForLanguage(languageCode,
                                 this.__cmdSource.getAllCommands()));
        var refreshed = true;
      }
    }
    refreshed || this.__cmdSource.refresh();
    this.reset();
  },

  moveIndicationUp: function CM_moveIndicationUp(context) {
    if (--this.__hilitedIndex < 0)
      this.__hilitedIndex = this.__activeQuery.suggestionList.length - 1;
    if (context) this._renderAll(context);
  },

  moveIndicationDown: function CM_moveIndicationDown(context) {
    if (++this.__hilitedIndex >= this.__activeQuery.suggestionList.length)
      this.__hilitedIndex = 0;
    if (context) this._renderAll(context);
  },

  _loadCommands: function CM__loadCommands() {
    if (this.__nlParser)
      this.__nlParser.setCommandList(this.__cmdSource.getAllCommands());
  },

  _setHelp: function CM__setHelp(help) {
    var doc = help.ownerDocument;
    function createFragment(html) {
      var range = doc.createRange();
      var fragment = range.createContextualFragment(html);
      if (!fragment) {
        range.selectNode(help);
        fragment = range.createContextualFragment(html);
      }
      range.detach();
      return fragment;
    }
    if (!("defaultHelp" in gDomNodes)) {
      gDomNodes.defaultHelp = createFragment(DEFAULT_HELP).firstChild;
    }
    if (!("feedUpdates" in gDomNodes)) {
      gDomNodes.feedUpdates = doc.createElement("box");
      let {feedManager} = UbiquitySetup.createServices();
      let count = 0;
      feedManager.getSubscribedFeeds().forEach(
        function eachFeed(feed, i, feeds) {
          function accList(list, feed, i)
            list.appendChild(
              <li><a href={feed.url} accesskey={(i + 1).toString(36)}
              >{feed.title}</a></li>);
          feed.checkForManualUpdate(function check(updated, confirmUrl) {
            feeds[i] = updated && {title: feed.title, url: confirmUrl};
            if (++count === feeds.length &&
                (feeds = feeds.filter(Boolean)).length)
              gDomNodes.feedUpdates.appendChild(createFragment(
                <div class="feed-updates" xmlns={NS_XHTML}>
                <h3>The following feeds have updates:</h3>
                </div>.appendChild(feeds.reduce(accList, <ol/>))));
          });
        });
    }
    help.appendChild(gDomNodes.defaultHelp);
    help.appendChild(gDomNodes.feedUpdates);
  },

  _renderSuggestions: function CM__renderSuggestions() {
    var {escapeHtml} = Utils, content = "";
    var {__activeQuery: {suggestionList}, __hilitedIndex: hindex} = this;
    for (let i = 0, l = suggestionList.length; i < l; ++i) {
      let {displayHtml, icon} = suggestionList[i];
      content += (
        '<div class="suggested' + (i === hindex ? " hilited" : "") +
        '" index="' + i + '"><div class="cmdicon">' +
        (icon ? '<img src="' + escapeHtml(icon) + '"/>' : "") +
        "</div>" + displayHtml + "</div>");
    }
    this.__domNodes.suggsDocument.body.innerHTML = content;
  },

  _renderPreview: function CM__renderPreview(context) {
    var activeSugg = this.hilitedSuggestion;
    if (!activeSugg || activeSugg === this.__lastSuggestion) return;

    var self = this;
    this.__lastSuggestion = activeSugg;
    this.__previewer.queuePreview(
      activeSugg.previewUrl,
      activeSugg.previewDelay,
      function queuedPreview(pblock) {
        try { activeSugg.preview(context, pblock) } catch (e) {
          self.__msgService.displayMessage({
            text: ('An exception occurred while previewing the command "' +
                   activeSugg._verb.name + '".'),
            exception: e});
        }
      });
  },

  _renderAll: function CM__renderAll(context) {
    if ("chromeWindow" in context &&
        context.chromeWindow.gUbiquity.isWindowOpen) {
      this._renderSuggestions();
      this._renderPreview(context);
    }
  },

  reset: function CM_reset() {
    var query = this.__activeQuery;
    if (!query.finished) query.cancel();
    this.__activeQuery = NULL_QUERY;
    this.__hilitedIndex = 0;
    this.__lastInput = "";
    this.__lastSuggestion = null;
    this.__queuedExecute = null;
  },

  updateInput: function CM_updateInput(input, context, asyncSuggestionCb) {
    this.reset();
    this.__lastInput = input;

    var query = this.__activeQuery =
      this.__nlParser.newQuery(input, context, this.maxSuggestions, true);
    query.onResults = asyncSuggestionCb || this.__lastAsyncSuggestionCb;

    if (asyncSuggestionCb)
      this.__lastAsyncSuggestionCb = asyncSuggestionCb;

    query.run();
  },

  onSuggestionsUpdated: function CM_onSuggestionsUpdated(input, context) {
    if (input !== this.__lastInput) return;

    var {hilitedSuggestion} = this;
    if (this.__queuedExecute && hilitedSuggestion) {
      this.__queuedExecute(hilitedSuggestion);
      this.__queuedExecute = null;
    }

    this.setPreviewState(this.__activeQuery.finished
                         ? (hilitedSuggestion
                            ? "with-suggestions"
                            : "no-suggestions")
                         : "computing-suggestions");
    this._renderAll(context);
  },

  execute: function CM_execute(context) {
    function doExecute(activeSugg) {
      try {
        this.__nlParser.strengthenMemory(activeSugg);
        activeSugg.execute(context);
      } catch (e) {
        this.__msgService.displayMessage({
          text: ('An exception occurred while running the command "' +
                 activeSugg._verb.name + '".'),
          exception: e});
      }
    }
    var {hilitedSuggestion} = this;
    if (hilitedSuggestion)
      doExecute.call(this, hilitedSuggestion);
    else
      this.__queuedExecute = doExecute;
  },

  getSuggestionListNoInput:
  function CM_getSuggListNoInput(context, asyncSuggestionCb) {
    let noInputQuery = this.__nlParser.newQuery(
      "", context, 4 * CommandManager.maxSuggestions);
    noInputQuery.onResults = function onResultsNoInput() {
      asyncSuggestionCb(noInputQuery.suggestionList);
    };
  },

  makeCommandSuggester: function CM_makeCommandSuggester() {
    var self = this;
    return function getAvailableCommands(context, popupCb) {
      self.refresh();
      self.getSuggestionListNoInput(context, popupCb);
    };
  },

  remember: function CM_remember() {
    var {hilitedSuggestion} = this;
    if (hilitedSuggestion) this.__nlParser.strengthenMemory(hilitedSuggestion);
  },

  get parser() this.__nlParser,
  get lastInput() this.__lastInput,
  get previewer() this.__previewer,

  get maxSuggestions() CommandManager.maxSuggestions,
  get hasSuggestions() this.__activeQuery.hasResults,
  get suggestions() this.__activeQuery.suggestionList,
  get hilitedSuggestion()
    this.__activeQuery.suggestionList[this.__hilitedIndex],
  get hilitedIndex() this.__hilitedIndex,
  set hilitedIndex(i) this.__hilitedIndex = i,
};
});
