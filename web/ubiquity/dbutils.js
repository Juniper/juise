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

// = DbUtils =

var EXPORTED_SYMBOLS = ["DbUtils"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

var DbUtils = {};

for each (let f in this) if (typeof f === "function") DbUtils[f.name] = f;
delete DbUtils.QueryInterface;

// === {{{ DbUtils.connectLite(tableName, schemaDict, initialRows, file) }}}
// Creates a simple DB file in the user's profile directory (if nonexistent)
// and returns a
// [[https://developer.mozilla.org/en/mozIStorageService|connection]] to it.
//
// {{{tableName}}}
// is the string which is used for both the file name and table name.
//
// {{{schemaDict}}} is the dictionary that represents the table schema.
//
// {{{initialRows}}} is an optional array of arrays that specifies
// initial values of the table.
//
// {{{file}}} is an optional {{{nsIFile}}} instance specifying the DB file.

function connectLite(tableName, schemaDict, initialRows, file) {
  if (!file)
    ((file = (Cc["@mozilla.org/file/directory_service;1"]
              .getService(Ci.nsIProperties)
              .get("ProfD", Ci.nsIFile)))
     .append(tableName + ".sqlite"));
  var connection = openDatabase(file);
  if (connection && !connection.tableExists(tableName)) {
    let schema = (
      "CREATE TABLE " + tableName + "(" +
      [key + " " + schemaDict[key] for (key in schemaDict)].join(",") +
      ");");
    for each (let row in initialRows) schema += (
      "INSERT INTO " + tableName + " VALUES(" +
      [typeof v === "string"
       ? "'" + v.replace(/\'/g, "''") + "'"
       : v == null ? "NULL" : v
       for each (v in row)].join(",") +
      ");");
    try { connection.executeSimpleSQL(schema) }
    catch (e) {
      Cu.reportError(
        tableName + " database table appears to be corrupt. Resetting it." +
        "\n(" + connection.lastErrorString + ")");
      // remove corrupt database table
      file.exists() && file.remove(false);
      connection = openDatabase(file);
      connection.executeSimpleSQL(schema);
    }
  }
  return connection;
}

function openDatabase(file) {
  /* If the pointed-at file doesn't already exist, it means the database
   * has never been initialized */
  var connection = null;
  var storSvc = (Cc["@mozilla.org/storage/service;1"]
                 .getService(Ci.mozIStorageService));
  try {
    connection = storSvc.openDatabase(file);
  } catch (e) {
    Cu.reportError(
      "Opening database failed. It may not have been initialized.");
  }
  return connection;
}

function createTable(connection, tableName, schema) {
  if (!connection.tableExists(tableName))
    try {
      connection.executeSimpleSQL(schema);
    } catch (e) {
      Cu.reportInfo(
        tableName + " database table appears to be corrupt. Resetting it.");
      let file = connection.databaseFile;
      // remove corrupt database table
      file.exists() && file.remove(false);
      connection = openDatabase(file);
      connection.executeSimpleSQL(schema);
    }
  return connection;
}
