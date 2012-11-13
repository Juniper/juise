/***** BEGIN LICENSE BLOCK *****
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
 *   Jono DiCarlo <jdicarlo@mozilla.com>
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

var EXPORTED_SYMBOLS = ["NLParser2", "parserRegistry"];

const Cu = Components.utils;

Cu.import("/ubiquity/modules/utils.js");
Cu.import("/ubiquity/modules/parser/new/parser.js");

var NLParser2 = {
  // Namespace object
  parserFactories: {},
  makeParserForLanguage: function(languageCode, verbList,
                                  ContextUtils, SuggestionMemory) {
    if (!(languageCode in NLParser2.parserFactories)) {
      try { loadParserMaker(languageCode) }
      catch (e if e.result === Components.results.NS_ERROR_FILE_NOT_FOUND) {
        throw Error("No parser is defined for " + uneval(languageCode));
      }
    }

    let parser = NLParser2.parserFactories[languageCode]();
    if (!Utils.isEmpty(verbList)) parser.setCommandList(verbList);
    /* If ContextUtils and/or SuggestionMemory were provided, they are
     * stub objects for testing purposes.  Set them on the new parser
     * object; it will use them instead of the real modules.
     * Normally I would do this in the constructor, but because we use
     * parserFactories[]() it's easier to do it here:
     */
    parser._contextUtils = (
      ContextUtils ||
      (Cu.import("/ubiquity/modules/contextutils.js", null)
       .ContextUtils));
    parser._suggestionMemory = (
      SuggestionMemory ||
      new (Cu.import("/ubiquity/modules/suggestion_memory.js", null)
           .SuggestionMemory)("main_parser"));

    return parser;
  }
};

var parserRegistry = eval(
  "0," +
  Utils.getLocalUrl(
    "/ubiquity/modules/parser/new/parser_registry.json",
    "utf-8"));

function loadParserMaker(code) {
  eval(Utils.getLocalUrl(
    "/ubiquity/modules/parser/new/" + code + ".js",
    "utf-8"));
  NLParser2.parserFactories[code] = makeParser;
}
