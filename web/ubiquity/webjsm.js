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

// = WebJsModule =
//
// The {{{WebJsModule}}} class is used when it's desirable to have JavaScript
// libraries that were originally intended for use on the web be
// accessible from JS Modules.
//
// Generally, this is done by using an always-existing DOM window to
// host the JS library in chrome space and provide direct access to it.
// This ensures that all standard globals the code expects to exist,
// such as {{{window}}} and {{{XMLHttpRequest}}}, actually exist.
//
// Note, however, that since the code is running in chrome space,
// care must be taken to ensure that any JS libraries loaded don't load
// remote code from untrusted sources.

var EXPORTED_SYMBOLS = ["WebJsModule"];

const Cc = Components.classes;
const Ci = Components.interfaces;

// == The WebJsModule Class ==
//
// The constructor requires a callback function, which is called when the
// {{{WebJsModule}}} is finished initializing itself. Optionally, the
// constructor can also take an instance of a DOM window, which it will
// use to host any JS code. If unspecified, the hidden DOM window is
// used.

function WebJsModule(callback, window) {
  if (!window)
    window = Cc["@mozilla.org/appshell/appShellService;1"]
             .getService(Ci.nsIAppShellService)
             .hiddenDOMWindow;

  var importedScripts = {};

  var self = this;
  var iframe = window.document.createElement("iframe");
  iframe.setAttribute("src", "/ubiquity/chrome/content/hiddenframe.html");
  iframe.addEventListener(
    "pageshow",
    function WJM__onPageShow() {
      iframe.removeEventListener("pageshow", arguments.callee, false);

      // Have our instance dynamically inherit the properties of the
      // hidden window.
      self.__proto__ = iframe.contentWindow;
      callback();
    },
    false
  );
  window.document.documentElement.appendChild(iframe);

  // === {{{WebJsModule#importScript()}}} ===
  //
  // This method is passed a URL that specifies the content-space JS
  // library to load. If the library is already loaded, this method
  // does nothing.
  //
  // Once the script is imported, any globals it created can be
  // directly accessed as properties of the {{{WebJsModule}}} instance.

  this.importScript = function WJM_importScript(url) {
    if (url in importedScripts) return;
    var doc = iframe.contentDocument, script = doc.createElement('script');
    script.setAttribute('src', url);
    doc.documentElement.appendChild(script);
    doc.documentElement.removeChild(script);
    importedScripts[url] = true;
  };
}
