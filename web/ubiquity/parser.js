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
 *   Michael Yoshitaka Erlewine <mitcho@mitcho.com>
 *   Jono DiCarlo <jdicarlo@mozilla.com>
 *   Blair McBride <unfocused@gmail.com>
 *   Satoshi Murakami <murky.satyr@gmail.com>
 *   Brandon Pung <brandonpung@gmail.com>
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

var EXPORTED_SYMBOLS = ["Parser"];

const Cu = Components.utils;

// This turns on a number of checks which disallow parses with
// multiple arguments in the same role.
// This speeds things up considerably for longer parses.
const DONT_PARSE_MULTIPLE_ARGS_PER_ROLE = true;

Cu.import("resource://ubiquity/modules/utils.js");
Cu.import("resource://ubiquity/modules/msgservice.js");

var gOldAlerted = false;

// = Ubiquity Parser: The Next Generation =
//
// This file, {{{parser.js}}}, is part of the implementation of Ubiquity's
// new parser design,
// [[https://wiki.mozilla.org/Labs/Ubiquity/Parser_2]].
//
// In this file, we will set up three different classes:
// * {{{Parser}}}: each language parser will be an instance of this class
// * {{{ParseQuery}}}: this is a parser query class, as described in
//   [[http://ubiquity.mozilla.com/trac/ticket/532|trac #532]]
// * {{{Parse}}}: parses constructed and returned by parser queries
//   are of this class.

// == {{{Parser}}} ==
//
// {{{Parser}}} object initialization takes place in each individual language
// file--this, in turn, is controlled by a {{{makeXxParser}}} factory function;
// take a look at en.js for an example.

function Parser(props) {
  if (typeof props === "string")
    this.lang = props;
  else
    for (var key in props) this[key] = props[key];
}
Parser.prototype = {
  // References to contextUtils and suggestionMemory modules; makeParserForLanguage()
  // in namespace.js will, and must, set these to either a stub for testing, or to the
  // real module.
  _contextUtils: null,
  _suggestionMemory: null,

  // ** {{{Parser#lang}}} **
  lang: "",

  // ** {{{Parser#branching}}} **
  //
  // The "branching" parameter refers to which direction arguments are found.
  // For example, English is a "right-braching" language. This is because the
  // noun phrases in arguments come //after// (or "right of") the delimiter
  // (in English, prepositions). It's called "branching" by analogy to a
  // tree: if at a particular node, most of the "content" goes to the right,
  // it's "right-branching". See also
  // [[http://en.wikipedia.org/wiki/Left-branching|branching on wikipedia]].
  branching: "left" || "right",
  usespaces: true,

  // ** {{{Parser#joindelimiter}}} **
  //
  // The {{{joindelimiter}}} parameter is the delimiter that gets inserted
  // when gluing arguments and their delimiters back together in display.
  // In the case of most languages, the space (' ') is fine.
  // See how it's used in {{{Parse.displayText}}} and
  // {{{Parse.displayHtml}}}.
  //
  // TODO: {{{joindelimiter}}} and {{{usespaces}}} may or may not be
  // redundant.
  joindelimiter: " ",
  // ** {{{Parser#suggestedVerbOrder}}} **
  // For verb-final languages, this should be -1.
  // For languages where some verb forms are sentence-initial, some are
  // sentence-final (e.g. German, Dutch), suggestedVerbOrder can be a function
  // which takes the verb name and returns either 0 or -1.
  suggestedVerbOrder: 0,
  verbFinalMultiplier: 1,
  verbInitialMultiplier: 1,
  examples: [],
  clitics: [],
  anaphora: ["this"],
  doNounFirstExternals: false,

  // ** {{{Parser#roles}}} **
  //
  // a list of semantic roles and their delimiters
  roles: [{role: "object", delimiter: ""}],

  // ** {{{Parser#_rolesCache}}} **
  //
  // The {{{_rolesCache}}} is a cache of the different subsets of the
  // parser's roles (semantic roles and their associated delimiters)
  // are available for each verb. For example, {{{_rolesCache.add}}} will
  // give you the subset of semantic roles which are appropriate for the
  // {{{add}}} verb.
  _rolesCache: {},

  // ** {{{Parser#_roleSignatures}}} **
  //
  // The {{{_roleSignatures}}} is a hash to keep track of different role
  _roleSignatures: {},

  // ** {{{Parser#_objectDelimiter}}} **
  //
  // This is a value set on initialization if the language has
  // a delimiter set for the object role besides "". If there is, objects
  // which *don't* have a modifier will be lowered in score.
  //
  // If set, the value is the default non-"" delimiter.
  //
  // Example: think Japanese.
  _objectDelimiter: false,

  // ** {{{Parser#_patternCache}}} **
  //
  // The {{{_patternCache}}} keeps various regular expressions for use by the
  // parser. Most are created by {{{Parser#initializeCache()}}}, which is called
  // during parser creation. This way, commonly used regular expressions
  // need only be constructed once.
  _patternCache: {},

  // ** {{{Parser#_otherRolesCache}}} **
  //
  // {{{_otherRolesCache}}} is simply a subset of {{{Parser#roles}}} whose roles
  // are not "object" nor have a blank delimiter (currently unsupported).
  // It is used later by {{{Parser#applyObjectsToOtherRoles}}}.
  _otherRolesCache: [],

  // ** {{{Parser#_nounCache}}} **
  //
  // Perhaps this should be moved out into its own module in the future.
  // Nouns are cached in this cache in the {{{Parser#detectNounTypes}}} method
  // and associated methods. Later in the parse process elements of {{{_nounCache}}}
  // are accessed directly, assuming that the nouns were already cached
  // using {{{Parser#detectNounType}}}.
  _nounCache: null,
  _defaultsCache: null,

  _verbList: null,
  _nounTypes: null,
  _nounTypeIdsWithNoExternalCalls: null,

  // ** {{{Parser#setCommandList()}}} **
  //
  // {{{setCommandList}}} takes the command list and filters it,
  // only registering those which have the property {{{.names}}}.
  // This is in order to filter out verbs which have not been made to
  // work with Parser 2.
  //
  // This function also now parses out all the nountypes used by each verb.
  // The nountypes registered go in the {{{Parser#_nounTypes}}} object, which
  // are used for nountype detection as well as the comparison later with the
  // nountypes specified in the verbs for argument suggestion and scoring.
  //
  // After the nountypes have been registered, {{{Parser#initializeCache()}}} is
  // called.
  setCommandList: function setCommandList(commandList) {
    // First we'll register the verbs themselves.
    var verbs = this._verbList = [];

    var skippedSomeVerbs = false;
    for each (let verb in commandList) {
      if (verb.oldAPI) skippedSomeVerbs = true;
      else verbs.push(verb);
    }

    if (skippedSomeVerbs && !gOldAlerted) {
      var msgService = new AlertMessageService();
      msgService.displayMessage("Some verbs were not loaded " +
                                "as they are not compatible with Parser 2.");
      gOldAlerted = true;
    }
    
    this.refreshCommandList();
  },
  
  addCommandToList: function addCommandToList(command) {
    if (command.oldAPI && !gOldAlerted) {
      var msgService = new AlertMessageService();
      msgService.displayMessage("A verb was not loaded " +
                                "as is is not compatible with Parser 2.");
      gOldAlerted = true;
    }
  },
  
  refreshCommandList: function addCommandToList() {

    var verbs = this._verbList;

    // Scrape the noun types up here.
    var nouns = this._nounTypes = {};
    var localNounIds = this._nounTypeIdsWithNoExternalCalls = {};
    var nounIdsForGivenInputs = {};
    this.doNounFirstExternals = Utils.prefs.getValue(
      "extensions.ubiquity.doNounFirstExternals", 0);
    for each (let verb in verbs) {
      for each (let arg in verb.arguments) {
        let nt = arg.nountype;
        let {id} = nt;
        nouns[id] = nt;
        if (nt.noExternalCalls)
          localNounIds[id] = true;
        let {input} = arg;
        if (input) ((nounIdsForGivenInputs[input]) ||
                    (nounIdsForGivenInputs[input] = {}))[id] = true;
      }
    }
    this.extraArgsToDetect = [
      {argText: input, nounTypeIds: nounIdsForGivenInputs[input]}
      for (input in nounIdsForGivenInputs)];

    for each (let nt in this._nounTypes) {
      if ('registerCacheObserver' in nt) {
        var id = nt.id;
        var thisParser = this;
        var flush = function customFlushFunction() {
          thisParser.flushNounCacheForId(id);
        }
        nt.registerCacheObserver(flush);
      }
    }

    this.initializeCache();
    // run language-specific setup code
    this.initializeLanguage();
  },

  // ** {{{Parser#initializeCache()}}} **
  //
  // This method is initialized when the language is loaded.
  // Caches a number of commonly used regex's into {{{this._patternCache}}}.
  initializeCache: function initializeCache() {
    this._nounCache = new NounCache(this);
    this._defaultsCache = {};
    var patternCache = this._patternCache = {};
    var verbPatterns = patternCache.verbs = {};
    var delimPatterns = patternCache.delimiters = {};
    // cache the roles used in each verb and a regex
    // which recognizes the delimiters appropriate for each verb
    var rolesCache = this._rolesCache = {};

    // pick each "other role" and its first delimiter.
    var otherRolesCache = this._otherRolesCache = {};
    for each (let {role, delimiter} in this.roles)
      // if there is another role besides object which doesn't
      // require a modifier (currently unsupported) this arg will
      // already have a parse where it is this role, so don't try to
      // make the object this role.
      if (role !== "object" && !(role in otherRolesCache) && delimiter)
        otherRolesCache[role] = delimiter;

    for each (let {role, delimiter} in this.roles) {
      if (role == 'object' && delimiter != '') {
        this._objectDelimiter = delimiter;
        break;
      }
    }

    var verbs = this._verbList;

    var roleSignatures = this._roleSignatures = {};
    for each (let verb in verbs) {
      let subsets = Utils.powerSet([arg.role
                                    for each (arg in verb.arguments)]);
      for each (let subset in subsets) {
        if (subset.length) {
          roleSignatures[subset.sort().join()] = true;
        }
      }
    }

    var RegexpTrie = Utils.regexp.Trie;
    // creates a regex that matches any delimiter of given roles
    function regexFromDelimeters(roles)
      RegExp("^" + RegexpTrie([role.delimiter for each (role in roles)]) + "$",
             "i");
    // this is the RegExp to recognize delimiters for an as yet unspecified
    // verb... in other words, it's just a RegExp to recognize every
    // possible delimiter.
    delimPatterns[""] = regexFromDelimeters(this.roles);

    var wordSep = /[-_\s](?!$)/g;
    var trieSubnames = RegexpTrie();
    for each (let verb in verbs) {
      // ["cogit ergo sum", "thought being"]
      // => ["cogit ergo sum", "thought being", "ergo sum", "being", "sum"]
      let subnames = [], {names} = verb;
      for (let i = 0, l = names.length; i < l; ++i) {
        let name = names[i];
        wordSep.lastIndex = 0;
        do {
          let {lastIndex} = wordSep;
          let subname = name.slice(lastIndex);
          trieSubnames.addPrefixes(subname);
          let snlc = new String(subname.toLowerCase());
          snlc.name = name;
          subnames[i + l * lastIndex] = snlc;
        } while (wordSep.test(name));
      }
      verbPatterns[verb.id] = subnames.filter(Boolean); // compact
      // _rolesCache[verb.id] is the subset of roles such that
      // there is at least one argument in verb which matches that role
      rolesCache[verb.id] =
        [role for each (role in this.roles)
         if (verb.arguments.some(function(arg) arg.role === role.role))];
      delimPatterns[verb.id] = regexFromDelimeters(rolesCache[verb.id]);
    }
    var verbMatcher = trieSubnames.toString();
    // verbInitialTest matches a verb at the beginning
    patternCache.verbInitialTest =
      RegExp(("^\\s*(" + verbMatcher + ")" +
              (this.usespaces ? "(\\s+.*$|$)" : "(.*$)")),
             "i");
    // verbFinalTest matches a verb at the end of the string
    patternCache.verbFinalTest =
      RegExp(((this.usespaces ? "(^.*?\\s+|^)" : "(^.*?)") +
              "(" + verbMatcher + ")\\s*$"),
             "i");

    // anaphora matches any of the anaphora ("magic words")
    // if usespaces = true, it will only look for anaphora as whole words,
    // but if usespaces = false, it will look for anaphora in words as well.
    var boundary = this.usespaces ? "\\b" : "";
    patternCache.anaphora =
      RegExp(boundary + RegexpTrie(this.anaphora) + boundary);
  },

  flushNounCache: function Parser_flushNounCache() {
    this._nounCache = new NounCache(this);
  },

  flushNounCacheForId: function Parser_flushNounCacheForId(id) {
    this._nounCache.clearSuggsForId(id)
  },

  // ** {{{Parser#initializeLanguage}}} **
  //
  // Run custom language-specific setup code here. This is called at the
  // end of initializeCache. See ja for an example of its use.

  initializeLanguage: function() {
  },

  // ** {{{Parser#newQuery()}}} **
  //
  // This method returns a new {{{ParseQuery}}} object, as detailed in
  // [[http://ubiquity.mozilla.com/trac/ticket/532|trac #532]]
  newQuery: function Parser_newQuery(queryString, context, maxSuggestions,
                                     dontRunImmediately) {
    var selObj = this._contextUtils.getSelectionObject(context);
    if (!selObj.text)
      selObj.text = "";
    var theNewQuery = new ParseQuery(this,
                                     queryString,
                                     selObj,
                                     context,
                                     maxSuggestions,
                                     dontRunImmediately);
    return theNewQuery;
  },

  // ** {{{Parser#wordBreaker()}}} **
  //
  // Takes an input string and returns a string with some words split up
  // by default only spaces in the input are presevered, but this method
  // can be overridden by individual language parses to deal with languages
  // with no spaces and languages with strong case marking.
  //
  // see the Japanese {{{ja.wordBreaker()}}} for an example.
  wordBreaker: function(input) {
    return input;
  },

  // ** {{{Parser#verbFinder()}}} **
  //
  // Takes an input string and a selection and returns an array of objects in
  // the form of {{{{_verb:..., argString:..., sel:....}}}}.
  // sel is a string of the text that is currently selected on the page.
  // If no text is selected, sel is an empty string.
  // It will return at least one possible pair, the trivial pair, which is
  // {{{{_verb: null, argString: input, sel: selection}}}}.
  //
  // The verb in {{{_verb}}} is actually a copy of the verb object with
  // some useful parse-specific additions:
  // * {{{_verb.id}}} is the verb's canonical name
  // * {{{_verb.text}}} is the text in the input that was matched
  // * {{{_verb._order}}} is a reference to where in the input string the verb
  //   was found: 0 if it was sentence-initial, -1 if sentence-final
  // {{{argString}}} is a string with the rest of the input.
  verbFinder: function(input, selection) {
    // initialize the returnArray with the trivial pair.
    var returnArray = [{
      _verb: {
        id: "",
        text: null,
        _order: null,
        input: null,
      },
      argString: Utils.trim(input),
      sel: selection
    }];

    var suggMem = this._suggestionMemory;
    function boostVerbScoreWithFrequency(score, inputPart, verbId) {
      // reinforcement starts at 1 because x^(1/1) = x^1 = x.
      var reinforcement = suggMem.getScore(inputPart, verbId) + 1;
      // We return the n-th root of the verb score, where n = reinforcement.
      // This is good as the score is originally in [0,1], so the return value
      // will stay in [0,1].
      return Math.pow(score, 1 / reinforcement);
    }

    // The match will only give us the prefix that it matched. For example,
    // if we have a verb "shoot" and had input "sho Fred", verbPrefix = "sho"
    // and now we must figure out which verb that corresponded to.
    // Keep in mind there may be multiple verbs which match the verbPrefix
    // that matched.
    //
    // TODO: write a unit test for this possibility.
    var verbs = this._verbList;
    var verbPatterns = this._patternCache.verbs;
    var {verbFinalMultiplier, verbInitialMultiplier} = this;
    function addParses(verbPiece, argString, order) {
      var vplc = verbPiece.toLowerCase();
      for each (let verb in verbs) {
        if (verb.disabled) continue;
        for each (let subname in verbPatterns[verb.id]) {
          if (subname.indexOf(vplc) !== 0) continue;
          let {name} = subname;
          // Score the quality of the verb match.
          // the sqrt makes it so the score reflects the fact that
          // initial letters in the verb prefix are more informative,
          // and that later letters add less to the overall confidence
          // of the verb match. The 0.3 flooring was added so that these
          // verb prefix matches, even if they're only one or two
          // characters, will get higher scoreMultipliers than noun-first
          // suggestions, which get scoreMultiplier of 0.3. (trac #750)
          let verbScore = (0.4 + 0.6 * Math.sqrt(verbPiece.length / name.length))
                          * (order ? verbFinalMultiplier : verbInitialMultiplier);
          verbScore = boostVerbScoreWithFrequency(verbScore, verbPiece, verb.id);

          returnArray.push({
            _verb: {
              __proto__: verb,
              text: name,
              _order: order,
              input: verbPiece,
              score: verbScore
            },
            argString: argString,
            sel: selection
          });
          break;
        }
      }
    }

    // We'll keep the initial match with no args, which we will rule out
    // in the final match.
    var verbOnlyMatch;
    // let's see if there's a verb at the beginning of the string
    var initialMatches = input.match(this._patternCache.verbInitialTest);
    if (initialMatches) {
      let [, verbPiece, argString] = initialMatches;
      let position = 0;
      if (/^\s*$/.test(argString)) {
        // this was a verb-only match which was interpreted as verb-initial
        verbOnlyMatch = verbPiece;
        // if we prefer verb-final parses to verb-initial ones...
        if (verbFinalMultiplier > verbInitialMultiplier)
          // add this verb-only parse as verb-final.
          position = -1;
      }
      addParses(verbPiece, argString, position);
    }

    // let's see if there's a verb at the end of the string
    var finalMatches = input.match(this._patternCache.verbFinalTest);
    if (finalMatches) {
      let [, argString, verbPiece] = finalMatches;
      if (argString || verbOnlyMatch !== verbPiece)
        // we didn't already see this prefix
        // as a sentence-initial verb-only match
        addParses(verbPiece, argString, -1);
    }

    return returnArray;
  },

  // ** {{{Parser#splitWords()}}} **
  //
  // Takes an input string and returns an object with the words and their
  // delimiters. Words are returned in left to right order, split using
  // \s and \u200b (the no-width space) as delimiters.
  //
  // The return object is of the form {{{{ words: [], delimiters: [], all: [] }}}} .
  // {{{words}}} and {{{delimiters}}} are just a copy of every other word in
  // {{{allWords}}}.
  //
  // Used by {{{Parser#argFinder()}}}
  splitWords: function(input) {
    var returnObj = { words: [], delimiters: [], allWords: [],
                      beforeSpace: "", afterSpace: "" };

    // if there is no non-space character, just return nothing.
    // (note that \S doesn't include \u200b)
    if (!/\S/.test(input))
      return returnObj;

    // take all whitespace that is not of the very special no-width variety
    // (\u200b) nor a East Asian full-width space (\u3000) and
    // replace them with regular spaces.
    input = input.replace(/[^\S\u200b\u3000]/g, ' ');

    // this regexp with the () in it matches words but also non-words
    // (delimiters). The even numbered elements will be words and the odd
    // numbered ones are delimiters.
    let splitWithWords = input.split(/(\S+)/);
    returnObj.afterSpace  = splitWithWords.pop();
    returnObj.beforeSpace = splitWithWords.shift();
    returnObj.allWords    = splitWithWords;

    for (let i in splitWithWords) {
      if (i % 2)
        returnObj.delimiters.push(splitWithWords[i]);
      else
        returnObj.words.push(splitWithWords[i]);
    }
    return returnObj;
  },

  // ** {{{Parser#hasDelimiter()}}} **
  //
  // Checks to see whether a certain delimiter is compatible with a certain
  // verb, i.e., whether that verb has a role which takes that delimiter.
  // This is done using the regex of delimiters of all roles of {{{verb}}} in
  // {{{_patternCache.delimiters[verbId]}}}. Returns true/false.
  //
  // Used by {{{Parser#argFinder()}}}
  hasDelimiter: function(delimiter, verbId) {
    return this._patternCache.delimiters[verbId].test(delimiter);
  },

  // ** {{{Parser#getRoleByDelimiter()}}} **
  //
  // Returns all semantic roles which may be represented by a given delimiter.
  //
  // Used by {{{Parser#argFinder()}}}
  getRoleByDelimiter: function(delimiter, roles) {
    delimiter = delimiter.toLowerCase();
    return [role.role for each (role in roles)
                      if (role.delimiter.toLowerCase() == delimiter) ];
  },

  // ** {{{Parser#argFinder()}}} **
  //
  // {{{argFinder()}}} takes a {{{{_verb:..., argString:..., sel:....}}}}
  // object (ie. each object of outputs from {{{verbFinder()}}}
  // and attempts to find all of the delimiters in the {{{argString}}}
  // and then find different parse combinations of arguments
  // in different roles. It also interpolates the selection in sel into
  // all possible roles for each parse being constructed. It returns an
  // array of {{{Parse}}} objects with its arguments and verb set.
  //
  // If the {{{verb}}} argument is set, it will only look for delimiters which
  // are appropriate for the given verb (using {{{_patternCache.delimiters}}}),
  // speeding things up a little bit.
  //
  // {{{argFinder()}}} is where the {{{branching}}} parameter is incredibly
  // important. It also uses {{{Parser#splitWords()}}} to split the words up
  // in the beginning.
  //
  // ** //A high level overview of {{{Parser#argFinder()}}}// **
  //
  // Since {{{Parser#argFinder()}}} is arguably the //meat// of the parser,
  // here's a high level overview of the code in this method.
  //
  // First take a look at an example of what is being done here:
  // [[https://wiki.mozilla.org/Labs/Ubiquity/Parser_2#step_4:_group_into_arguments]].
  //
  // {{{argFinder}}} first finds all the indices of the words in the string
  // which look like a possible delimiter ({{{possibleDelimiterIndices}}}). It
  // then creates every possible combination of these positions which are
  // delimiters using a power set.
  //
  // For each of these possible delimiter positions ({{{delimiterIndices}}}),
  // it does the following:
  //
  // # If there are extra words at the beginning or end which won't be a target
  //    of any of the delimiters, throw it in the {{{object}}} role.
  // # Look at each delimiter position in {{{delimiterIndices}}}. Pick every
  //    possible substring (along word boundaries) of the argument to the
  //    delimiter's left (if left-branching) or right (if right-branching).
  //    Put that word in any of the possible roles for that delimiter.
  //    Put any extra words not picked up by that argument in {{{object}}}.
  // # Rinse and repeat.
  //
  // Since at every substep (every delimiter) in step 2 above there are
  // multiple possible role-argument assignments to be made, all the parses up
  // to the current delimiter is kept in {{{theseParses}}}. Any role-argument
  // assignment is done to a copy of every member of {{{theseParses}}} (each
  // individually called {{{thisParse}}} and these new copies are kept in
  // {{{newParses}}}. At the end of the loop, {{{newParses}}} becomes the new
  // {{{theseParses}}} to become the basis for the next loop. It's //intense//.
  //
  // Each argument that's set gets a property called {{{_order}}}. This is used
  // by {{{Parse.displayText}}} and {{{Parse.displayHtml}}}
  // in order to reconstruct the input
  // for display. The _order values become left-to-right placement values.
  // Each argument gets one _order value for both the argument and the delimiter
  // as we can reconstruct the order of the delimiter wrt the argument using
  // the branching preference.
  //
  // TODO: add better explanation/examples of {{{_order}}} in a blog post or
  // inline.
  //
  // The {{{scoreMultiplier}}} parameter is set at this point, making
  // {{{maxScore}}} valid for all returned parses.

  argFinder: function argFinder(argString, verb, input, thisQuery) {
    // initialize possibleParses. This is the array that we're going to return.
    var possibleParses = [];
    // { push.apply(x, y) } is better than { x = x.concat(y) }
    var {push} = possibleParses;

    // Some verbs are coming here with undefined scores??

    // if the argString is empty, return a parse with no args.
    if (!argString) {
      let defaultParse = new Parse(thisQuery, input, verb, argString);
      if (defaultParse._verb.id) {
        defaultParse.scoreMultiplier = 1;
      } else {
        defaultParse.scoreMultiplier = 0.3;
        defaultParse._suggested = true;
      }

      // The verb match's score affects the scoreMultiplier.
      defaultParse.scoreMultiplier *= (defaultParse._verb.score || 1);
      // start score off with one point for the verb.
      defaultParse._score = defaultParse.scoreMultiplier;

      defaultParse.args = {};
      return [defaultParse];
    }

    // if the verb doesn't take any arguments but the argString is not empty,
    // kill this parse.
    if (verb.id && !(verb.arguments || 0).length) {
      return [];
    }

    // split words using the splitWords() method
    let splitInput = this.splitWords(argString);
    // for example, if the input is "rar rar   rar"
    // then words = ['rar','rar','rar']
    // delimiters = [' ','   ']
    // allWords = ['rar',' ','rar','   ','rar']
    let {words, delimiters, allWords} = splitInput;

    // let's find all the possible delimiters
    let possibleDelimiterIndices =
      [+i for (i in words) if (this.hasDelimiter(words[i], verb.id))];

    // if the verb is set, only look for delimiters which are available
    let roles = verb.id ? this._rolesCache[verb.id] : this.roles;

    // this is a cache of the possible roles for each delimiter word encountered
    let rolesForEachDelimiterCache = {};

    // Find all the possible combinations of delimiters.
    // The "power set" of a set
    // is a set of all the subsets of the original set.
    // For example: a power set of [1,2] is [[],[1],[2],[1,2]]
    var possibleDelimiterCombinations = Utils.powerSet(
                                                possibleDelimiterIndices);

    // for each set of delimiterIndices which are possible...
    // Note that the values in the delimiterIndices for each delimiter are the
    // indices which correspond to those delimiters.
    EACH_DI:
    for each (let delimiterIndices in possibleDelimiterCombinations) {
      // don't process invalid delimiter combinations
      // (where two delimiters are back to back)
      for (let i = delimiterIndices.length; --i > 0;) {
        if (delimiterIndices[i - 1] + 1 === delimiterIndices[i])
          continue EACH_DI;
      }

      // theseParses will be the set of new parses based on this delimiter
      // index combination. We'll seed it with a Parse which doesn't
      // have any arguments set.
      let seedParse = new Parse(thisQuery, input, verb, argString);
      // get all parses started off with their scoreMultipier values
      if (verb.id) {
        seedParse.scoreMultiplier = 1;
      } else {
        seedParse.scoreMultiplier = 0.3;
        seedParse._suggested = true;
      }
      // if there are no delimiters at all, put it all in the direct object
      if (!delimiterIndices.length) {
        seedParse.setArgumentSuggestion(
          "object",
          { _order: 1,
            input: allWords.join(""),
            modifier: ""});
        possibleParses.push(seedParse);
        continue;
      }

      let lastIndex = delimiterIndices[delimiterIndices.length - 1];
      // Check for a delimiter at the end (if right-
      // branching) or at the beginning (if left-branching)
      // These are bad because then they will never find an associated
      // argument.
      if (this.branching === "right" && lastIndex === words.length - 1 ||
          this.branching === "left" && delimiterIndices[0] === 0)
        Utils.dump("maybe this is why I'm dead.");
      // TODO check if this breaks things and, if it does, kill these
      // delimiter combinations right here.

      // if there are extra words at the beginning or end, make them a
      // direct object
      if (this.branching === "left") {
        if (lastIndex < words.length - 1) {
          seedParse.setArgumentSuggestion(
            "object",
            { _order: 2 * delimiterIndices.length + 2,
              input: allWords.slice(2 * lastIndex + 2).join(''),
              modifier: ""}
            );
        }
      } else {
        if (delimiterIndices[0] > 0) {
          seedParse.setArgumentSuggestion(
            "object",
            { _order: 1,
              input: (allWords.slice(0, 2 * delimiterIndices[0] - 1)
                      .join('')),
              modifier: ""});
        }
      }

      var theseParses = [seedParse];

      // If we're right branching, we'll just go through left to right.
      // If we're left branching, however, we want to reverse the order of
      // the delimiterIndices in order to go through the arguments right to left.
      //
      // (It actually doesn't matter and could be done the other way, but this
      // is the way I set it up and set the _order based on this.)
      if (this.branching === "left") delimiterIndices.reverse();

      // Loop over each delimiter
      //
      // In each pass through this loop, we'll add new arguments to the copies
      // of the parses in theseParses and put them in newParses. Then, at the
      // end of the loop, we'll set theseParses = newParses. This way, at any
      // point throughout the loop, theseParses will be all the possible
      // parses of the arguments *up to* this point.
      for (let i = 0, l = delimiterIndices.length; i < l; ++i) {
        let newParses = [];

        // j will be used as an iterator for how far out from the delimiter we
        // want to reach to get an argument. For example, if we have
        //
        //   DELIMITER WORD1 WORD2 WORD3
        //
        // we want j to be the position of WORD1, WORD2, and WORD3 one after
        // the other. Thus we set jmin to be the position of WORD1 and jmax
        // to be the position of WORD3. (And inside out and vice versa for
        // left-branching languages, but you get the idea.)
        if (this.branching === "left") {// find args right to left
          var jmin = ((delimiterIndices[i + 1] == null) ?
                      0 : delimiterIndices[i + 1] + 1);
          var jmax = delimiterIndices[i] - 1;
        } else {
          var jmin = delimiterIndices[i] + 1;
          var jmax = ((delimiterIndices[i + 1] == null) ?
                      words.length - 1 : delimiterIndices[i + 1] - 1);
        }

        // Compute the possible roles for this delimiter
        // We'll keep these in a cache so we don't have to look it up
        // using Parser.getRoleByDelimiter() each time.
        let delim = words[delimiterIndices[i]];
        if (!rolesForEachDelimiterCache[delim])
          rolesForEachDelimiterCache[delim] =
            this.getRoleByDelimiter(delim, roles);

        // For each scope of arguments... for example,
        // WORD1, WORD1 WORD2, or WORD1 WORD2 WORD3...
        for (var j = jmin; j <= jmax; j++) {
          // for each delimiter's possible role
          for each (var role in rolesForEachDelimiterCache[delim]) {
            // for each of the current parses
            for (var k in theseParses) {
              // thisParse is our local copy. We'll mess with it and
              // add it into newParses.
              let thisParse = theseParses[k].copy();

              if (this.branching === "left") {// find args right to left
                // our argument is words (j)...(jmax)
                // note that Array.slice(i,k) returns *up to* k
                var argument = allWords.slice(2 * j, (2 * jmax) + 1).join('');
                // our modifier, including the space after
                var modifier = words[delimiterIndices[i]];
                var innerSpace = delimiters[delimiterIndices[i] - 1];
                var outerSpace = delimiters[delimiterIndices[i]];

                // put the selected argument in its proper role
                thisParse.setArgumentSuggestion(role,
                  { _order: 1 + 2*(delimiterIndices.length - i),
                    input: argument,
                    modifier: modifier,
                    innerSpace: innerSpace,
                    outerSpace: outerSpace
                  });

                // put the extra words between the earlier delimiter and our
                // arguments into the object role
                if (j != jmin) {

                  // our argument is words (jmin)...(j-1)
                  // note that Array.slice(i,j) returns *up to* j
                  var argument = allWords.slice(2 * jmin, 2 * (j - 1) + 1)
                                         .join('');
                  var outerSpace = (2 * (j - 1) + 1 < allWords.length ?
                                    allWords[2 * (j - 1) + 1] : '');

                  // push it!
                  thisParse.setArgumentSuggestion('object',
                    { _order: 2*(delimiterIndices.length - i),
                      input:argument,
                      modifier:'',
                      innerSpace: '',
                      outerSpace: outerSpace
                    });

                }
              } else {
                // our argument is words (jmin)...(j)
                // note that Array.slice(i,j) returns *up to* j
                var argument = allWords.slice(2 * jmin, (2 * j) + 1)
                                    .join('');

                // our delimiter
                var modifier = words[delimiterIndices[i]];
                var innerSpace = delimiters[delimiterIndices[i]];
                var outerSpace = delimiters[delimiterIndices[i] - 1];

                // put the selected argument in its proper role
                thisParse.setArgumentSuggestion(role,
                  { _order: 1 + 2*(i)+2 - 1,
                    input: argument,
                    modifier: modifier,
                    innerSpace: innerSpace,
                    outerSpace: outerSpace
                  });

                // put the extra words between this delimiter and the next
                // into the object role
                if (j != jmax) {

                  // our argument is words (j+1)...(jmax)
                  // note that Array.slice(i,j) returns *up to* j
                  var argument = allWords.slice(2 * (j + 1),(2 * jmax) + 1)
                                         .join('');
                  var outerSpace = (2 * (j + 1) - 1 >= 0 ?
                                    allWords[2 * (j + 1) - 1] : '');

                  // push it!
                  thisParse.setArgumentSuggestion('object',
                    { _order: 1 + 2*(i)+2,
                      input: argument,
                      modifier: '',
                      innerSpace: '',
                      outerSpace: outerSpace
                    });
                }
              }

              // add thisParse to newParses
              newParses.push(thisParse);
            }
          }
        }
        // we went through the loop once (through one delimiter) so
        // now we'll make theseParses = parses of all arguments through the
        // current delimiter and use it as the basis for the next delimiter's
        // argument assignments.
        theseParses = newParses;
      }

      var oneArgPerRole = function oneArgPerRole(parse) {
        // Returns true if there is only one argument per role.
        // Returns false otherwise. The "object" role is not checked.
        for (let role in parse.args)
          if (role !== "object" && parse.args[role].length > 1)
            return false;
        return true;
      }

      if (DONT_PARSE_MULTIPLE_ARGS_PER_ROLE)
        theseParses = theseParses.filter(oneArgPerRole);

      // Put all of the different delimiterIndices combinations' parses into
      // possibleParses to return it.
      push.apply(possibleParses, theseParses);
    }
    return possibleParses;
  },

  // ** {{{Parser#updateScoreMultiplierWithArgs}}} **
  // alter the score of a parse based on the number of arguments it has for
  // each of its roles
  updateScoreMultiplierWithArgs: function(parse) {
    for (let role in parse.args) {
      // if there are multiple arguments of any role, mark this parse down.
      if (parse.args[role].length > 1) {
        parse.scoreMultiplier *= Math.pow(0.5,
                                          parse.args[role].length - 1);
      }
    }
    // The verb match's score affects the scoreMultiplier.
    // If the verb is not set yet, it gets a 1, but that's fine, because by
    // virtue of having to suggest a verb, the scoreMultiplier has already
    // been *=0.3 elsewhere.
    parse.scoreMultiplier *= (parse._verb.score || 1);
    // start score off with one point for the verb.
    parse._score = parse.scoreMultiplier;
    return parse;
  },

  // ** {{{Parser#cleanArgument()}}} **
  //
  // {{{cleanArgument}}} is run on each argument when being assigned to a role.
  // {{{cleanArgument}}} is the place to do things like strip off articles like
  // "the" or "a" if that is appropriate for your language.
  cleanArgument: function(word) {
    return word;
  },

  // ** {{{Parser#interpolateSelection()}}} **
  //
  // {{{interpolateSelection}}} is taking the selected text and applying it to all
  // roles of all verbs being considered for the provided parse.
  interpolateSelection: function interpolateSelection(parse, selection) {
    let returnArr = [];
    let count = 0;

    // We will assign the whole selection to the direct object.
    // Then, in step 7, object -> other roles interpolation will
    // make sure that the selection gets tried in all roles of
    // the verb that is chosen.

    let parseCopy = parse.copy();
    if (!('object' in parseCopy.args))
      parseCopy.args.object = [];
    parseCopy.args['object'].push({
      _order: --count,
      input: selection,
      modifier: this._objectDelimiter || "",
      outerSpace: this.joindelimiter,
      fromSelection: true});
    parseCopy.scoreMultiplier *= 1.2; // TODO: do we really want this?
    //parseCopy._score = parseCopy.scoreMultiplier;
    returnArr.push(parseCopy);
    return returnArr;
    // TODO: create differential updateScoreMultiplierWithArgs
  },

  // ** {{{Parser#substituteSelection()}}} **
  //
  // {{{substituteSelection()}}} takes a parse and a selection string. It
  // should only be called if the {{{selection}}} is not empty. It looks for
  // any of the anaphora set in {{{Parser#anaphora}}} and creates a copy of
  // that parse where that anaphor has been substituted with the selection
  // string.
  //
  // The new string with the substitution is assigned to each arg's
  // {{{input}}} property.
  //
  // An array of //new// parses is returned, so it should then be
  // {{{concat}}}'ed to the current running list of parses.
  substituteSelection: function substituteSelection(parse, selection) {
    var returnArr = [];

    for (let role in parse.args) {
      let args = parse.args[role];
      for (let i in args) {
        let oldArg = args[i].input;
        let newArg = oldArg.replace(this._patternCache.anaphora, selection);

        if (newArg !== oldArg) {
          let parseCopy = parse.copy();
          parseCopy.args[role][i].input = newArg;
          parseCopy.scoreMultiplier *= 1.2;
          returnArr.push(parseCopy);
        }
      }
    }

    return returnArr;
  },

  // ** {{{Parser#substituteNormalizedArgs()}}} **
  //
  // {{{substituteNormalizedArgs()}}} takes a parse. It runs each argument's
  // {{{input}}} through {{{normalizeArg}}}. If {{{normalizeArg}}} returns
  // matches, it is substituted into the parse.
  //
  // An array of //new// parses is returned, so it should then be
  // {{{concat}}}'ed to the current running list of parses.
  substituteNormalizedArgs: function substituteNormalizedArgs(parse) {
    var returnArr = [];

    for (let role in parse.args) {
      let args = parse.args[role];
      for (let i in args) {
        let arg = args[i].input;

        let baseParses = [parse].concat(returnArr);

        for each (let substitute in this.normalizeArgument(arg)) {
          for each (let baseParse in baseParses) {
            let parseCopy = baseParse.copy();
            parseCopy.args[role][i].inactivePrefix = substitute.prefix;
            parseCopy.args[role][i].input = substitute.newInput;
            parseCopy.args[role][i].inactiveSuffix = substitute.suffix;
            returnArr.push(parseCopy);
          }
        }
      }
    }

    return returnArr;
  },

  // ** {{{Parser#normalizeArg}}} **
  //
  // Returns an array of arrays of the form
  // [[arg,prefix,new argument,suffix]]
  //
  // For example, if this is Catalan, we may strip off the article "el"
  // so we'd return, on input "el google",
  // [['el google','el ','google','']]
  //
  // The easiest way to do this is to just use a String.match(), like
  // {{{return [input.match(/(el\s+|la\s+)(.+)()/i)]}}}

  normalizeArgument: function(input) {
    return [];
  },

  // ** {{{Parser#applyObjectsToOtherRoles()}}} **
  //
  // {{{applyObjectsToOtherRoles()}}} takes a parse. It looks for
  // each argument with role "object" and didn't have a modifier
  // and returns new copies of the parse with this object applied
  // to other args with modifiers.
  //
  // An array of //new// parses is returned, so it should then be
  // {{{concat}}}'ed to the current running list of parses.
  applyObjectsToOtherRoles: function applyObjectsToOtherRoles(parse) {
    var returnArr = [];

    // if nothing had the role "object", return nothing.
    if (!parse.args.object) return [];

    let baseParses = [parse];
    let returnArr = [];
    let rolesToTry = this._otherRolesCache;
    if (parse._verb.id) {
      rolesToTry = {};
      for each (let arg in parse._verb.arguments) {
        if (arg.role != 'object')
          rolesToTry[arg.role] = this._otherRolesCache[arg.role];
      }
    }

    for (let key in parse.args.object) {
      let object = parse.args.object[key];
      // if the object has a modifier, we don't want to override that
      // so we won't apply this object to other roles.
      if (object.modifier)
        continue; // goes to the next parse.args.object key

      // If the argument was not from a selection context (ie., it was from
      // the input (argString)) and the known verb *does* have an object,
      // don't apply. In other words, "weather chicago" > "weather near chicago"
      // but not "twitter hello" > "twitter as hello".
      if (!object.fromSelection && parse._verb.id) {
        let roles = [arg.role for each (arg in parse._verb.arguments)];
        if (roles.indexOf('object') != -1)
          continue;
      }

      let newParses = [];
      for (let role in rolesToTry) {
        let delimiter = rolesToTry[role];
        for each (let baseParse in baseParses) {
          if (DONT_PARSE_MULTIPLE_ARGS_PER_ROLE && role in baseParse.args)
            continue;
          let parseCopy = baseParse.copy();
          let objectCopy = {__proto__: object,
                            modifier: delimiter,
                            innerSpace: this.joindelimiter};
          parseCopy.args.object.splice(key,1);
          if (!parseCopy.args[role])
            parseCopy.args[role] = [];
          parseCopy.args[role].push(objectCopy);

          newParses.push(parseCopy);
        }
      }
      baseParses = baseParses.concat(newParses);
      returnArr = returnArr.concat(newParses);
    }

    let realReturnArr = [];
    for each (let parse in returnArr) {
      if (!parse.args.object.length ||
           (parse.args.object.length === 1
            && parse.args.object[0] == undefined) )
      delete(parse.args.object);

      // for parses with an impossible combination of roles
      let signature = [role for (role in parse.args)];
      if (this._roleSignatures[signature.sort().join()]) {
        realReturnArr.push(parse);
      }

    }

    return realReturnArr;
  },

  // ** {{{Parser#suggestVerb()}}} **
  //
  // {{{suggestVerb()}}} takes a parse and, if it doesn't yet have a verb,
  // suggests one based on its arguments. If one of the parse's argument's
  // roles is not used by a verb, that verb is not suggested.
  //
  // If a verb was suggested, {{{_suggested}}} is set to true.
  //
  // An array of //new// parses is returned, so it should replace the original
  // list of parses (which may include parses without any verbs). If none of
  // the verbs match the arguments' roles in the parse, it will return [].
  //
  // All returning parses also get their {{{scoreMultiplier}}} property set
  // here as well.
  suggestVerb: function suggestVerb(parse, inputMatchesSomeVerb) {
    // for parses which already have a verb
    if (parse._verb.id) return [parse];

    // for parses WITHOUT a set verb:
    var returnArray = [];
    var verbs = this._verbList;

    // For the time being... kill parses which have multiple arguments
    // of the same role, as we have no real way of dealing with them and
    // their scores are so low anyway...

    for each (let roleArgArray in parse.args) {
      if (roleArgArray.length > 1)
        return [];
    }

    VERBS:
    for each (let verb in verbs) {
      if (verb.disabled) continue;
      // Check each role in our parse.
      // If none of the roles are used by the arguments of the verb,
      // skip to the next verb.
      // Furthermore, if the role is used by an argument of the verb but the parse is using a
      // suggested verb (i.e. noun first suggestion) and the nountype of the arg uses
      // an external call, skip to the next verb
      let parser = this;
      for (let role in parse.args){
        if (!verb.arguments.some(function (arg) {
          let noExternals = parser._nounTypeIdsWithNoExternalCalls[arg.nountype.id];
          let noVerbMatchCase = (!inputMatchesSomeVerb &&
                                 parser.doNounFirstExternals);
          return (arg.role === role && (noExternals || noVerbMatchCase));
        })) continue VERBS;
      }

      // Verb's score is ranked from 0-1 not based on quality of match (there's no text to
      // match to) but solely on how often this verb has been used (for any input) in the
      // past.
      let frequency = this._suggestionMemory.getScore("", verb.id);
      let verbScore = 1 - ( 0.7 / ( 1 + frequency ) );
      // TODO score is getting assigned correctly... but then having no apparent effect
      // on ranking...  where is the score getting reset?  It's not in argFinder, these
      // verbs assigned here appear not to make it as far as argFinder.

      let parseCopy = parse.copy();
      // same as before: the verb is copied from the verblist but also
      // gets some extra properties (id, text, _order) assigned.
      parseCopy._verb = {
        __proto__: verb,
        text: verb.names[0],
        _order: (typeof this.suggestedVerbOrder === "function"
                 ? this.suggestedVerbOrder(verb.names[0])
                 : this.suggestedVerbOrder),
        score: verbScore
      };

      returnArray.push(parseCopy);
    }
    return returnArray;
  },

  // ** {{{Parser#suggestArgs()}}} **
  //
  // {{{suggestArgs()}}} returns an array of copies of the given parse by
  // replacing each of the arguments' text with the each nountype's suggestion.
  // This suggested result goes in the argument's {{{text}}}, {{{html}}},
  // and {{{data}}} properties. We'll also take this
  // opportunity to set each arg's {{{score}}} property, also coming from
  // the nountype's {{{suggest()}}} result, to be used in computing the
  // parse's overall score.
  //
  // If, along the way, we find that the verb does not take one of the
  // parsed arguments (nountype mismatch), we set {{{thisVerbTakesThisRole}}} =
  //  false and [] is returned.
  //
  // There may be multiple returned as each argument may have multiple possible
  // nountypes or multiple suggestions for each nountype.
  //
  // This function *also* takes care of filling in unfilled arguments with
  // the nountypes' and verbs' default values (if available).

  suggestArgs: function suggestArgs(parse) {
    //Utils.log('verb:'+parse._verb.name);

    // the combination is the combination of which suggestions of which
    // nountype we put in which arguments.
    let combinations = [[]];

    let maxSuggestions = parse._query.maxSuggestions;

    for (let role in parse.args) {
      let thisVerbTakesThisRole = false;

      // For the time being... kill parses which have multiple arguments
      // of the same role, as we have no real way of dealing with them and
      // their scores are so low anyway...
      // This helps fix testDontInterpolateInTheMiddleOfAWord
      if (parse.args[role].length > 1)
        return [];

      for each (let verbArg in parse._verb.arguments) {
        if (role == verbArg.role) {
          // for each argument of this role...
          for (let i in parse.args[role]) {
            // this is the argText to check
            let argText = parse.args[role][i].input;

            let newOrders = [];
            // we'll put all the new orders, based on the previous combinations,
            // here.

            let nountypeId = verbArg.nountype.id;
            let suggestions = this._nounCache.getSuggs(argText, nountypeId);
            if (suggestions) { //new
              for (let suggestionId in suggestions) {
                for each (let baseOrder in combinations) {
                  let newOrder = baseOrder.concat([{
                    role:role,
                    argText:argText,
                    nountypeId:nountypeId,
                    suggestionId:suggestionId}]);
                  newOrders.push(newOrder);
                }
              }
              combinations = newOrders;
              thisVerbTakesThisRole = true;
            }
          }

          // If thisVerbTakesThisRole, it means that we've already found the
          // appropriate argument for this role and thus we do not have to
          // keep looking. The continue statement means we move onto the next
          // role.
          if (thisVerbTakesThisRole)
            continue;
        }
      }
      //Utils.log('finished role');
      if (!thisVerbTakesThisRole) {
        //Utils.log('killing parse because it doesnt take the role:',parse._id);
        return [];
      }
    }

    let returnArr = [];

    for each (let combination in combinations) {
      let combinationJson = JSON.stringify(combination);
      // if we've already completed this combination of suggestions
      // before, don't do it again.
      if (parse._suggestionCombinationsThatHaveBeenCompleted[combinationJson])
        continue;

      let combinationParse = parse.copy();
      for each (let argOrder in combination) {
        let {role,argText,nountypeId,suggestionId} = argOrder;
        let suggestion = this._nounCache.getSuggs(argText,nountypeId)[suggestionId];
        for each (let arg in combinationParse.args[role]) {
          if (arg.input == argText) {
            for(let key in suggestion)
              arg[key] = suggestion[key];

            if (role == 'object' && this._objectDelimiter
                && !(arg.modifier))
              arg.score *= 0.6;

            break;
          }
        }
      }

      // we use this array to check whether we've used this before.
      parse._suggestionCombinationsThatHaveBeenCompleted[combinationJson]
                                                                  = true;
      combinationParse._combination = combinationJson;
      returnArr.push(combinationParse);
    }

    // now check for unfilled arguments so we can fill them with defaults
    let {unfilledRoles} = parse;
    let myDefaultsCache = {};

    for each (let role in unfilledRoles) {
      let missingArg;
      for each (let arg in parse._verb.arguments) if (arg.role === role) {
        missingArg = arg;
        break;
      }
      let noun = missingArg.nountype;
      let defaultValues = [];
      if (missingArg.input) {
        defaultValues = this._nounCache.getSuggs(missingArg.input, noun.id);
      }
      else if (missingArg.default)
        defaultValues = [].concat(missingArg.default);
      else {
        if (!(noun.id in this._defaultsCache)) {
          let dvals = (typeof noun.default === "function"
                       ? noun.default()
                       : noun.default);
          Utils.isArray(dvals) ||
            (dvals = [dvals]);
          dvals[0] ||
            (dvals = [{text: "", html: "", data: null, summary: ""}]);
          this._defaultsCache[noun.id] = dvals;
        }
        defaultValues = this._defaultsCache[noun.id];
      }

      for each (let defaultValue in defaultValues) {
        // default-suggested arguments should be ranked lower
        defaultValue.score = (defaultValue.score || 1) / 2;

        // if a default value was set, let's make sure it has its modifier.
        if (defaultValue.text) {
          //Utils.log('text = '+defaultValue.text);
          defaultValue.outerSpace = this.joindelimiter;

          for each (let roleDesc in this.roles) {
            if (roleDesc.role == role) {
              //Utils.log('found the right role');
              defaultValue.modifier = roleDesc.delimiter;
              //Utils.log('delimiter: '+roleDesc.delimiter);
              if (roleDesc.delimiter == '')
                defaultValue.innerSpace = '';
              else
                defaultValue.innerSpace = this.joindelimiter;
              break;
            }
          }
        }
      }

      myDefaultsCache[role] = defaultValues;
    }

    for each (let role in unfilledRoles) {
      let newreturn = [];
      for each (let defaultValue in myDefaultsCache[role]) {
        for each (let parseToReturn in returnArr) {
          let newParse = parseToReturn.copy();
          newParse.setArgumentSuggestion(role, defaultValue);
          newreturn.push(newParse);
        }
      }
      returnArr = newreturn;
    }

    for each (let parse in returnArr) {
      // for each of the roles parsed in the parse
      for (let role in parse.args) {
        // multiply the score by each role's first argument's nountype match score
        parse._score += (parse.args[role][0].score || 0) * parse.scoreMultiplier;
      }
    }

    return returnArr;
  },

  // == Noun Type utilities ==
  //
  // In the future these methods of {{{Parser}}} probably ought to
  // go into a separate class or something.
  //
  // ** {{{Parser#detectNounType()}}} **
  //
  // This method does the nountype detecting.
  // It takes an argument string and runs through all of the noun types in
  // {{{Parser#_nounTypes}}} and gets their suggestions (via their
  // {{{.suggest()}}} methods). It then takes each of those suggestions,
  // marks them with which noun type it came from (in {{{.nountype}}})
  // and puts all of those suggestions in an object (hash) keyed by
  // noun type name.
  detectNounType:
  function detectNounType(currentQuery, x, nounTypeIds, callback) {

    let cachedNounTypeIds = [];
    let uncachedNounTypeIds = [];

    // Make a list of nountypes we need to suggest x as, and make a list
    // of nountypes which have already cached x.

    var DEFAULT_CACHE_TIME = 60*60*24; // seconds = 1 day

    for (let nounTypeId in nounTypeIds) {
      var time = this._nounCache.getTime(x,nounTypeId);
      var cacheTime = this._nounTypes[nounTypeId].cacheTime;
      if (time && cacheTime !== 0 && (cacheTime === -1 || Date.now() < time + (cacheTime || DEFAULT_CACHE_TIME) * 60)) {
        currentQuery.dump('detectDecision('+x+','+nounTypeId+'): using CACHED value from '+(Date.now() - time)+'ms ago');
        cachedNounTypeIds.push(nounTypeId);
      } else {
        currentQuery.dump('detectDecision('+x+','+nounTypeId+'): will compute now...');
        uncachedNounTypeIds.push(nounTypeId);
      }
    }

    Utils.setTimeout(function detectNounType_cachedCallback() {
      if (cachedNounTypeIds.length) {
        currentQuery.dump('found cached values of '+x+' for '+cachedNounTypeIds.join());
        if (typeof callback == 'function') {
//          currentQuery.dump("running callback ("+callback.name+") now");
          callback.apply(currentQuery, [x,cachedNounTypeIds]);
        }
      }
    });

    var handleSuggs = function detectNT_handleSuggs(suggs, id) {
      if (!suggs || !suggs.length)
        return [];
      if (!Utils.isArray(suggs)) suggs = [suggs];
      // This extra step here which admittedly looks redundant is to
      // "fix" arrays which were the product of a nountype from a locked
      // down feed plugin, to enable proper enumeration. This is due to some
      // weird behavior which has to do with XPCSafeJSObjectWrapper.
      // Ask satyr or mitcho for details.
      suggs = [suggs[key] for (key in suggs)];
      for each (let s in suggs) s.nountypeId = id;
      return suggs;
    };
    var thisParser = this;
    var myCallback = function detectNT_myCallback(suggestions, id, asyncFlag) {
      var ids = (
        id != null
        ? [id]
        : [id for each (id in uncachedNounTypeIds)
           if (thisParser._nounCache.getTime(x, id) ||
               currentQuery._detectionTracker.getComplete(x, id))]);

      for each (let newSugg in suggestions) {
        let {nountypeId} = newSugg;
        thisParser._nounCache.addSugg(x, nountypeId, newSugg);
        thisParser._nounCache.setTime(x, nountypeId);
      }

      if (currentQuery.finished) {
        currentQuery.dump("this query is already finished... so don't suggest this noun!");
        return;
      }

      if (typeof callback == 'function') {
//        currentQuery.dump("running callback ("+callback.name+") now");
        callback.apply(currentQuery, [x, ids, asyncFlag]);
      }
    };

    var activeNounTypes = this._nounTypes;

    if (uncachedNounTypeIds.length) {
      Utils.setTimeout(function detectNounType_asyncDetect(){
        var returnArray = [];
        var hx = Utils.escapeHtml(x);

        let ids = uncachedNounTypeIds;
        currentQuery.dump("detecting: " + x + " for " + ids);

        var dT = currentQuery._detectionTracker;
        var nC = thisParser._nounCache;
        for each (let id in uncachedNounTypeIds) {
          if (dT.getStarted(x, id)) continue;
          // let's mark this x, id pair as checked, meaning detection has
          // already begun for this pair.
          dT.setStarted(x, id, true);

          let thisId = id;
          var resultsFromSuggest = handleSuggs(
            activeNounTypes[id].suggest(
              x, hx, function detectNounType_completeAsyncSuggest(suggs) {
                if (!dT.getRequestCount(x, thisId))
                  dT.setComplete(x, thisId, true);
                suggs = handleSuggs(suggs, thisId);
                myCallback(suggs, thisId, true);
              }, null),
            id);

          var hadImmediateResults = false;
          for each (let result in resultsFromSuggest) {
            if (result.summary) {
              returnArray.push(result);
              hadImmediateResults = true;
            } else {
              dT.addOutstandingRequest(x, id, result);
            }
          }

          // Check whether (a) no more results are coming and
          // (b) there were no immediate results.
          // In this case, try to complete the parse now.
          if (!dT.getRequestCount(x, id)) {
            nC.setTime(x, id); // set as completed now, even if nothing was added
            dT.setComplete(x, id, true);
            if (!hadImmediateResults) {
              for each (let parseId in dT.getParseIdsToCompleteForIds(x, [id])) {
                currentQuery._verbedParses[parseId].complete = true;
              }
              if (currentQuery._verbedParses.every(function(parse) parse.complete))
                currentQuery.finishQuery();
            }
          }
        }
        myCallback(returnArray);
      }, 0);
    }
  },
  // ** {{{Parser#strengthenMemory}}} **
  //
  // Strengthen the association between the verb part of the user's input and the
  // suggestion that the user ended up choosing, in order to give better ranking to
  // future suggestions.
  strengthenMemory: function P_strengthenMemory(chosenSuggestion) {
    // Input (current contents of ubiquity input box) is passed to us for API backwards
    // compatibility reasons, but we can ignore it and get the verb part of the raw input
    // from verb.input.

    let chosenVerb = chosenSuggestion._verb.id;
    // Question:  Are the IDs guaranteed to be consistent across runs??
    let inputVerb = chosenSuggestion._verb.input;
    // inputVerb is undefined if the suggestion was made without any part of the input
    // being the verb.
    if (inputVerb) {
      this._suggestionMemory.remember(inputVerb, chosenVerb);
    }

    // Also keep track of total number of times verb has been used, regardless of input:
    this._suggestionMemory.remember("", chosenVerb);
  }
}

// == {{{ParseQuery}}} ==
//
// The {{{ParseQuery}}} interface is described in
// [[http://ubiquity.mozilla.com/trac/ticket/532|trac #532]].
//
// The constructor takes the Parser that's being used, the {{{queryString}}},
// {{{context}}} object, and {{{maxSuggestions}}}. Useful if you want to
// set some more parameters or watches on the query.
//
// The {{{Parser#newQuery()}}} method is used to initiate
// a query instead of calling {{{new ParseQuery()}}} directly.
//
var ParseQuery = function(parser, queryString, selObj, context,
                            maxSuggestions, dontRunImmediately) {
  this._date = new Date();
  this._idTime = this._date.getTime();
  this.parser = parser;
  this.input = queryString;
  //chop off leading and trailing whitespace from input
  this.input = this.input.replace(/^\s+|\s+$/g, '');
  this.context = context;
  this.maxSuggestions = maxSuggestions;
  this.selObj = selObj;

  // _detectionTracker is an instance of NounTypeDetectionTracker which
  // keeps track of all the nountype detections.
  this._detectionTracker = new NounTypeDetectionTracker(this);

  // code flow control stuff
  // used in async faux-thread contrl
  this.finished = false;
  this._keepworking = true;

  // ** {{{ParseQuery#_times}}} **
  //
  // {{{_times}}} is an array of post-UNIX epoch timestamps for each step
  // of the derivation. You can check it later to see how long different
  // steps took.
  this._times = [];

  // ** {{{ParseQuery#_step}}} **
  //
  // This {{{_step}}} property is increased throughout {{{_yieldParse()}}}
  // so you can check later to see how far the query went.
  this._step = 0;

  // ** {{{ParseQuery#lastParseId}}} **
  // This is a counter of the number of "parses" (including intermediate
  // parses) created during the parse query. Used to ID parses.
  this.lastParseId = 0;

  // TODO: Think about putting some components into
  // [[https://developer.mozilla.org/En/DOM/Worker|Worker threads]].

  // Internal variables
  // These are filled in one by one as we go along.
  this._input = '';
  this._preParses = [];
  this._possibleParses = [];
  this._verbedParses = [];
  this._topScores = [];

  // The percentage of nountype detection completed on last update
  this._previousProgress = 0;

  // this is a list of all Parse's as created.
  this._allParses = {};

  // ** {{{ParseQuery#_scoredParses}}} **
  this._scoredParses = [];


  this.dump("Making a new parser2 query: " + this.input);

  if (!dontRunImmediately)
    this.run();
}

ParseQuery.prototype = {
  dump: function PQ_dump(msg) {
    Utils.dump(this._idTime + ":" + (new Date - this._idTime), msg);
  },

  // ** {{{ParseQuery#run()}}} **
  //
  // {{{run()}}} actually starts the query. As a {{{yield}}}-ing model
  // of faux-threads is used right now, the code looks a little bizarre.
  //
  // Basically in every run of {{{doAsyncParse()}}}, it will move the
  // {{{parseGenerator = ParseQuery._yieldingParse()}}} generator
  // one step, meaning we progress in the parse from one {{{yield}}}
  // breakpoint to the next.
  //
  // Most of this async code is by Blair.
  run: function PQ_run() {
    this._keepworking = true;

    var parseGenerator = this._yieldingParse();
    var self = this;

//    self._lastAsync = null;
//    self._timeBetweenAsyncs = [];

    function doAsyncParse() {
//      if (self._lastAsync)
//        self._timeBetweenAsyncs.push(new Date - self._lastAsync);
//      self._lastAsync = new Date;
      try {
        var ok = parseGenerator.next();
      } catch(e) {
        if (e !== StopIteration) {
          Cu.reportError(e);
          Cu.reportError("Traceback for last exception:\n" +
                         ExceptionUtils.stackTrace(e));
        }
        return;
      }
      if (ok && !self.finished && self._keepworking)
        Utils.setTimeout(doAsyncParse, 0);
    }
    this.dump("I have initiated the async query.");
    Utils.setTimeout(doAsyncParse, 0);

    return true;
  },

  _next: function PQ__next() {
    this._times[this._step++] = new Date;
    this.dump("STEP " + this._step);
  },

  // ** {{{ParseQuery#_yieldingParse()}}} **
  //
  // {{{_yieldingParse()}}} is not really a normal function but
  // a [[https://developer.mozilla.org/en/New_in_JavaScript_1.7|generator]].
  // This has to do with the {{{doAsyncParse()}}} asynchronous parsing
  // system described above in {{{ParseQuery#run()}}}.
  //
  // The bottom line, though, is that this function defines the flow of the
  // parse derivation with {{{yield true}}} thrown in at different points
  // where the asynchronous query would stop and check in with the outside
  // world.
  //
  // The steps here are as described in
  // [[https://wiki.mozilla.org/Labs/Ubiquity/Parser_2|the ParserTNG proposal]].
  // Notes that the first two steps are actually done outside of
  // {{{_yieldingParse()}}}.
  //
  // # split words/arguments + case markers
  // # pick possible verbs
  // # pick possible clitics
  // # group into arguments
  // # substitute anaphora (aka "magic words")
  // # suggest normalized arguments
  // # attempt to apply objects to other roles for parses w/o verbs
  // # suggest verbs for parses without them
  // # do nountype detection + cache
  // # replace arguments with their nountype suggestions
  // # score + rank
  // # done!
  _yieldingParse: function PQ__yieldingParse() {

    // start with step 1
    this._next();

    // STEP 1: split into words
    this._input = this.parser.wordBreaker(this.input);
    yield true;
    this._next();

    // STEP 2: pick possible verbs
    this._preParses = this.parser.verbFinder(this._input, this.selObj.text);
    yield true;
    this._next();

    // STEP 3: pick possible clitics
    // TODO: find clitics
    // yield true;
    this._next();

    var {push} = Array.prototype;
    // STEP 4: group into arguments and apply selection interpolation
    for each (var preParse in this._preParses) {
      push.apply(this._possibleParses,
                 this.parser.argFinder(preParse.argString,
                                       preParse._verb,
                                       this.input,
                                       this));
    }
    yield true;

    //if we have a selection, apply the selection interpolation
    if (this.selObj.text && this.selObj.text.length) {
      let selection = this.selObj.text;
      for each (let parse in this._possibleParses) {
        push.apply(this._possibleParses,
                   this.parser.interpolateSelection(parse, selection));
      }
      yield true;
    }
    this._next();

    // STEP 5: substitute anaphora
    // set selection with the text in the selection context
    if (this.selObj.text) {
      let selection = this.selObj.text.replace(/\$/g, "$$$$");
      for each (let parse in this._possibleParses) {
        // if there is a selection and if we find some anaphora in the entire
        // input...
        if (this.parser._patternCache.anaphora.test(this._input)) {
          push.apply(this._possibleParses,
                     this.parser.substituteSelection(parse, selection));
        }
      }
      yield true;
    }
    this._next();

    // STEP 6: substitute normalized forms
    // check every parse for arguments that could be normalized.
    for each (let parse in this._possibleParses) {
      push.apply(this._possibleParses,
                 this.parser.substituteNormalizedArgs(parse));
    }
    yield true;
    this._next();

    // STEP 7: attempt to apply objects to other roles
    // Attempt to apply any args
    // with role "object" to other roles. This is so that parses like
    // "calendar" => "add to calendar" (role: goal) or "google" =>
    // "search with google" (role: instrument). This adds new usability to
    // overlord verbs by being able to just enter the provider name.
    for each (let parse in this._possibleParses) {
      push.apply(this._possibleParses,
                 this.parser.applyObjectsToOtherRoles(parse));
    }
    yield true;
    this._next();

    // STEP 8: suggest verbs for parses which don't have one
    var inputMatchesSomeVerb = false;
    var STEP_8_YIELD_LOOP_NUM = 10;
    var count = 0;
    if(this._possibleParses.some(function(parse) !parse._suggested))
      inputMatchesSomeVerb = true;
    for each (let parse in this._possibleParses) {
      let newVerbedParses = this.parser.suggestVerb(parse, inputMatchesSomeVerb);
      for each (let newVerbedParse in newVerbedParses) {
        newVerbedParse = this.parser.updateScoreMultiplierWithArgs(newVerbedParse);
        newVerbedParse._suggestionCombinationsThatHaveBeenCompleted = {};
        this.addIfGoodEnough('verbed', newVerbedParse);
      }
      if (count++ % STEP_8_YIELD_LOOP_NUM === 0)
        yield true;
    }
    if (count % STEP_8_YIELD_LOOP_NUM !== 0)
      yield true;

    // rekey this._verbedParses so we're using the right ID's
    // this simplifies some things later where we need to track these parse ID's
    var newVerbedParses = [];
    for each (let parse in this._verbedParses) {
      newVerbedParses[parse._id] = parse;
    }
    this._verbedParses = newVerbedParses;
    this._next();

    // STEP 9: do nountype detection + cache
    // STEP 10: suggest arguments with nountype suggestions
    // STEP 11: score

    // let's keep a list of arguments we need to cache
    this._argsToCache = {};
    var dT = this._detectionTracker;

    for (let parseId in this._verbedParses) {
      let parse = this._verbedParses[parseId];
      if (Utils.isEmpty(parse.args))
        // This parse doesn't have any arguments. Complete it now.
        Utils.setTimeout(function completeNow(thisQuery, parse) {
          thisQuery.completeParse(parse);
        }, 0, this, parse);
      else for (let role in parse.args) {
        let arg = parse.args[role];
        for each (let x in arg) {
          // this is the text we're going to cache
          let argText = x.input;
          let ids = [verbArg.nountype.id
                     for each (verbArg in parse._verb.arguments)
                     if (verbArg.role === role)];

          if (!(argText in this._argsToCache)) {
            this._argsToCache[argText] = 1;
          }

          for each (let id in ids) {
            dT.addParseIdToComplete(argText, id, parseId);
          }
        }
      }
    }

    count = 0;
    var STEP_9_YIELD_LOOP_NUM = 4;
    for each (let {argText, nounTypeIds} in
              dT.getArgsAndNounTypeIdsToCheck(this.parser)) {
      this.parser.detectNounType(this, argText, nounTypeIds,
                                 this.tryToCompleteParses);
      // we don't need to yield that often...
      if (count++ % STEP_9_YIELD_LOOP_NUM === 0)
        yield true;
    }
  },

  // Set up tryToCompleteParses()
  // This function will be called at the end of each nountype detection.
  // If it finds some parse that that is ready for scoring, it will then
  // handle the scoring.
  tryToCompleteParses: function PQ_tryToCompleteParses(argText, ids, asyncFlag) {
    this.dump("tryToCompleteParses(" + argText + "," + ids + ")");

    if (this.finished) {
      this.dump("this query has already finished");
      return false;
    }

    var addedAny = false;
    var dT = this._detectionTracker;
    var parseIdsToTryToComplete = dT.getParseIdsToCompleteForIds(argText,ids)
    this.dump("parseIds to try to complete:" + parseIdsToTryToComplete);
    for each (let parseId in parseIdsToTryToComplete) {
      let thisParse = this._verbedParses[parseId];
      if (!thisParse.complete &&
          thisParse.allNounTypesDetectionHasCompleted()) {
        addedAny = this.completeParse.apply(this,[thisParse]) || addedAny;
      }
    }

    // Only call onResults if we added any parses, or if we just
    // passed the throttling threshold for displaying results
    //
    // Also, don't run onResults here if this.finished,
    // as if the finished flag was just turned on, it would have independently
    // called onResults.

    if (this.aggregateScoredParses().length > 0
         && !this.finished) {
      // THROTTLING OF ONRESULTS (#833) - still experimental
      var throttleThreshold = 0.8;
      var progress = this._detectionTracker.detectionProgress;
      var passedThreshold = this._previousProgress < throttleThreshold &&
                            progress >= throttleThreshold;
      if ((addedAny && (asyncFlag || progress >= throttleThreshold)) ||
        passedThreshold){
        this.dump("calling onResults now");
        this.onResults();
      }
      this._previousProgress = progress;
    }
    return addedAny;
  },

  completeParse: function PQ_completeParse(thisParse) {
    var requestCount = thisParse.getRequestCount();

    if (!(thisParse._requestCountLastCompletedWith == undefined)
        && thisParse._requestCountLastCompletedWith == requestCount) {
      return false;
    }

    thisParse._requestCountLastCompletedWith = requestCount;

    if (requestCount == 0) {
      thisParse.complete = true;
    }

    // go through all the arguments in thisParse and suggest args
    // based on the nountype suggestions.
    // If they're good enough, add them to _scoredParses.
    var suggestions = this.parser.suggestArgs(thisParse);

    var addedAny = false;
    for each (let newParse in suggestions)
      addedAny = this.addIfGoodEnough("scored", newParse) || addedAny;

    if (this._verbedParses.every(function(parse) parse.complete))
      this.finishQuery();

    return addedAny;
  },

  finishQuery: function PQ_finishQuery() {
    this._next();
    this.finished = true;
    this.dump("done!!!");
    /*
    var steps = this._step;
    for (let i = 1; i < steps; ++i)
      this.dump("step " + i + ": " +
                (this._times[i] - this._times[i-1]) + " ms");
    this.dump("total: " +
              (this._times[steps-1] - this._times[0]) + " ms");
    this.dump("There were " + this.aggregateScoredParses().length + " completed parses");
    */
    this.onResults();
  },
  aggregateScoredParses: function PQ_aggregateScoredParses() {
    return this._scoredParses;
  },
  // ** {{{ParseQuery#hasResults}}} (read-only) **
  //
  // A getter for whether there are any results yet or not.
  get hasResults() { return this.aggregateScoredParses().length > 0; },

  // ** {{{ParseQuery#suggestionList}}} (read-only) **
  //
  // A getter for the suggestion list.
  get suggestionList() {
    return (this.aggregateScoredParses()
            .sort(byScoreDescending)
            .slice(0, this.maxSuggestions));
  },

  // ** {{{ParseQuery#cancel()}}} **
  //
  // If the query is running in async mode, the query will stop at the next
  // {{{yield}}} point when {{{cancel()}}} is called.
  cancel: function PQ_cancel() {
    //Utils.log(this);
    let reqCount = this._detectionTracker.getRequestCount();
    this.dump("cancelled! " + reqCount +
              " outstanding request(s) being canceled");
    //abort and reset any async requests that are running
    this._detectionTracker.abortOutstandingRequests();

    this._keepworking = false;
  },

  // ** {{{ParseQuery#onResults()}}} **
  //
  // A handler for the endgame. To be overridden.
  onResults: function PQ_default_onResults() {},

  // ** {{{ParseQuery#addIfGoodEnough()}}} **
  //
  // Takes a {{{parseClass}}} ("verbed" or "scored") and a {{{newParse}}}.
  //
  // Looking at the {{{maxSuggestions}}} value (= m), defines "the bar" (the
  // lowest current score of the top m parses in the {{{parseCollection}}}).
  // Adds the {{{newParse}}} to the {{{parseCollection}}} if it has a chance
  // at besting "the bar" in the future ({{{newParse.maxScore > theBar}}}).
  // If {{{newParse}}} was added and that "raised the bar", it will go through
  // all parses in the {{{parseCollection}}} and kill off those which have
  // no chance in hell of overtaking the new bar.
  //
  // A longer explanation of "Rising Sun" optimization strategy (and why it is
  // applicable here, and with what caveats) can be found in the article
  // [[http://mitcho.com/blog/observation/scoring-for-optimization/|Scoring for Optimization]].
  //
  // Returns true if the parse was added, false if not.
  addIfGoodEnough: function PQ_addIfGoodEnough(parseClass, newParse) {
    if (parseClass != 'verbed' && parseClass != 'scored')
      throw new Error('#addIfGoodEnough\'s parseClass arg must either be '
                     +'"scored" or "verbed".');

    var parseCollection = this['_'+parseClass+'Parses'];

    let parseIds = [parse._id for each (parse in parseCollection)];
    if (parseIds.indexOf(newParse._id) != -1) {
      this.dump("already contains parse #" + newParse._id + "!");
      return false;
    }

    var maxIndex = this.maxSuggestions - 1;
    if (!parseCollection[maxIndex]){
      parseCollection.push(newParse);
      return true;
    }

    parseCollection.sort(byScoreDescending);

    // "the bar" is the lowest current score among the top candidates
    // New candidates must exhibit the *potential* (via maxScore) to beat
    // this bar in order to get added.
    var theBar = parseCollection[maxIndex].score;

    // at this point we can already assume that there are enough suggestions
    // (because if we had less, we would have already returned with newParse
    // added). Thus, if the new parse's maxScore is less than the current bar
    // we will not return it (effectively killing the parse).
    if (newParse.maxScore < theBar)
      return false;

    // add the new parse and sort again
    parseCollection.push(newParse);
    parseCollection.sort(byScoreDescending);

    var newBar = parseCollection[maxIndex].score;
    if (newBar > theBar) {
      // sort by descending maxScore order
      parseCollection.sort(function(a, b) b.maxScore - a.maxScore);
      let i = parseCollection.length;
      while (--i > maxIndex && parseCollection[i].maxScore < theBar)
        parseCollection.pop();
    }

    return true;
  }
};

// == {{{NounTypeDetectionTracker}}} ==
//
// {{{NounTypeDetectionTracker}}} is the class for
// {{{ParseQuery#_detectionTracker}}} which is used to keep track of which
// (argText,nountypeId) pairs have been started or completed

var NounTypeDetectionTracker = function(query) {
  this._query = query;
  this.detectionSpace = {};
}
NounTypeDetectionTracker.prototype = {
  _query: null,
  detectionSpace: {},
  _ensureNode: function DT__ensureNode(arg,id) {
    if (!(arg in this.detectionSpace))
      this.detectionSpace[arg] = {};
    if (!(id in this.detectionSpace[arg]))
      this.detectionSpace[arg][id] = { started: false, complete: false,
                                       parseIds: [],
                                       outstandingRequests: [] };
  },

  getStarted: function DT_getStarted(arg,id) {
    this._ensureNode(arg,id);
    return this.detectionSpace[arg][id].started;
  },
  setStarted: function DT_setStarted(arg,id,bool) {
    this._ensureNode(arg,id);
    return this.detectionSpace[arg][id].started = bool;
  },

  getComplete: function DT_getComplete(arg,id) {
    this._ensureNode(arg,id);
    return this.detectionSpace[arg][id].complete;
  },
  setComplete: function DT_setComplete(arg,id,bool) {
    this._ensureNode(arg,id);
    return this.detectionSpace[arg][id].complete = bool;
  },
  getParseIdsToComplete: function DT_getParseIdsToComplete(arg,id) {
    this._ensureNode(arg,id);
    return this.detectionSpace[arg][id].parseIds;
  },
  getParseIdsToCompleteForIds: function
    DT_getParseIdsToCompleteForIds(arg,ids) {
    let returnHash = {};
    for each (let id in ids)
      for each (let parseId in this.getParseIdsToComplete(arg,id))
        returnHash[+parseId] = true;
    return [id for (id in returnHash)];
  },
  addParseIdToComplete: function DT_addParseIdToComplete(arg,id,parseId) {
    this._ensureNode(arg,id);
    return this.detectionSpace[arg][id].parseIds.push(+parseId);
  },

  getOutstandingRequests: function DT_getOutstandingRequests(arg,id) {
    this._ensureNode(arg,id);
    return this.detectionSpace[arg][id].outstandingRequests;
  },
  addOutstandingRequest: function DT_addOutstandingRequest(arg,id,request) {
    this._ensureNode(arg,id);
    return this.detectionSpace[arg][id].outstandingRequests.push(request);
  },

  // **{{{NounTypeDetectionTracker#getArgsAndNounTypeIdsToCheck}}}**
  //
  // This returns an array of pairs of argument strings and the nountypes
  // they must be checked against.
  //
  // Now takes the current parser to get
  getArgsAndNounTypeIdsToCheck:
  function DT_getArgsAndNounTypeIdsToCheck(parser) {
    var returnArr = parser.extraArgsToDetect.slice();

    // note that the argsAndNounTypeIdsToCheck format designates
    // nounTypeIds to be a *hash*, not array.
    for (let argText in this.detectionSpace) {
      var nounTypeIds = {};
      for (let id in this.detectionSpace[argText])
        nounTypeIds[id] = true;
      returnArr.push({argText:argText,
                      nounTypeIds:nounTypeIds});
    }

    return returnArr;
  },

  // ** {{{NounTypeDetectionTracker#getRequestCount}}} **
  //
  // A getter for the total number of open requests
  getRequestCount: function DT_getRequestCount(x,id) {
    let numRequests = 0;
    for (let i in this.detectionSpace){
      if (x && x != i)
        continue;

      for (let j in this.detectionSpace[i]) {
        if (id && id != j)
          continue;

        for each (let req in this.detectionSpace[i][j].outstandingRequests) {
          if (req.readyState != undefined && req.readyState != 0
              && req.readyState != 4)
            numRequests++;
        }
      }
    }

    return numRequests;
  },

  abortOutstandingRequests: function DT_abortOutstandingRequests() {
    for (let i in this.detectionSpace){
      for (let j in this.detectionSpace[i]) {
        for each (let req in this.detectionSpace[i][j].outstandingRequests) {
          if (req.abort)
            req.abort();
        }
        // actually delete them once they've been aborted.
        this.detectionSpace[i][j].outstandingRequests = [];
      }
    }
  },
  get detectionProgress() {
    var dS = this.detectionSpace;
    var count = 0;
    var total = 0;
    for (let i in dS) {
      for (let j in dS[i]) {
        if (!(j in this._query.parser._nounTypes))
          continue;
        // detectionProgress only checks for the progress of noExternalCalls
        // nountypes
        if (!this._query.parser._nounTypes[j].noExternalCalls)
          continue;

        total++;
        count += dS[i][j].complete;
//              if (!(dS[i][j].complete)
//                Utils.log(j);
      }
    }
    return (count/total);
  }
}

var NounCache = function(parser) {
  this.cacheSpace = {};
}
NounCache.prototype = {
  cacheSpace: {},
  _ensureNode: function NC__ensureNode(arg,id) {
    if (!(arg in this.cacheSpace))
      this.cacheSpace[arg] = {};
    if (!(id in this.cacheSpace[arg]))
      this.cacheSpace[arg][id] = { setTime: null,
                                   suggs: [] };
  },
  getTime: function NC_getTime(arg,id) {
    this._ensureNode(arg,id);
    return this.cacheSpace[arg][id].setTime;
  },
  setTime: function NC_getTime(arg,id,time) {
    this._ensureNode(arg,id);
    this.cacheSpace[arg][id].setTime = (time || Date.now());
  },
  getSuggs: function NC_getSuggs(arg,id) {
    this._ensureNode(arg,id);
    // If it's not cached, return null.
    // This way you can distinguish "never been cached" (null) and
    // cached [].
    if (!this.cacheSpace[arg][id].setTime)
      return null;
    return this.cacheSpace[arg][id].suggs;
  },
  getBestSugg: function NC_getBestSugg(arg,id) {
    this._ensureNode(arg,id);
    if (!this.cacheSpace[arg][id].setTime)
      return null;
    var bestSugg = null;
    for each (var sugg in this.cacheSpace[arg][id].suggs)
      if (sugg.score > (bestSugg != null ? bestSugg.score : 0))
        bestSugg = sugg;
    return bestSugg;
  },
  hasSuggsForIds: function NC_hasSuggsForIds(arg,ids) {
    for (let id in ids) {
      this._ensureNode(arg,id);
    }
    for (let id in ids) {
      if (!this.cacheSpace[arg][id].setTime)
        return false;
    }
    return true;
  },
  setSuggs: function NC_setSugg(arg,id,suggs) {
    this._ensureNode(arg,id);
    this.cacheSpace[arg][id].setTime = Date.now();
    this.cacheSpace[arg][id].suggs = suggs;
  },
  addSugg: function NC_addSugg(arg,id,newSugg) {
    this._ensureNode(arg,id);
    let thisSuggIsNew = true;
    for each (let oldSugg in this.cacheSpace[arg][id].suggs) {
      // Here, we only compare the summary propery.
      // There are a few reasons for this:
      // 1. We want to avoid suggestions which only differ in score
      // 2. If the summary is not different, then the data should
      //    be different, as then the user is forced to choose between
      //    identical suggestions.
      // 3. Checking data is dangerous as isEqual (now deprecated) is not
      //    made to handle xpconnect objects, which are often in data.
      //    (This was, I suspect, the problem with #829.)
      if (newSugg.summary == oldSugg.summary) {
        thisSuggIsNew = false;
        oldSugg.score = Math.max(oldSugg.score,newSugg.score);
      }
    }

    if (thisSuggIsNew)
      this.cacheSpace[arg][id].suggs.push(newSugg);
  },
  clearSuggsForId: function NC_clearSuggsForId(id) {
    for (var arg in this.cacheSpace) {
      if (id in this.cacheSpace[arg])
        this.cacheSpace[arg][id] = { setTime: null,
                                     suggs: [] };
    }
  }
}

// == {{{Parse}}} ==
//
// {{{Parse}}} is the class for all of the parses which will be returned
// in an array by the {{{ParseQuery#suggestionList}}}. It is also used
// throughout the parse process.
//
// The constructor takes the {{{branching}}} and {{{joindelimiter}}} parameters
// from the {{{Parser}}} (which are used for the {{{displayText}}}
// and {{{displayHtml}}} methods) and the {{{verb}}} and {{{argString}}}.
// Individual arguments in
// the property {{{args}}} should be set individually afterwards.

var Parse = function(query, input, verb, argString, parent) {
  this._query = query;
  this.input = input;
  this._verb = verb;
  this.argString = argString;
  this.args = {};
  // this is the internal score variable--use the score property
  this._score = 0;
  this.scoreMultiplier = 0;
  // !complete means we're still parsing or waiting for async nountypes
  this.complete = false;
  this._id = (query.lastParseId ++);
  if (parent)
    this._parent = parent;
  this._query._allParses[this._id] = this;
}

Parse.prototype = {
  // ** {{{Parse#displayText}}} **
  //
  // {{{displayText}}} prints the verb and arguments in the parse by
  // ordering all of the arguments (and verb) by their {{{_order}}} properties.
  // It displays the parses in plain text (no html).
  get displayText() {
    // This is the main string to be returned.
    let display = '';
    // This string is built in case there's a verb at the end of the sentence,
    // in which case we slap this on at the end.
    let displayFinal = '';

    // If the verb has _order = -1, it means it was at the end of the input.
    if (this._verb._order != -1){
      display = (this._verb.text || 'null')
        + this._query.parser.joindelimiter;
    }
    else {
      displayFinal = this._query.parser.joindelimiter
        + (this._verb.text || 'null');
    }
    // Copy all of the arguments into an ordered array called argsArray.
    // This will then be in the right order for display.
    let argsArray = [];
    let maxArgsNumber = 100; // HACK
    let negativeArgsArray = [];
    let unfilledArgs = []; // unfilled args are displayed at the end
    let hiddenRoles = [a.role for each (a in this._verb.arguments)
                       if (a.hidden)];
    for (let role in this.args) {
      if (~hiddenRoles.indexOf(role)) continue;
      for each (let argument in this.args[role]) {
        if (argument.text) {
          if ("_order" in argument) {
            if (argument._order > 0) {
              argsArray[argument._order] = argument;
              argsArray[argument._order].role = role;
            } else {
              negativeArgsArray[maxArgsNumber + argument._order] = argument;
              negativeArgsArray[maxArgsNumber + argument._order].role = role;
            }
          } else
            unfilledArgs.push(argument);
        }
      }
    }

    argsArray = argsArray.concat(negativeArgsArray).concat(unfilledArgs);

    for each (let arg in argsArray) {
      let className = 'argument';
      if (!arg.modifier)
        className = 'object';

      // Depending on the _branching parameter, the delimiter goes on a
      // different side of the argument.
      if (this._query.parser.branching == 'right') {
        display += (arg.outerSpace || '')
          + (arg.modifier ? arg.modifier + arg.innerSpace : '')
          + (arg.inactivePrefix ? arg.inactivePrefix : '')
          + "[ " + (arg.summary || arg.text || arg.input || arg.label) + " ]"
          + (arg.inactiveSuffix ? arg.inactiveSuffix : '');
      }
      else {
        display += (arg.inactivePrefix ? arg.inactivePrefix : '')
          + "[ " + (arg.summary || arg.text || arg.input || arg.label) + " ]"
          + (arg.inactiveSuffix ? arg.inactiveSuffix : '')
          + (arg.modifier ? arg.innerSpace + arg.modifier : '')
          + (arg.outerSpace || '');
      }
    }
    return display + displayFinal;
  },

  // ** {{{Parse#displayHtml}}} **
  //
  // {{{displayHtml}}} prints the verb and arguments in the parse by
  // ordering all of the arguments (and verb) by their {{{_order}}} properties.
  // It returns the parses with nice {{{<span class='...'></span>}}}
  // wrappers.
  get displayHtml() {
    let hesc = Utils.escapeHtml;
    let {parser} = this._query;
    // This is the main string to be returned.
    let display = "";
    // This string is built in case there's a verb at the end of the sentence,
    // in which case we slap this on at the end.
    let displayFinal = "";

    let verb = (
      '<span class="verb" title="' + hesc(this._verb.id || "null") + '">' +
      (hesc(this._verb.text) || "<i>null</i>") +
      "</span>");
    if (this._verb._order === -1)
      // the verb was at the end of the input.
      displayFinal = parser.joindelimiter + verb;
    else
      display = verb + parser.joindelimiter;
    // Copy all of the arguments into an ordered array called argsArray.
    // This will then be in the right order for display.
    let argsArray = [];
    let maxArgsNumber = 100; // HACK
    let negativeArgsArray = [];
    let unfilledArgs = []; // unfilled args are displayed at the end
    let hiddenRoles = [a.role for each (a in this._verb.arguments)
                       if (a.hidden)];
    for (let role in this.args) {
      if (~hiddenRoles.indexOf(role)) continue;
      for each (let argument in this.args[role]) {
        if (argument.text) {
          if ("_order" in argument) {
            if (argument._order > 0) {
              argsArray[argument._order] = argument;
              argsArray[argument._order].role = role;
            }
            else {
              negativeArgsArray[maxArgsNumber + argument._order] = argument;
              negativeArgsArray[maxArgsNumber + argument._order].role = role;
            }
          }
          else unfilledArgs.push(argument);
        }
      }
    }

    argsArray = argsArray.concat(negativeArgsArray).concat(unfilledArgs);

    for each (let arg in argsArray) {
      let summary = (
        '<span class="' + (arg.modifier ? "argument" : "object") + '">' +
        (!arg.inactivePrefix ? "" :
         '<span class="inactive">' + arg.inactivePrefix + "</span>") +
        arg.summary +
        (!arg.inactiveSuffix ? "" :
         "<span class='inactive'>" + arg.inactiveSuffix + "</span>") +
        "</span>");
      let mod = (!arg.modifier ? "" :
                 '<span class="delimiter" title="' + arg.role + '">' +
                 arg.modifier + arg.innerSpace + "</span>");
      let outersp = arg.outerSpace || "";
      // Depending on the _branching parameter, the delimiter goes on a
      // different side of the argument.
      display += (parser.branching === "right"
                  ? outersp + mod + summary
                  : summary + mod + outersp);
    }

    for each (let neededArg in this._verb.arguments) {
      let arg = this.args[neededArg.role];
      if (!arg || (arg[0] || 0).text) continue;
      let {label} = neededArg;
      if (!label) {
        let nt = neededArg.nountype;
        // _name is for backward compatibility
        label = hesc(nt.label || nt._name || "?");
      }
      label = ('<span class="' +
               (arg.modifier ? "argument" : "object") +
               '">' + label + "</span>");
      for each (let parserRole in parser.roles)
        if (parserRole.role === neededArg.role) {
          let delim = ('<span class="delimiter" title="' + parserRole.role +
                       '">' + parserRole.delimiter + "</span>");
          label = (parser.branching === "right"
                   ? delim + parser.joindelimiter + label
                   : label + parser.joindelimiter + delim);
          break;
        }
      display += (parser.joindelimiter +
                  '<span class="needarg">' + label + "</span>");
    }
    return display + displayFinal;
  },

  get displayHtmlDebug() (
    Utils.escapeHtml(this._id) + ": " + this.displayHtml + " (" +
    ((this.score * 100 | 0) / 100 || "<i>no score</i>") + ',' +
    ((this.scoreMultiplier * 100 | 0) / 100 || "<i>no score</i>") +
    ")"),

  get completionText() {
    var {lastNode} = this;
    var originalText = lastNode.input;
    var newText = lastNode.text;
    var findOriginal = new RegExp(originalText+'$');
    if (findOriginal.test(this.input))
      return this.input.replace(findOriginal,newText) + this._query.parser.joindelimiter;

    return this.input;
  },

  // **{{{Parse#icon}}}**
  //
  // Gets the verb's icon.
  get icon() this._verb.icon,

  // **{{{Parse#execute()}}}**
  //
  // Execute the verb. Only the first argument in each role is returned.
  // The others are thrown out.
  execute: function(context) {
    return this._verb.execute(context, this.firstArgs);
  },

  // **{{{Parse#preview()}}}**
  //
  // creates the verb preview.
  preview: function(context, previewBlock) {
    this._verb.preview(context, previewBlock, this.firstArgs);
  },

  // **{{{Parse#previewDelay}}} (read-only)**
  //
  // Return the verb's {{{previewDelay}}} value.
  get previewDelay() {
    return this._verb.previewDelay;
  },

  // **{{{Parse#previewUrl}}} (read-only)**
  //
  // Return the verb's {{{previewUrl}}} value.
  get previewUrl() {
    return this._verb.previewUrl;
  },

  // **{{{Parse#firstArgs}}} (read-only)**
  get firstArgs() {
    let firstArgs = {};
    var unnecessaryProperties = {_order: true,
                                 modifier: true,
                                 nountypeId: true,
                                 innerSpace: true,
                                 outerSpace: true,
                                 role: true};
    for (let role in this.args) {
      firstArgs[role] = {};
      for (let prop in this.args[role][0]) {
        if (!(prop in unnecessaryProperties))
          firstArgs[role][prop] = this.args[role][0][prop];
      }
    }
    return firstArgs;
  },

  // **{{{Parse#lastNode}}}**
  //
  // Return the parse's last node, whether a verb or an argument.
  // This can be used to power something like tab-completion, by replacing
  // {{{parse.lastNode.input}}} with {{{parse.lastNode.text}}}.
  get lastNode() {
    // default value if there are no arguments
    let lastNode = this._verb;
    if (this._verb._order != -1) {
      for (let role in this.args) {
        for each (let arg in this.args[role]) {
          if (arg.text != '') {
            if (arg._order == undefined) // if it was a default, it's last
              lastNode = arg;
            else if (arg._order > lastNode._order)
              lastNode = arg;
          }
        }
      }
    }
    return lastNode;
  },

  // **{{{Parse#getArgsAndNounTypeIdsToCheck}}} (read-only)**
  //
  // This returns an array of pairs of argument strings and the nountypes
  // they must be checked against.
  getArgsAndNounTypeIdsToCheck: function PP_getArgsAndNounTypeIdsToCheck() {
    if (this._argsAndNounTypeIdsToCheck)
      return this._argsAndNounTypeIdsToCheck;

    var foundNoNounTypesToCheck = true;
    var returnArr = this._argsAndNounTypeIdsToCheck = [];
    for (let role in this.args) {
      // for each argument of this role...
      for each (let arg in this.args[role]) {
        // this is the argText to check
        let argText = arg.input;
        let nounTypeIds = {};
        for each (let verbArg in this._verb.arguments) {
          if (verbArg.role == role) {
            let id = verbArg.nountype.id;
            nounTypeIds[verbArg.nountype.id] = true;
          }
        }

        if (nounTypeIds.length)
          foundNoNounTypesToCheck = false;

        returnArr.push({argText:argText,
                        nounTypeIds:nounTypeIds});
      }
    }

    return returnArr;
  },

  getRequestCount: function PP_getRequestCount() {
    let requestCount = 0;
    for each (let {argText,nounTypeIds} in
                                this.getArgsAndNounTypeIdsToCheck()) {
      for each (let id in nounTypeIds)
        requestCount += this._query._detectionTracker.getRequestCount(argText,id);
    }
    return requestCount;
  },

  // **{{{Parse#allNounTypesDetectionHasCompleted()}}} (read-only)**
  //
  // If all of the arguments' nountype detection has completed, returns true.
  // Here, "nountype detection has completed" means that either (arg,nountype)
  // is marked "complete" (meaning there are no outstanding async requests)
  // OR there are some suggestions.
  // This means this parse can move onto Step 8.
  allNounTypesDetectionHasCompleted: function
    PP_allNounTypesDetectionHasCompleted() {
    var argsAndNounTypeIdsToCheck = this.getArgsAndNounTypeIdsToCheck();
    var thisQuery = this._query;

    for each (let {argText, nounTypeIds} in argsAndNounTypeIdsToCheck) {
      if (!thisQuery.parser._nounCache.hasSuggsForIds(argText,nounTypeIds)) {
        for (let id in nounTypeIds) {
          if (!thisQuery._detectionTracker.getComplete(argText,id))
            return false;
        }
      }
    }
    // if all is in the nounCache
    return true;
  },

  // ** {{{Parse#maxScore}}} **
  //
  // {{{maxScore}}} computes the maximum possible score which a partial
  // parse could possibly yield. It's used in cases where an async nountype
  // has yet to return the nountype score for one or more arguments.
  get maxScore() {
    if (this.complete)
      return this.score;

    if (!this._verb.text)
      return 0; // we still cannot determine the maxScore

    // get one point for the verb
    var score = this.scoreMultiplier;

    for each (let verbArg in this._verb.arguments) {
      if (!this.args[verbArg.role])
        // in this case, no argument has been set for this role and never will.
        continue;
      // in this case, we've already received the score from the nountype.
      // or, the arg text was set for this role, but we haven't
      // yet received an async score. Wait for it and assume good faith.
      score += (this.args[verbArg.role][0].score || 1) * this.scoreMultiplier;
    }

    return score;
  },

  // ** {{{Parse#score}}} **
  //
  // {{{score}}} returns the current value of {{{_score}}}.
  get score() this._score,

  // ** {{{Parse#setArgumentSuggestion()}}} **
  //
  // Accepts a {{{role}}} and a suggestion and sets that argument properly.
  // If there is already an argument for that role, this method will *not*
  // overwrite it, but rather add an additional argument for that role.
  setArgumentSuggestion: function PP_setArgumentSuggestion(role, sugg) {
    (this.args[role] || (this.args[role] = [])).push(sugg);
  },

  // ** {{{Parse#unfilledRoles}}} **
  //
  // Gets a list of roles which the parse's verb accepts but
  // have not been filled yet
  get unfilledRoles()(this._verb.id
                      ? [verbArg.role
                         for each (verbArg in this._verb.arguments)
                         if (!(verbArg.role in this.args))]
                      : []),

  // ** {{{Parse#copy()}}} **
  //
  // Returns a copy of this parse.
  copy: function PP_copy() {
    var ret = new Parse(this._query,
                        this.input,
                        this._verb,
                        this.argString,
                        this);
    // NOTE: at one point we copied these args by
    // ret.args = {__proto__: this.args}
    // This, however, created duplicate parses (or, rather, the prototype copies
    // got touched) when substituteNormalizedArgs is enacted. We must prototype
    // each actual argument, not the collection of arguments, so this is what
    // we did.
    for (let role in this.args)
      ret.args[role] = [{__proto__: sugg}
                        for each (sugg in this.args[role])];
    ret.complete = this.complete;
    ret._suggested = this._suggested;
    ret._step = this._query._step;
    ret.scoreMultiplier = this.scoreMultiplier;
    ret._score = this._score;
    // for debug purposes
    ret._caller = this.copy.caller.name;
    return ret;
  }
};

function byScoreDescending(a, b) b.score - a.score;
