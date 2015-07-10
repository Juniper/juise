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
    commandNumber: 0,

    // This serves as 'source' that returns autocomplete list
    autoComplete: function (value, response) {
        $.dbgpr("test: input: [" + value.term + "]");
        var parse = $.clira.parse(value.term);
        $.dbgpr("parse: " + parse.possibilities.length);

        var res = [ ],
            controller = this.controller,
            delay = 0, delay2 = 0;

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
                if (p.command.complete) {
                    $.dbgpr("calling custom completion");
                    delay2 = p.command.complete(controller, p, res, value);
                    if (delay2 > delay)
                        delay = delay2;
                }

            }
        });

        if (delay2 == 0) {
            response(res);
        } else {
            $.dbgpr("autocomplete: delay for " + delay + "ms: " + res.length);
            setTimeout(function() {
                $.dbgpr("autocomplete: now firing: " + res.length);
                response(res);
            }, delay);
        }
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

        if ($.clira.commandCount) {
            this.set('commandNumber', ++$.clira.commandCount);
        } else {
            $.clira.commandCount = 1;
            this.set('commandNumber', 1);
        }

        var content = {
            contentTemplate: templateName,
            command: finalCommand,
            commandNumber: this.get('commandNumber'),
            completed: false,
            context: this,
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
 * Array controller to hold the list of recently used devices
 */
Clira.RecentDevicesController = Em.ArrayController.extend({
    content: Em.A(),
    needs: ['commandInput'],
    actions: {
        'onDevice': function(target) {
            // On click, fill command input
            this.set('controllers.commandInput.command', 'on ' + target + ' ');
        }
    },

    /*
     * Add recently used list of devices to top of the list
     */
    addDevice: function(device) {
        // Add some height to our header to make room for this.
        $('div#input-top').height(90);
        if (this.content.contains(device)) {
            this.content.removeObject(device);
        }
        this.content.insertAt(0, device);
    },
    
    init: function() {
        this.get('controllers.commandInput.command');
    },

    /*
     * Recently used list of devices as a property on content of array
     */
    ru: function() {
        return this.content;
    }.property('@each')
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
    needs: ['recentDevices'],

    // Action functions to handle close and toggle button clicks
    actions: {
        close: function(controller) {
            controller.get('view').$().slideToggle($.clira.prefs.slide_speed, 
                                                    function() {
                Ember.run(function() {
                    controller.get('view').get('parentView').destroy();
                });
            });
        },
        collapse: function(controller) {
            controller.get('view').$().slideToggle($.clira.prefs.slide_speed, 
                                                    function() {
                Ember.run(function() {
                    controller.get('view')
                          .set('isVisible', !controller.get('view.isVisible'));
                });
            });
        },
        commandClick: function() {
            this.set('context.command', this.get('command'));
        },
	stick: function(controller) {
            controller.get('view').$().slideDown($.clira.prefs.slide_speed, 
                                                    function() {
                var ele = $('#output-container-' + controller.commandNumber);

                if (ele.hasClass('sticky')) {
                    ele.removeClass('sticky');
                } else {
                    ele.addClass('sticky');
                }
            });
        }
    },

    init: function() {
        // Set template name to be used for output content
        this.contentTemplate = this.get('contentTemplate');

        // Register recentDevices controller
        this.container = new Em.Container();
        this.container.register('controller:recentDevices', 
                                Clira.RecentDevicesController);
    },

    /*
     * Observe change in output value and call onOutputChange function on the
     * command if defined
     */
    valueDidChange: function() {
        var command = this.get('command'),
            completed = this.get('completed'),
            output = this.get('output'),
            parse = this.get('parse'),
            poss = this.get('poss'),
            view = this.get('view');

        if (poss.command.onOutputChange && view && output && completed) {
            poss.command.onOutputChange.call(null, view, command, parse,
                                            poss);
        }
    }.observes('output')
});
/* recent used list of devices to the top of list
 */


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
        this.get('controllers.commandInput.commandNumber');
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
    }.observes('controllers.commandInput.commandNumber')
 });


/*
 * Controller to order commands saved in CommandHistory model
 */
Clira.CommandHistoryController = Em.ArrayController.extend({
    sortAscending: false,
    sortProperties: ['on']
});
