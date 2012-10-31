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

Components.utils.import("resource://ubiquity/modules/utils.js");
Components.utils.import("resource://ubiquity/modules/setup.js");
Components.utils.import("resource://ubiquity/modules/localization_utils.js");

var L = LocalizationUtils.propertySelector(
  "chrome://ubiquity/locale/devubiquity.properties");

// set up the interface which will control the parser.

var Cc = Components.classes;
var Ci = Components.interfaces;

var displayParse = function(parse, labels) (
  parse._id + ": " + parse.displayHtml
  + (labels && parse._caller ? '<br/>' + L("ubiquity.playpen.caller") + ' ' + parse._caller : '')
  + (labels && parse._combination ? '<br/>' + L("ubiquity.playpen.combo") + ' <code>'
    +parse._combination.replace(/},/g,'},<br/>') + '</code>' : '')
  + (labels ? '<br/>' + L("ubiquity.playpen.argstring") + ' ' + parse.argString : '')
  + (labels ? '<br/>' : ' (')
  + (labels ? L("ubiquity.playpen.score") + ' ' : '')
    + ((parse.score * 100 | 0) / 100 || "<em>" + L("ubiquity.playpen.noscore") + "</em>") + ', '
  + (labels ? L("ubiquity.playpen.multiplier") + ' ' : '')
    + ((parse.scoreMultiplier * 100 | 0) / 100 || "<em>" + L("ubiquity.playpen.noscore") + "</em>")
  + (labels ? '' : ')')
)

var demoParserInterface = {
  startTime: 0,
  endTime: 0,
  runtimes: 0,
  currentLang: UbiquitySetup.languageCode,
  currentParser: null,
  currentQuery: {},
  parse: function() {
    if (this.currentQuery.cancel != undefined)
      if (!this.currentQuery.finished)
        this.currentQuery.cancel();
//    this.currentParser._nounCache = {};

    if ($('#flushcache')[0].checked)
      this.currentParser.flushNounCache();

    $('#parseinfo').empty();
    $('#parsetree').empty();
    this.currentQuery = this.currentParser.newQuery($('.input').val(),{},$('#maxSuggestions').val(),true); // this last true is for dontRunImmediately

    // custom flag to make sure we don't onResults multiple times per query.
    this.currentQuery.resulted = false;

    // override the selection object
    this.currentQuery.selObj = {text: $('#selection').val(),
                                html: $('#selection').val()};

    $('#scoredParses').empty();

    if ($('#displayparseinfo')[0].checked &&
        this.runtimes + 2 > $('.runtimes').text()) {
      this.currentQuery.watch('_step',function(id,oldval,newval) {
        switch (oldval) {
          case 1:
            $('<h3>' + L("ubiquity.playpen.step1") + '</h3><code>'+this._input+'</code>').appendTo($('#parseinfo'));
            break;

          case 2:
            $('<h3>' + L("ubiquity.playpen.step2") + '</h3><ul id="preParses"></ul>').appendTo($('#parseinfo'));
            for each (preParse in this._preParses) {
              $('<li>V: <code title="'+(preParse._verb.id || 'null')+'">'+(preParse._verb.text || '<em>' + L("ubiquity.playpen.null") + '</em>')+'</code>, ' + L("ubiquity.playpen.argstring") + ' <code>'+preParse.argString+'</code>, sel: <code>'+preParse.sel+'</code></li>').appendTo($('#preParses'));
            }
            break;

          case 3:
            $('<h3>' + L("ubiquity.playpen.step3") + '</h3>').appendTo($('#parseinfo'));
            break;

          case 4:
            $('<h3>' + L("ubiquity.playpen.step4") + '</h3><ul id="argParses"></ul>').appendTo($('#parseinfo'));
            for each (var parse in this._possibleParses) {
              $('#argParses').append('<li>' + parse.displayHtml + '</li>');
            }
            $('<p><small>'+this._possibleParses.length+' ' + L("ubiquity.playpen.parsespossible") + '</small></p>').appendTo($('#parseinfo'));
            break;

          case 5:
            $('<h3>' + L("ubiquity.playpen.step5") + '</h3><ul id="newPossibleParses"></ul>').appendTo($('#parseinfo'));
            for each (var parse in this._possibleParses) {
              $('#newPossibleParses')
                .append('<li>' + parse.displayHtml + '</li>');
            }
            $('<p><small>'+this._possibleParses.length+' ' + L("ubiquity.playpen.parsespossible") + '</small></p>').appendTo($('#parseinfo'));
            break;

          case 6:
            $('<h3>' + L("ubiquity.playpen.step6") + '</h3><ul id="normalizedArgParses"></ul>').appendTo($('#parseinfo'));
            for each (var parse in this._possibleParses) {
              $('#normalizedArgParses')
                .append('<li>' + displayParse(parse) + '</li>');
            }
            $('<p><small>'+this._possibleParses.length+' ' + L("ubiquity.playpen.parsespossible") + '</small></p>').appendTo($('#parseinfo'));
            break;

          case 7:
            $('<h3>' + L("ubiquity.playpen.step7") + '</h3><ul id="otherRoleParses"></ul>').appendTo($('#parseinfo'));
            for each (var parse in this._possibleParses) {
              $('#otherRoleParses')
                .append('<li>' + displayParse(parse) + '</li>');
            }
            $('<p><small>'+this._possibleParses.length+' ' + L("ubiquity.playpen.parsespossible") + '</small></p>').appendTo($('#parseinfo'));
            break;

          case 8:
            $('<h3>' + L("ubiquity.playpen.step8") + '</h3><ul id="verbedParses"></ul>').appendTo($('#parseinfo'));
            for each (var parse in this._verbedParses) {
              $('#verbedParses')
                .append('<li>' + displayParse(parse) + '</li>');
            }
            $('<p><small>'+this._verbedParses.length+' ' + L("ubiquity.playpen.parsedverbs") + '</small></p>').appendTo($('#parseinfo'));
            break;

          case 9:
            /*$('<h3>step 7: noun type detection</h3><ul id="nounCache"></ul>').appendTo($('#parseinfo'));
            for (var text in nounCache) {
              var html = $('<li><code>'+text+'</code></li>');
              var list = $('<ul></ul>');
              for each (let suggestion in nounCache[text]) {
                $('<li>type: <code>'+suggestion.nountype.name+'</code>, suggestion: '+suggestion.text+', score: '+suggestion.score+'</li>').appendTo(list);
              }
              list.appendTo(html);
              html.appendTo($('#nounCache'));
            }*/

            $('<h3>' + L("ubiquity.playpen.step9") + '</h3><ul id="debugScoredParses"></ul>').appendTo($('#parseinfo'));
            var allScoredParses = this.aggregateScoredParses();
	          for each (let parse in allScoredParses) {
              $('#debugScoredParses')
                .append('<li>' + displayParse(parse) + '</li>');
            }
            $('<p><small>'+allScoredParses.length+' ' + L("ubiquity.playpen.parsednouns") + '</small></p>').appendTo($('#parseinfo'));
            break;

        }

        return newval;
      });
    }

    if ($('#displayparsetree')[0].checked &&
        this.runtimes + 2 > $('.runtimes').text()) {
      this.currentQuery.watch('_step',function(id,oldval,newval) {
        switch (oldval) {
          //case 3: TODO
          case 4:
          case 5:
          case 6:
          case 7:
            for each (let parse in this._possibleParses)
              if (!parse._survivedStep)
                parse._survivedStep = oldval;
            break;

          case 8:
            for each (let parse in this._verbedParses)
              if (!parse._survivedStep)
                parse._survivedStep = oldval;
            break;

          case 9:
	          for each (let parse in this.aggregateScoredParses())
              if (!parse._survivedStep)
                parse._survivedStep = oldval;
            break;
        }

        return newval;
      });
    }

    this.currentQuery.onResults = function() {
      if (this.finished && !this.resulted) {
        this.resulted = true;
        demoParserInterface.runtimes++;
        $('.current').text(demoParserInterface.runtimes);
        dump(demoParserInterface.runtimes+' ' + L("ubiquity.playpen.done") + '\n');
        if (demoParserInterface.runtimes < $('.runtimes').text())
          demoParserInterface.parse();
        else {
          demoParserInterface.endTime = new Date().getTime();

          dump(L("ubiquity.playpen.duration") + ' '+(demoParserInterface.endTime - demoParserInterface.startTime)+'\n');
          $('.total').text(demoParserInterface.endTime - demoParserInterface.startTime);

          dump(L("ubiquity.playpen.average") + ' '+(demoParserInterface.endTime - demoParserInterface.startTime)/demoParserInterface.runtimes+'\n');
          $('.avg').text(Math.round((demoParserInterface.endTime - demoParserInterface.startTime) * 100/demoParserInterface.runtimes)/100);

          var suggestionList = this.suggestionList;
 	        $('#scoredParses').empty();
          for each (var parse in suggestionList) {

            $('#scoredParses')
              .append('<tr><td>' + displayParse(parse) + '</td></tr>');
          }

        }

        if ($('#displayparsetree')[0].checked &&
            demoParserInterface.runtimes + 2 > $('.runtimes').text()) {
          for each (let parse in demoParserInterface.currentQuery._allParses) {

            let host = $('#parsetree');
            if (parse._parent && $('#wrap'+parse._parent._id).length)
              host = $('#wrap'+parse._parent._id+' > .children');

            let displayStep = (parse._survivedStep || parse._step);
            $("<div class='treewrap"+(parse._step ? ' ' + L("ubiquity.playpen.step") +parse._step : '')+"' id='wrap"+parse._id+"'>"
                +(parse._step ? "<div class='badge"+(parse._survivedStep?' ' + L("ubiquity.playpen.winner"):'')+" badge"+displayStep+"'>"
                  +displayStep
                +"</div>" : '')
                +"<div class='treeleaf' id='leaf"+parse._id+"'>"
                +displayParse(parse,true)+"</div>"
                +"<div class='children'></div>"
              +"</div>").appendTo(host);
          }
        }

      }
    }
    this.currentQuery.run();

  }
}

function getParser(sync) {
  if (sync) {
    var {gUbiquity} = Utils.currentChromeWindow;
    if (gUbiquity) return gUbiquity.cmdManager.__nlParser;
    else $('#gubiquity').show();
  }
  eval(Utils.getLocalUrl("resource://ubiquity/modules/parser/new/"+
                         UbiquitySetup.languageCode + ".js"), "utf-8");
  return makeParser();
}

$(document).ready(function(){
  var [gUSync] = $("#gu-sync").change(function(){ location.reload() });
  var parser = getParser(gUSync.checked);
  parser.setCommandList(UbiquitySetup.createServices()
                        .commandSource.getAllCommands());
  demoParserInterface.currentParser = parser;

  for each (let {role, delimiter} in parser.roles) {
    // may need to switch &quot; to &#34;
    $('<li><code>'+role+'</code>: &quot;'+ delimiter+'&quot;</li>').appendTo($('#roles'));
  }

  for (let id in parser._nounTypes) {
    let nountype = parser._nounTypes[id];
    $('<li><code>'+id+'</code>: {label: <code>'+nountype.label+'</code>, '
      + L("ubiquity.playpen.name") + ' <code>'+nountype.name+'</code>}</li>')
      .appendTo($('#nountypes'));
  }

  for each (let verb in parser._verbList) {
    // skip if disabled
    if (verb.disabled) continue;

    let {names, help, description} = verb;
    let args = $('<ul></ul>');
    for each (let {nountype, role, label} in verb.arguments) {
      $('<li>' + L("ubiquity.playpen.role") + ' <code>'+role+'</code>, ' + L("ubiquity.playpen.nountype") + ' <code>'+nountype.id+'</code></li>').appendTo(args);
    }
    let item = $('<li><strong><code>'+names[0]+'</code></strong></li>');
    if (verb.arguments.length) {
      $(':<br/>').appendTo(item);
      args.appendTo(item);
    }
    item.appendTo($('#verblist'));
  }

  if (UbiquitySetup.parserVersion != 2) {
    $('#parser2').show();
  }

  function run() {
    demoParserInterface.startTime = new Date().getTime();
    $('.runtimes').text($('#times').val());
    demoParserInterface.runtimes = 0;
    demoParserInterface.parse();
  }

  $('.input').keyup(function autoParse(e){
    if ($('#autoparse')[0].checked) {
      var input = $('.input').val();
      if (input && autoParse.lastInput !== (autoParse.lastInput = input))
        run();
    }
    else if (e.keyCode === KeyEvent.DOM_VK_RETURN) run();
  });
  $('#run').click(run);

  //$('#clearnouncache').click(function() { nounCache = []; });

  $('.toggle').click(function(e){$(e.currentTarget).siblings().toggle();});

});
