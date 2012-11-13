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

// = NounUtils =
//
// A library of noun related utilities.
// {{{CmdUtils}}} inherits them all.

jQuery(function ($) {
var EXPORTED_SYMBOLS = ["NounUtils"];

var NounUtils = {};

for each (let f in this) if (typeof f === "function") NounUtils[f.name] = f;
delete NounUtils.QueryInterface;

Components.utils.import("/ubiquity/modules/utils.js");

// === {{{ NounUtils.NounType(label, expected, defaults) }}} ===
//
// Constructor of a noun type that accepts a specific set of inputs.
// See {{{NounType._from*}}} methods for details
// (but do not use them directly).
//
// {{{label}}} is an optional string specifying default label of the nountype.
//
// {{{expected}}} is the instance of {{{Array}}}, {{{Object}}} or {{{RegExp}}}.
// The array can optionally be a space-separeted string.
//
// {{{defaults}}} is an optional array or space-separated string
// of default inputs.

function NounType(label, expected, defaults) {
  if (!(this instanceof NounType))
    return new NounType(label, expected, defaults);

  if (typeof label !== "string")
    [label, expected, defaults] = ["?", label, expected];

  if (typeof expected.suggest === "function") return expected;

  function maybe_qw(o) typeof o === "string" ? o.match(/\S+/g) || [] : o;
  expected = maybe_qw(expected);
  defaults = maybe_qw(defaults);

  var maker = NounType["_from" + Utils.classOf(expected)];
  for (let [k, v] in new Iterator(maker(expected))) this[k] = v;
  this.suggest = maker.suggest;
  this.label = label;
  this.noExternalCalls = true;
  this.cacheTime = -1;
  if (this.id) this.id += Utils.computeCryptoHash("MD5", (uneval(expected) +
                                                          uneval(defaults)));
  if (defaults) {
    // [[a], [b, c], ...] => [a].concat([b, c], ...) => [a, b, c, ...]
    this.default =
      Array.concat.apply(0, [this.suggest(d) for each (d in defaults)]);
  }
}

// ** {{{ NounUtils.NounType._fromArray(words) }}} **
//
// Creates a noun type that accepts a finite list of specific words
// as the only valid inputs. Those words will be suggested as {{{text}}}s.
//
// {{{words}}} is the array of words.

NounType._fromArray = function NT_Array(words)({
  id: "#na_",
  name: words.slice(0, 2) + (words.length > 2 ? ",..." : ""),
  _list: [makeSugg(w) for each (w in words)],
});

// ** {{{ NounUtils.NounType._fromObject(dict) }}} **
//
// Creates a noun type from the given key:value pairs, the key being
// the {{{text}}} attribute of its suggest and the value {{{data}}}.
//
// {{{dict}}} is the object of text:data pairs.

NounType._fromObject = function NT_Object(dict) {
  var list = [makeSugg(key, null, dict[key]) for (key in dict)];
  return {
    name: ([s.text for each (s in list.slice(0, 2))] +
           (list.length > 2 ? ",..." : "")),
    _list: list,
  };
};

NounType._fromArray.suggest = NounType._fromObject.suggest = (
  function NT_suggest(text) grepSuggs(text, this._list));

// ** {{{ NounUtils.NounType._fromRegExp(regexp) }}} **
//
// Creates a noun type from the given regular expression object
// and returns it. The {{{data}}} attribute of the noun type is
// the {{{match}}} object resulting from the regular expression
// match.
//
// {{{regexp}}} is the RegExp object that checks inputs.

NounType._fromRegExp = function NT_RegExp(regexp) ({
  id: "#nr_",
  name: regexp + "",
  rankLast: regexp.test(""),
  _regexp: RegExp(
    regexp.source,
    [ "g"[regexp.global     - 1]
    , "i"[regexp.ignoreCase - 1]
    , "m"[regexp.multiline  - 1]
    , "y"[regexp.sticky     - 1]
    ].join('')),
});
NounType._fromRegExp.suggest = function NT_RE_suggest(text, html, cb,
                                                      selectionIndices) {
  var match = text.match(this._regexp);
  if (!match) return [];
  // ToDo: how to score global match
  var score = "index" in match ? matchScore(match) : 1;
  return [makeSugg(text, html, match, score, selectionIndices)];
};

// === {{{ NounUtils.matchScore(match) }}} ===
//
// Calculates the score for use in suggestions from
// a result array ({{{match}}}) of {{{RegExp#exec}}}.

const SCORE_BASE = 0.3;
const SCORE_LENGTH = 0.25;
const SCORE_INDEX = 1 - SCORE_BASE - SCORE_LENGTH;

function matchScore(match) {
  var inLen = match.input.length;
  return (SCORE_BASE +
          SCORE_LENGTH * Math.sqrt(match[0].length / inLen) +
          SCORE_INDEX  * (1 - match.index / inLen));
}

// === {{{NounUtils.makeSugg(text, html, data, score, selectionIndices)}}} ===
//
// Creates a suggestion object, filling in {{{text}}} and {{{html}}} if missing
// and constructing {{{summary}}} from {{{text}}} and {{{selectionIndices}}}.
// At least one of {{{text}}}, {{{html}}} or {{{data}}} is required.
//
// {{{text}}} can be any string.
//
// {{{html}}} must be a valid HTML string.
//
// {{{data}}} can be any value.
//
// {{{score}}} is an optional float number representing
// the score of the suggestion. Defaults to {{{1.0}}}.
//
// {{{selectionIndices}}} is an optional array containing the start and end
// indices of selection within {{{text}}}.

function makeSugg(text, html, data, score, selectionIndices) {
  if (text == null && html == null && arguments.length < 3)
    // all inputs empty!  There is no suggestion to be made.
    return null;

  // Shift the argument if appropriate:
  if (typeof score === "object") {
    selectionIndices = score;
    score = null;
  }

  // Fill in missing fields however we can:
  if (text != null) text += "";
  if (html != null) html += "";
  if (!text && data != null)
    text = data.toString();
  if (!html && text >= "")
    html = Utils.escapeHtml(text);
  if (!text && html >= "")
    text = html.replace(/<[^>]*>/g, "");

  // Create a summary of the text:
  var snippetLength = 35;
  var summary = (text.length > snippetLength
                 ? text.slice(0, snippetLength - 1) + "\u2026"
                 : text);

  // If the input comes all or in part from a text selection,
  // we'll stick some html tags into the summary so that the part
  // that comes from the text selection can be visually marked in
  // the suggestion list.
  var [start, end] = selectionIndices || 0;
  summary = (
    start < end
    ? (Utils.escapeHtml(summary.slice(0, start)) +
       "<span class='selection'>" +
       Utils.escapeHtml(summary.slice(start, end)) +
       "</span>" +
       Utils.escapeHtml(summary.slice(end)))
    : Utils.escapeHtml(summary));

  return {
    text: text, html: html, data: data,
    summary: summary, score: score || 1};
}

// === {{{ NounUtils.grepSuggs(input, suggs, key) }}} ===
//
// A helper function to grep a list of suggestion objects by user input.
// Returns an array of filtered suggetions, each of them assigned {{{score}}}
// calculated by {{{NounUtils.matchScore()}}}.
//
// {{{input}}} is a string that filters the list.
//
// {{{suggs}}} is an array or dictionary of suggestion objects.
//
// {{{key}}} is an optional string to specify the target property
// to match with. Defaults to {{{"text"}}}.

function grepSuggs(input, suggs, key) {
  if (!input) return [];
  if (key == null) key = "text";
  var re = Utils.regexp(input, "i"), match;
  return ([(sugg.score = matchScore(match), sugg)
           for each (sugg in suggs) if ((match = re.exec(sugg[key])))]
          .sort(byScoreDescending));
}

function byScoreDescending(a, b) b.score - a.score;

// === {{{ NounUtils.mixNouns(label, nouns) }}} ===
//
// Creates a noun by combining two or more nouns.
//
// {{{label}}} is an optional string specifying the created noun's label.
//
// {{{nouns}}} is the array of nouns.

function mixNouns(label) {
  var gotLabel = typeof label === "string";
  var nouns = gotLabel ? arguments[1] : label;
  if (!Utils.isArray(nouns))
    nouns = Array.slice(arguments, gotLabel ? 1 : 0);
  function mixer(key) function suggestMixed() {
    var val, suggsList = [
      typeof val === "function" ? val.apply(noun, arguments) : val
      for each (noun in nouns) if ((val = noun[key])) ];
    return suggsList.concat.apply([], suggsList); // flatten
  };
  return {
    label: gotLabel ? label : [n.label || "?" for each (n in nouns)].join("|"),
    rankLast: nouns.some(function (n) n.rankLast),
    noExternalCalls: nouns.every(function (n) n.noExternalCalls),
    suggest: mixer("suggest"),
    default: nouns.some(function (n) "default" in n) && mixer("default"),
  };
}

    $.u.NounUtils = NounUtils;

});
