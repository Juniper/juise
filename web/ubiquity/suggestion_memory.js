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
 *   Jono DiCarlo <jdicarlo@mozilla.com>
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

// = SuggestionMemory =

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
const Z = {__proto__: null}; // keep this empty!

Cu.import("/ubiquity/modules/utils.js");
Cu.import("/ubiquity/modules/dbutils.js");

var EXPORTED_SYMBOLS = ["SuggestionMemory"];

// In this schema, one row represents that fact that for the
// named suggestionMemory object identified by (id_string),
// it happened (score) number of times that the user typed in
// the string (input) and, out of all the suggested completions,
// the one the user chose was (suggestion).
function openDatabase(file) DbUtils.connectLite(
  "ubiquity_suggestion_memory",
  { id_string  : "VARCHAR(256)",
    input      : "VARCHAR(256)",
    suggestion : "VARCHAR(256)",
    score      : "INTEGER" },
  [], file);

Utils.defineLazyProperty(this, openDatabase, "gDatabaseConnection");

var gTables = {__proto__: null};

// TODO: when and how do we need to close our database connection?

// == SuggestionMemory(id, connection) ==
// The constructor.
//
// {{{id}}} is a unique string which will keep this suggestion memory
// distinct from the others in the database when persisting.
//
// {{{connection}}} is an optional {{{mozIStorageConnection}}}
// object to specify its database connection.

function SuggestionMemory(id, connection) {
  this._init(id, connection);
}
SuggestionMemory.prototype = {
  constructor: SuggestionMemory,
  toString: function SM_toString() "[object SuggestionMemory]",
  toJSON: function SM_toJSON() this._table,

  _createStatement: function SM__createStatement(selectSql) {
    try {
      return this._connection.createStatement(selectSql);
    } catch (e) {
      e.message = this._connection.lastErrorString;
      throw e;
    }
  },

  _getScores: function SM__getScores(input)
    this._table[input] || (this._table[input] = {__proto__: null}),

  _init: function SM__init(id, connection) {
    this._id = id;
    this._connection = connection || gDatabaseConnection;
    this._table = gTables[id] || (gTables[id] = {__proto__: null});
    // this._table is a dictionary of dictionaries with a format like this:
    // {
    //   input1: {
    //     suggestion1: 3,
    //     suggestion2: 4,
    //   },
    //   input2: {
    //     suggestion3: 1,
    //   }
    // }

    // So now, get everything from the database that matches our ID,
    // and turn each row into an entry in this._table:
    var selStmt = this._createStatement(
      "SELECT input, suggestion, score " +
      "FROM ubiquity_suggestion_memory " +
      "WHERE id_string == ?1");
    selStmt.bindUTF8StringParameter(0, id);
    while (selStmt.executeStep()) {
      let suggs = this._getScores(selStmt.getUTF8String(0));
      suggs[selStmt.getUTF8String(1)] = +selStmt.getUTF8String(2);
    }
    selStmt.finalize();
  },

  // === {{{ SuggestionMemory#remember(input, suggestion, ammount) }}}
  // Increases the strength of the association between {{{input}}} and
  // {{{suggestion}}}.
  remember: function SM_remember(input, suggestion, ammount) {
    ammount = +ammount || 1;
    var scores = this._getScores(input);
    if (suggestion in scores) {
      var score = scores[suggestion] += ammount;
      var stmt = this._createStatement(
        "UPDATE ubiquity_suggestion_memory " +
        "SET score = ?1 " +
        "WHERE id_string = ?2 AND input = ?3 AND " +
        "suggestion = ?4");
      stmt.bindInt32Parameter(0, score);
      stmt.bindUTF8StringParameter(1, this._id);
      stmt.bindUTF8StringParameter(2, input);
      stmt.bindUTF8StringParameter(3, suggestion);
    }
    else {
      var score = scores[suggestion] = ammount;
      var stmt = this._createStatement(
        "INSERT INTO ubiquity_suggestion_memory " +
        "VALUES (?1, ?2, ?3, ?4)");
      stmt.bindUTF8StringParameter(0, this._id);
      stmt.bindUTF8StringParameter(1, input);
      stmt.bindUTF8StringParameter(2, suggestion);
      stmt.bindInt32Parameter(3, score);
    }
    stmt.execute();
    stmt.finalize();
    return score;
  },

  // === {{{ SuggestionMemory#getScore(input, suggestion) }}} ===
  // === {{{ SuggestionMemory#setScore(input, suggestion, score) }}} ===
  // Gets/Sets the number of times that {{{suggestion}}} has been associated
  // with {{{input}}}.
  getScore: function SM_getScore(input, suggestion)
    (this._table[input] || Z)[suggestion] || 0,
  setScore: function SM_setScore(input, suggestion, score)
    this.remember(input, suggestion, score - this.getScore(input, suggestion)),

  // === {{{ SuggestionMemory#getTopRanked(input, numResults) }}} ===
  // Returns the top {{{numResults}}} number of suggestions that have
  // the highest correlation with {{{input}}}, sorted.
  // May return fewer than {{{numResults}}},
  // if there aren't enough suggestions in the table.
  getTopRanked: function SM_getTopRanked(input, numResults) {
    let fetchStmt = this._createStatement(
      "SELECT suggestion FROM ubiquity_suggestion_memory " +
      "WHERE id_string = ?1 AND input = ?2 ORDER BY score DESC " +
      "LIMIT ?3");
    fetchStmt.bindUTF8StringParameter(0, this._id);
    fetchStmt.bindUTF8StringParameter(1, input);
    fetchStmt.bindInt32Parameter(2, numResults);
    let retVals = [];
    while (fetchStmt.executeStep()) {
      retVals.push(fetchStmt.getUTF8String(0));
    }
    fetchStmt.finalize();
    return retVals;
  },

  // === {{{ SuggestionMemory#wipe(input, suggestion) }}} ===
  // Wipes the specified entry out of this suggestion memory instance.
  // Omitting both {{{input}}} and {{{suggestion}}} deletes everything.
  // Be careful with this.
  wipe: function SM_wipe(input, suggestion) {
    let wheres =
      [["id_string", this._id], ["input", input], ["suggestion", suggestion]];
    let wipeStmt = this._createStatement(
      "DELETE FROM ubiquity_suggestion_memory WHERE " +
      [k + " = ?" + (i + 1)
       for ([i, [k]] in new Iterator(wheres))].join(" AND "));
    for (let [i, [, v]] in new Iterator(wheres))
      wipeStmt.bindUTF8StringParameter(i, v);
    wipeStmt.execute();
    wipeStmt.finalize();
    gTables[this._id] = null;
    this._init(this._id, this._connection);
  },
};

SuggestionMemory.openDatabase = openDatabase;

// TODO: Do we need functions for dealing with multiple SuggestionMemory
// instances, e.g. listSuggestionMemoryIds() or wipeAllSuggestionMemory()?
