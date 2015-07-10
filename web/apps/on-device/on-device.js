/*
 * $Id$
 *  -*-  indent-tabs-mode:nil -*-
 * Copyright 2013, Juniper Network Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

jQuery(function($) {
    /*
     * Some commands may need to use different output formats or format the
     * returned output. We maintain a list of commands that need these
     * exceptions. Before executing a command, we match it against these
     * exceptions and use specified format or use onReplyRender/
     * onCompleteRender functions before rendering output to the container
     */
    var exceptions = [
        {
            command: 'ping',
            format: 'xml',
            stream: true,
            maxHeight: 150,
            stop: true,
            stopAction: function(muxer, muxid) {
                /* RPC sync will end streaming command */
                muxer.sendData("]]>]]>", muxid);
            },
            onReplyRender: function(data) {
                return '<pre>' + data.replace(/\n/g, '<br>') + '</pre>';
            },
            onCompleteRender: function(data) {
                return '<pre>' + data.replace(/\n/g, '<br>') + '</pre>';
            }
        },
        {
            command: 'monitor',
            format: 'xml',
            stream: true,
            stop: true,
            maxHeight: 300,
            stopAction: function(muxer, muxid) {
                /* RPC sync will end streaming command */
                muxer.sendData("]]>]]>", muxid);
            },
            onReplyRender: function(data) {
                return '<pre>' + data.replace(/\n/g, '<br>') + '</pre>';
            },
            onCompleteRender: function(data) {
                return '<pre>' + data.replace(/\n/g, '<br>') + '</pre>';
            }
        },
        {
            command: 'traceroute',
            format: 'xml',
            stream: true,
            stop: true,
            maxHeight: 150,
            stopAction: function(muxer, muxid) {
                /* RPC sync will end streaming command */
                muxer.sendData("]]>]]>", muxid);
            },
            onReplyRender: function(data) {
                return '<pre>' + data.replace(/\n/g, '<br>') + '</pre>';
            },
            onCompleteRender: function(data) {
                return '<pre>' + data.replace(/\n/g, '<br>') + '</pre>';
            }
        }

    ];

    jQuery.clira.commandFile({
        name: "on-device",
        commands: [
            {
                command: "on",
                arguments: [
                    {
                        name: "target",
                        type: "string",
                        help: "Remote device name",
                        nokeyword: true
                    },
                    {
                        name: "command",
                        type: "string",
                        multiple_words: true,
                        help: "Command to execute",
                        nokeyword: true
                    }
                ],
                onOutputChange: function(view, cmd, parse, poss) {
                    setTimeout(function() {
                        view.$().find('[class~="data"]').each(function() {
                            var help = $(this).attr('data-help'),
                                type = $(this).attr('data-type'),
                                xpath = $(this).attr('data-xpath'),
                                tag = $(this).attr('data-tag'),
                                output = "<div>";
                            if (help) {
                                output += "<b>Help</b>: " + help  + "<br/>";
                            }
                            if (type) {
                                output += "<b>Type</b>: " + type  + "<br/>";
                            }
                            if (xpath) {
                                output += "<div class='xpath-wrapper'>"
                                       + "<a class='xpath-link' href='#'>"
                                       + "show xpath</a><div class='xpath'>" 
                                       + xpath + "</div></div><br/>";
                            }
                            output += "</div>";
                            $(this).qtip({
                                content: {
                                    title: "<b>" + tag + ":</b>",
                                    text: function () {
                                        var div = $(output);
                                        div.find(".xpath-link")
                                           .click(function() {
                                            var xpath = $(this).next();
                                            if (xpath.is(":hidden")) {
                                                xpath.show();
                                                $(this).text("hide xpath");
                                            } else {
                                                xpath.hide();
                                                $(this).text("show xpath");
                                            }
                                            return false;
                                        });
                                        return div;
                                    }
                                },
                                hide: {
                                    fixed: true,
                                    delay: 300
                                },
                                style: "qtip-tipped"
                            });
                        });
                    }, 0);
                },
                execute: function (view, cmd, parse, poss) {
                    parse.dbgpr("working command: " + poss.command.command);
                    view.get('controller').set('output', "Running.... ");
                    
                    var cname = poss.data.target;
                    // cname.replace("_", "__", "g"); // Maybe not needed?
                    // classnames can't have periods
                    cname = cname.replace(".", "_", "g");

                    var command = poss.data.command
                                      .replace(/^\s+|\s+$/g,'');
                    // If user wants a form, build and display form
                    if (command.indexOf('form', command.length - 4) !== -1) {
                        command = command.substring(0, 
                                    command.lastIndexOf(' '));
                        buildForm(view, command, poss);
                    } else {
                        executeInternal(view, poss.data.target,
                                                poss.data.command);
                    }
                },
                complete: function (controller, poss, results, value) {
                    if (!(poss.data.target && poss.data.command))
                        return;

                    addCompletions(controller, poss, poss.data.target, 
                                   poss.data.command, results, value.term);

                    return 500; // Need a delay/timeout
                }
            }
        ]
    });
    
    function executeInternal(view, target, command) {
        var ex = null;
        // Check if we are exception
        $.each(exceptions, function(k, v) {
            if (command.indexOf(v.command) == 0) {
                ex = v;
            }
        });

        if (ex) {
            if (ex.maxHeight) {
                view.$().css("height", ex.maxHeight);
                view.$().css("overflow-y", "auto");
            }

            $.clira.runCommandInternal(view, target, command, 
                                                    ex.format, onComplete, 
                                                    ex.stream, ex.onReplyRender,
                                                    ex.onCompleteRender);

            if (ex.stop && ex.stopAction) {
                view.get('controller').set('stopButton', true);
                view.set('controller._actions.stopAction', ex.stopAction);
            }

        } else {
            $.clira.runCommandInternal(view, target, command, 'html', 
                                       onComplete, false);
        }
    }

    function onComplete(view, success, output) {
        if (success) {
            // If we have error running RPC, catch and let the user know
            var $xmlDoc = $($.parseXML(output));
                
            if ($xmlDoc.find('rpc-error').length > 0) {
                view.set('controller.output', 'Failed to run command');
            }
        }
    }

    function addCompletions(controller, poss, target, command, results, raw) {
        var completion = "";
        var space = false;
        if (raw.slice(-1) == ' ') {
            space = true;
        }

        // Command completion on box on ?
        if (command.slice(-2) == " ?" 
            && (command.split('"').length - 1) % 2 == 0) {
            command = command.slice(0, -1);

            // Remove ? from the command input box
            controller.set('command', controller.get('command').slice(0, -1));

            //Remove command with ? from autocomplete list
            results.pop();
        }
        var payload = "<command expand='expand'>" + command + (space ? ' ' : '') + "?</command>";
        $.dbgpr("on-device: rpc [" + payload + "]");

        $.clira.muxer().rpc({
            create: "no",
            target: target,
            payload: payload,
            onreply: function (data) {
                completion += data;
            },
            oncomplete: function () {
                var $xmlDoc = $($.parseXML(completion));
                $xmlDoc.find("expand-item").each(function (n, item) {
                    var $this = $(this);
                    var complete = {
                        command: command,
                        name: $this.find("name").text(),
                        help: $this.find("help").text(),
                        expandable: $this.find("expandable").text(),
                        enter: $this.find("enter").text(),
                        data: $this.find("data").text()
                    };

                    var cl = buildCommandLine(command, poss, complete);

                    var r = {
                        label: cl.text,
                        value: cl.text,
                        html:  cl.html,
                        help: complete.help,
                        image: poss.command.image,
                        image_class: poss.command.image_class,
                        complete: complete
                    }

                    var scores = {
                        complete: 10,
                        enter: 5
                    }

                    r.score = scores.complete;
                    if (r.complete.enter)
                        r.score += scores.enter;

                    results.push(r);
                    $.dbgpr("on-device.addCompletions: " + complete.name);
                });

                $.dbgpr("on-device.addCompletions done: " + results.length);
            },
            onhostkey: function (data) {
                $.dbgpr("on-device: onhostkey (fails)");
            },
            onpsphrase: function (data) {
            },
            onpsword: function (data) {
            },
            onclose: function (event, message) {
            },
            onerror: function (message) {
                $.dbgpr("on-device: onerror (fails)");
            }
        });
    }

    function buildCommandLine (command, poss, complete) {
        var leader = "on target " + poss.data.target + " command ";
        var li = command.lastIndexOf(" ");
        if (li > 1)
            leader += command.substring(0, li + 1);
        var cl = leader + complete.name;

        var res = {
            text: cl + " ",
            html: "<div class='command-line command-line-on'>"
                + cl + "</div>"
        }
        return res;
    }


    function buildForm (view, command, poss) {
        var buttons =  [{
            caption: "Execute",
            validate: true,
            onclick: function() {
                var values = this.get('controller.fieldValues'),
                    fields = this.get('controller.fields');
                var morecommand = "";

                if (this.get('errorCount') > 0) {
                    return false;
                }

                for (var i = 0; i < fields.length; i++) {
                    if (fields[i].spacer) {
                        continue;    
                    }
                    if (values.hasOwnProperty(fields[i].name) 
                            && values[fields[i].name]) {
                        if (fields[i].nokeyword)  {
                            if (fields[i].boolean) {
                                morecommand += " " + fields[i].name;
                            } else {
                                morecommand += " " 
                                    + values[fields[i].name];
                            }
                        } else if (fields[i].boolean) {
                            morecommand += " " + fields[i].name;
                        } else {
                            morecommand += " " + fields[i].name 
                                + " " + values[fields[i].name];
                        }
                    }
                }
                this.get('parentView').get('parentView').destroy();

                // Update output header
                this.set('controller.command', 
                         'on ' + poss.data.target + ' ' 
                         + command + morecommand);

                executeInternal(view, poss.data.target, 
                                command + morecommand);
             }
        }, {
        caption: "Reset",
            onclick: function() {
                var fields = this.get('controller.fields');
                fields.forEach(function(field) {
                    Ember.set(field, 'value', null);
                });
            }
       }];
       $.clira.buildAutoForm(poss.data.target, command, view, buttons);
   }
});
