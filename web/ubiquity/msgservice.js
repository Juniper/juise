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

// = Message Services =
// {{{MessageService}}} is the name of an interface that provides a
// means for notifying the end-user of important events in a non-intrusive
// manner.
//
// An object that implements the {{{MessageService}}} interface must
// expose the following method:

// === {{{MessageService#displayMessage(msg)}}} ===
// Displays the given message. {{{msg}}} may be a simple string, but it
// can also be a JavaScript object with the following keys, all of them
// optional:
//
// {{{msg.title}}} is the title of the message.
//
// {{{msg.text}}} is the body of the message.
//
// {{{msg.icon}}} is a URL pointing to an icon for the message.
//
// {{{msg.exception}}} is an exception object corresponding to the
// exception that the message represents, if any.
//
// {{{msg.onclick}}} is a function called when the text is clicked.
//
// {{{msg.onfinished}}} is a function called when the alert goes away.

jQuery(function ($) {
var EXPORTED_SYMBOLS = ["ExceptionUtils",
                        "ErrorConsoleMessageService",
                        "AlertMessageService",
                        "CompositeMessageService"];

// const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

// Cu.import("/ubiquity/modules/utils.js");
// Cu.import("/ubiquity/modules/localization_utils.js");

const PREF_SHOW_ERROR = "extensions.ubiquity.displayAlertOnError";

var L = $.u.LocalizationUtils.propertySelector(
  "/ubiquity/chrome/locale/coreubiquity.properties");

// == Message Service Implementations ==

// === {{{ErrorConsoleMessageService}}} ===
// This {{{MessageService}}} logs messages containing exceptions to
// the JavaScript error console, also logging their stack traces if
// possible.

$.u.ErrorConsoleMessageService = function ErrorConsoleMessageService() {}
ErrorConsoleMessageService.prototype = {
  displayMessage: function ECMS_displayMessage(msg) {
    var {exception} = msg || 0;
    if (exception == null) return;

    Utils.reportError(exception);
    var tb = ExceptionUtils.stackTrace(exception);
    if (tb) Utils.reportError("Traceback for last exception:\n" + tb);
  }
};

// === {{{AlertMessageService}}} ===
// This {{{MessageService}}} uses {{{nsIAlertsService}}} to
// non-modally display the message to the user. On Windows, this shows
// up as a "toaster" notification at the bottom-right of the
// screen. On OS X, it's shown using
// [[http://en.wikipedia.org/wiki/Growl_%28software%29|Growl]].

$.u.AlertMessageService = function AlertMessageService() {}
AlertMessageService.prototype = {
  DEFAULT_ICON : "/ubiquity/chrome/skin/icons/favicon.ico",
  DEFAULT_TITLE: L("ubiquity.msgservice.defaultmsgtitle"),
  displayMessage: function AMS_displayMessage(msg) {
    var text  = "";
    var title = this.DEFAULT_TITLE;
    var icon  = this.DEFAULT_ICON;
    var textClickable = false;
    var cookie = "";
    var alertListener = null;
    var name = null;

    if (Utils.classOf(msg) !== "Object")
      text = String(msg);
    else {
      if ("text" in msg) text = String(msg.text);
      if ("exception" in msg) {
        if (!Utils.prefs.getValue(PREF_SHOW_ERROR, false)) return;
        text += " (" + msg.exception + ")";
      }
      if ("title" in msg) title = String(msg.title);
      if ("icon"  in msg) icon  = String(msg.icon);

      let {onclick, onfinished} = msg;
      if (onclick) textClickable = true;
      if (onclick || onfinished)
        alertListener = {
          observe: function AMS_observe(subject, topic, data) {
            if (topic === "alertclickcallback" && onclick)
              onclick();
            else if (topic === "alertfinished" && onfinished)
              onfinished();
          }
        };
    }
    try {
      Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService)
        .showAlertNotification(icon, title, text, textClickable,
                               cookie, alertListener, name);
    } catch (e) {
      Cu.reportError(e);
      Utils.focusUrlInBrowser("/ubiquity/chrome/content/bug19warning.xhtml");
    }
  }
};

// === {{{CompositeMessageService}}} ===
// Combines one or more {{{MessageService}}} implementations under a
// single {{{MessageService}}} interface.
//
// Use the {{{add()}}} method to add new implementations.

$.u.CompositeMessageService = function CompositeMessageService() {
  this._services = [];
}
CompositeMessageService.prototype = {
  add: function CMS_add(service) {
    this._services.push(service);
    return this;
  },

  displayMessage: function CMS_displayMessage(msg) {
    for each (let service in this._services)
      service.displayMessage(msg);
  }
};

// == Exception Utilities ==
// The {{{ExceptionUtils}}} namespace provides some functionality for
// introspecting JavaScript and XPCOM exceptions.

var ExceptionUtils;
$.u.ExceptionUtils = ExceptionUtils = {
  stackTraceFromFrame: function EU_stackTraceFromFrame(frame, formatter) {
    if (!formatter)
      formatter = function EU_defaultFormatter(frame) { return frame; };

    var output = "";

    while (frame) {
      output += formatter(frame) + "\n";
      frame = frame.caller;
    }

    return output;
  },

  stackTrace: function EU_stackTrace(e, formatter) {
    var output = "";
    if (e.location) {
      // It's a wrapped nsIException.
      output += this.stackTraceFromFrame(e.location, formatter);
    }
    else if (e.stack)
      // It's a standard JS exception.

      // TODO: It would be nice if we could parse this string and
      // create a 'fake' nsIStackFrame-like call stack out of it,
      // so that we can do things w/ this stack trace like we do
      // with nsIException traces.
      output += e.stack;
    else
      // It's some other thrown object, e.g. a bare string.
      ;

    return output;
  }
};

    $.u.ExceptionUtils = ExceptionUtils;
    $.u.ErrorConsoleMessageService = ErrorConsoleMessageService;
    $.u.AlertMessageService = AlertMessageService;
    $.u.CompositeMessageService = CompositeMessageService;

});
