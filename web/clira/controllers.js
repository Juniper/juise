/*
 * $Id$
 *  -*-  indent-tabs-mode:nil -*-
 * Copyright 2013, Juniper Networks Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */


/*
 * Controller for the command AutoComplete view.
 */
Clira.CommandInputController = Em.ObjectController.extend({
    // Need access to outputs controller to insert output after command run
    needs: ['outputs'],
    command: null,

    // This serves as 'source' that returns autocomplete list
    autoComplete: function (value, response) {
        $.dbgpr("test: input: [" + value.term + "]");
        var parse = $.clira.parse(value.term);
        $.dbgpr("parse: " + parse.possibilities.length);

        var res = [ ];

        parse.eachPossibility(function (n, p) {
            p.render();
            if (p.text) {
                var r = {
                    label: p.text,
                    value: p.text,
                    html:  p.html,
                    help: p.command.help,
                    image: p.command.image,
                    image_class: p.command.image_class
                }

                res.push(r);

                // If the command defines a custom completion, use it
                if (false && p.command.complete) {
                    $.dbgpr("calling custom completion");
                    p.command.complete(p, res, value);
                }

            }
        });
        response(res);
    },

    // Runs the command and appends output to outputs controller
    executeCommand: function() {
        if (!this.command)
            return;

        $.dbgpr("execute: input: [" + this.command + "]");
        var parse = $.clira.parse(this.command);
        $.dbgpr("parse: " + parse.possibilities.length);

        var poss = parse.possibilities[0];

        var parseErrors = this._emitParseErrors(parse, poss);

        // Template for outputContent child defaults to 'output_content'
        var templateName = "output_content",
            finalCommand = this.command,
            output = null;
        
        if (parseErrors.length == 0) {
            if (poss.command.templateName)
                templateName = poss.command.templateName;

            if (poss.command.command.length > finalCommand.length)
                finalCommand = poss.command.command;

            // Save command into history
            var history = Clira.CommandHistory.create({
                command: $.trim(finalCommand),
                on: new Date().getTime()
            });
            history.saveRecord();

            // Set parseErrors to null so we don't render it
            parseErrors = null;
        }

        var content = {
            contentTemplate: templateName,
            command: finalCommand,
            messages: parseErrors,
            output: output,
            parse: parse,
            parseErrors: parseErrors,
            poss: poss
        };

        /*
         * Create an instance of OutputContainerController with the output and
         * template data from the command run and prepend it to the 
         * OutputsController.
         */
        this.get('controllers.outputs')
            .unshiftObject(Clira.OutputContainerController.create(content));

        // Reset command input field 
        this.set('command', '');
    },

    // Parses the possibilities and returns an array containing error messages
    _emitParseErrors: function(parse, poss) {
        var messages = [];

        if (poss == undefined) {
            messages.push({message: "unknown command", type: "error"});
            return messages;
        }

        $.each(poss.command.arguments, function (n, arg) {
            var message;

            if (arg.mandatory && !poss.seen[arg.name]) {
                message = "Missing mandatory argument: " + arg.name;
            } else if ($.clira.types[arg.type].needs_data
                    && poss.seen[arg.name] && !poss.data[arg.name]) {
                message = "Missing argument value: " + arg.name;
            }

            if (message) {
                messages.push({message: message, type: "error"});
            }
        });

        return messages;
    }
});


/*
 * OutputsController is an ArrayController that holds outputs from commands.
 * For each command run, we create an array item of type OutputContainer and
 * add OutputsController.
 */
Clira.OutputsController = Em.ArrayController.extend();


/*
 * Controller for the OutputContainerView. Container view gets output data and
 * template name for output content child view from here
 */
Clira.OutputContainerController = Em.Controller.extend({
    data: null,
    contentTemplate: null,

    init: function() {
        // Set template name to be used for output content
        this.contentTemplate = this.get('contentTemplate');
    }
});


/*
 * Controller for pulldown icon to toggle the visibility of pulldown view
 */
Clira.PulldownController = Em.ObjectController.extend({
    needs: ['mruPulldown'],

    toggleMruPulldown: function() {
        this.get('controllers.mruPulldown').toggleProperty('visible');
    }
});


/*
 * Controller for mru pulldown view
 */
Clira.MruPulldownController = Em.ArrayController.extend({
    needs: ['commandInput'],
    visible: false,

    // Get the history from CommandHistory model and set it to content on init
    init: function() {
        this.set('content', Clira.CommandHistory.find());
        this.get('controllers.commandInput.command');
    },
   
    // Most recently used command list as a computed property on content
    mru: function() {
        var ru = Em.A(),
            command;
        
        // Iterate over command history and maintain mru list
        this.content.uniq().forEach(function(item) {
            command = item.get('command');
            if (command) {
                /*
                 * If we are seeing this the first time, insert it into the 
                 * array, otherwise pull it to the top
                 */
                if (ru.contains(command)) {
                    ru.removeObject(command);
                }
                ru.insertAt(0, command);
            }
        });
        return ru;
    }.property('content'),

    /*
     * Observe command input field for changes and update content which will 
     * then update computed property mru
     */
    updateMru: function() {
        this.set('content', Clira.CommandHistory.find());
    }.observes('controllers.commandInput.command')
 });


/*
 * Controller to order commands saved in CommandHistory model
 */
Clira.CommandHistoryController = Em.ArrayController.extend({
    sortAscending: false,
    sortProperties: ['on']
});
