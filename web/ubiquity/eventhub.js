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

// = EventHub =
//
// This is a simple class that can be used to provide an Observer pattern
// to any functionality that needs it.

var EXPORTED_SYMBOLS = ['EventHub'];

// == The EventHub Class ==
//
// Creating an instance of this class takes no arguments.

function EventHub() {
  let self = this;
  let listeners = {};

  function ensureEventName(eventName) {
    if (!(eventName in listeners))
      listeners[eventName] = [];
  };

  // === {{{EventHub#addListener()}}} ===
  //
  // Adds a listener which will be called back whenever an event
  // of the given name occurs.
  self.addListener = function addListener(eventName, listener) {
    ensureEventName(eventName);

    if (listeners[eventName].indexOf(listener) != -1)
      throw new Error('Listener already registered for event "' + eventName +
                      '"');
    listeners[eventName].push(listener);
  };

  // === {{{EventHub#removeListener()}}} ===
  //
  // Removes the given listener from being called back whenever the
  // event of the given name occurs.
  self.removeListener = function removeListener(eventName, listener) {
    ensureEventName(eventName);

    let index = listeners[eventName].indexOf(listener);
    if (index == -1)
      throw new Error('Listener not registered for event "' + eventName +
                      '"');
    listeners[eventName].splice(index, 1);
  };

  // === {{{EventHub#notifyListeners()}}} ===
  //
  // Calls all registered listeners of the given event name, passing
  // them the event name and the given data as arguments.
  self.notifyListeners = function notifyListeners(eventName, data) {
    ensureEventName(eventName);

    function notify(listener) {
      listener(eventName, data);
    }

    var listenersCopy = listeners[eventName].slice();
    listenersCopy.forEach(notify);
  };

  // === {{{EventHub#attachMethods()}}} ===
  //
  // This function attaches its {{{addListener()}}} and
  // {{{removeListener()}}} methods to the given object. It's useful
  // as a way of dynamically adding observability to any object: the
  // object keeps a privately-accessible {{{EventHub}}} instance so
  // that it can notify listeners, and listeners can use the object's
  // public methods to register and unregister themselves.

  self.attachMethods = function attachMethods(target) {
    target.addListener = self.addListener;
    target.removeListener = self.removeListener;
  };
}
