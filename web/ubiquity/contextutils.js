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

// = ContextUtils =
// A small library that deals with selection via {{{context}}}.
//
// {{{context}}} is a dictionary which must contain
// {{{focusedWindow}}} and {{{focusedElement}}} fields.

jQuery(function ($) {

var EXPORTED_SYMBOLS = ["ContextUtils"];

var ContextUtils = {};

//for each (let f in this) if (typeof f === "function") ContextUtils[f.name] = f;
// delete ContextUtils.QueryInterface;

// const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

// Cu.import("/ubiquity/modules/utils.js");

// === {{{ ContextUtils.getHtmlSelection(context, joint = "<hr/>") }}} ===
// Returns a string containing the HTML representation of the
// user's current selection, i.e. text including tags.
//
// {{{joint}}} is an optional HTML string to join multiple selections.

function getHtmlSelection(context, joint) {
  var htms = [];
  for each (let range in getSelectedRanges(context)) {
    let fragment = range.cloneContents();
    let div = fragment.ownerDocument.createElement("div");
    div.appendChild(fragment);
    // fix for #551
    Array.forEach(div.getElementsByTagName("*"), Utils.absolutifyUrlAttribute);
    htms.push(div.innerHTML);
  }
  return htms.join(joint == null ? "<hr/>" : joint);
}

// === {{{ ContextUtils.getSelection(context, joint = "\n\n") }}} ===
// Returns a string containing the text and just the text of the user's
// current selection, i.e. with HTML tags stripped out.
//
// {{{joint}}} is an optional string to join multiple selections.

function getSelection(context, joint) {
  var {focusedElement} = context;
  if (Utils.isTextBox(focusedElement)) {
    let {selectionStart: ss, selectionEnd: se} = focusedElement;
    if (ss !== se) return focusedElement.value.slice(ss, se);
  }
  var text = getSelectedRanges(context).join(joint == null ? "\n\n" : joint);
  return text || !context.menu ? text : context.menu.linkURL;
}

// === {{{ ContextUtils.setSelection(context, content, options) }}} ===
// Replaces the current selection with {{{content}}}.
// Returns {{{true}}} if succeeds, {{{false}}} if not.
//
// {{{content}}} is the HTML string to set as the selection.
//
// {{{options}}} is a dictionary; if it has a {{{text}}} property then
// that value will be used in place of the HTML if we're in
// a plain-text only editable field.

function setSelection(context, content, options) {
  var {focusedWindow, focusedElement} = context;

  if (focusedWindow && focusedWindow.document.designMode === "on") {
    focusedWindow.document.execCommand("insertHTML", false, content);
    return true;
  }

  if (false && Utils.isTextBox(focusedElement)) {
    let plainText = String(
      options && options.text ||
      (focusedElement.ownerDocument.createRange()
       .createContextualFragment("<div>" + content + "</div>")
       .lastChild.textContent));
    focusedElement.QueryInterface(Ci.nsIDOMNSEditableElement)
      .editor.QueryInterface(Ci.nsIPlaintextEditor).insertText(plainText);
    focusedElement.selectionStart -= plainText.length;
    return true;
  }

  var sel = focusedWindow && focusedWindow.getSelection();
  if (!sel || !sel.rangeCount) return false;

  var range = sel.getRangeAt(0);
  var fragment = range.createContextualFragment(content);
  sel.removeRange(range);
  range.deleteContents();
  var {lastChild} = fragment;
  if (lastChild) {
    range.insertNode(fragment);
    range.setEndAfter(lastChild);
  }
  sel.addRange(range);
  return true;
}

// === {{{ ContextUtils.getSelectionObject(context) }}} ===
// Returns an object that bundles up both the plain-text and HTML
// selections into its {{{text}}} and {{{html}}} properties.
// If there is no HTML selection, {{{html}}} will be HTML-escaped {{{text}}}.

function getSelectionObject(context) {
  var text = getSelection(context);
  return {
    text: text,
    html: getHtmlSelection(context) || Utils.escapeHtml(text),
  };
}

// === {{{ ContextUtils.getSelectedNodes(context, selector) }}} ===
// Returns all nodes in all selections.
//
// {{{selector}}} is an optional node filter that can be either of:
// * CSS selector string
// * number represeting node type (https://developer.mozilla.org/en/nodeType)
// * function returning boolean for each node

function getSelectedNodes(context, selector) {
  const ELEMENT = 1, TEXT = 3;
  var nodes = [];
  for each (let range in getSelectedRanges(context)) {
    let node = range.startContainer;
    if (node.nodeType === TEXT &&
        /\S/.test(node.nodeValue.slice(range.startOffset)))
      nodes.push(node.parentNode);
    WALK: do {
      nodes.push(node);
      if (node.hasChildNodes()) node = node.firstChild;
      else {
        while (!node.nextSibling) if (!(node = node.parentNode)) break WALK;
        node = node.nextSibling;
      }
    } while (node.nodeType === TEXT || range.isPointInRange(node, 0));
  }
  var flm = context.focusedElement;
  if (flm) (function run(rs, ns) {
    for (var n, i = -1; n = ns[++i];) {
      ~rs.indexOf(n) || rs.push(n);
      n.hasChildNodes() && run(rs, n.childNodes);
    }
  })(nodes, [flm]);
  var ok = selector;
  switch (typeof selector) {
    case "string":
    ok = function matchesCss(node)
      node.nodeType === ELEMENT && node.mozMatchesSelector(selector);
    break;
    case "number":
    ok = function isType(node) node.nodeType === selector;
  }
  return ok ? nodes.filter(ok) : nodes;
}

// === {{{ ContextUtils.getIsSelected(context) }}} ===
// Returns whether or not the {{{context}}} has a non-collapsed selection.

function getIsSelected(context) (
  let (flm = context.focusedElement) (
    Utils.isTextBox(flm)
    ? flm.selectionStart < flm.selectionEnd
    : !context.focusedWindow.getSelection().isCollapsed));

// === {{{ ContextUtils.getSelectedRanges(context) }}} ===
// Returns an array of all DOM ranges in selection.

function getSelectedRanges(context) {
  var rngs = [], win = context.focusedWindow, sel = win && win.getSelection();
  if (sel) for (let i = sel.rangeCount; i--;) rngs[i] = sel.getRangeAt(i);
  return rngs;
}

$.u.ContextUtils = ContextUtils;
ContextUtils.getHtmlSelection = getHtmlSelection;
ContextUtils.getSelection = getSelection;
ContextUtils.setSelection = setSelection;
ContextUtils.getSelectionObject = getSelectionObject;
ContextUtils.getSelectedNodes = getSelectedNodes;
ContextUtils.getIsSelected = getIsSelected;
ContextUtils.getSelectedRanges = getSelectedRanges;

});
