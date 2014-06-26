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
                        })
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
                        $.clira.runCommand(view, poss.data.target,
                                            poss.data.command);
                    }
                },
                complete: function (controller, poss, results, value) {
                    if (!(poss.data.target && poss.data.command))
                        return;

                    addCompletions(controller, poss, poss.data.target, 
                                   poss.data.command, results);

                    return 200; // Need a delay/timeout
                }
            }
        ]
    });

    function addCompletions(controller, poss, target, command, results) {
        var completion = "";
        // Command completion on box on ?
        if (command.slice(-2) == " ?" 
            && (command.split('"').length - 1) % 2 == 0) {
            command = command.slice(0, -1);

            // Remove ? from the command input box
            controller.set('command', controller.get('command').slice(0, -1));

            //Remove command with ? from autocomplete list
            results.pop();
        }
        var payload = "<command expand='expand'>" + command + "?</command>";
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
        var nodeinfo = "";
        var muxer = $.clira.muxer();

        // Execute complete RPC and get available options
        muxer.rpc({
            div: view.$(),
            view: view,
            target: poss.data.target,
            payload: '<complete>'
                    + command + ' ?'
                    + '</complete>',
            onreply: function (data) {
                nodeinfo += data;
            },
            oncomplete: function () {
                var $xmlDoc = $($.parseXML(nodeinfo)),
                    dataValues = [],
                    nokeyItem = null,
                    selects = {};

                if (true) {
                    var fields = [];
                    $xmlDoc.find("expand-item").each(
                        function (n, item) {
                            var $this = $(this),
                                type = $this.find('type').text(),
                                enter = $this.find('enter').length,
                                data = $this.find('data').length,
                                nokeyword =
                                $this.find('flag-nokeyword').length, 
                                help = $this.find('help').text(),
                                mandatory =
                                $this.find('flag-mandatory').length,
                                hidden = $this.find('hidden').length,
                                name = $this.find('name').text();

                            if (hidden > 0) {
                                return;
                            }

                            if (mandatory > 0) {
                                name = name + ' (*)';
                            }

                            if (type !== 'TYPE_COMMAND' 
                                && type !== 'TYPE_CHOICE' && enter == 0 
                                && name !== '|' && data == 0 
                                && (type === 'TYPE_TOGGLE' || nokeyword == 0)) {
                                var field = {
                                    name: name,
                                    title: $this.find('name').text(),
                                    type: $this.find('type').text(),
                                    nokeyword: nokeyword,
                                    boolean: $this.find('type')
                                                  .text() == "TYPE_TOGGLE"
                                };

                                if (help.length > 0) {
                                    field['help'] = help;
                                } else {
                                    field['help'] = name;
                                }

                                fields.unshift(field);
                            }

                            if (data > 0) {
                                dataValues.push(name);
                            }

                            if (type == 'TYPE_CHOICE') {
                                var parent = $this.find('parent').text();
                                if (selects[parent]) {
                                    selects[parent].push(name);
                                } else {
                                    selects[parent] = [name];
                                }
                            }

                            if (nokeyword > 0 && data == 0) {
                                nokeyItem = $this;
                            }
                        }
                    );

                    // Iterate through list of selects
                    $.each(selects, function(k, v) {
                        fields.unshift({
                            name: k,
                            nokeyword: true,
                            help: k,
                            title: k,
                            radio: true,
                            data: v
                        });
                    });

                    if (nokeyItem) {
                        var mandatory = nokeyItem.find('flag-mandatory')
                                                 .length;
                        var field = {
                            title: nokeyItem.find('name').text(),
                            help:  nokeyItem.find('help').text() ?
                            nokeyItem.find('help').text() :
                            nokeyItem.find('name').text(),
                            name: nokeyItem.find('name').text(),
                            boolean: nokeyItem.find('type')
                                              .text() == "TYPE_TOGGLE",
                            mandatory: mandatory,
                            nokeyword: true,
                        };

                        if (selects && dataValues.length > 0) {
                            dataValues.unshift('');
                            field['select'] = true;
                            field['data'] = dataValues;
                        }

                        fields.unshift(field);
                    }

                    var v = view.get('parentView').container
                                .lookup('view:DynForm');
                    v.fields = fields;
                    v.buttons = [{
                        caption: "Execute",
                        onclick: function() {
                            var values = this.get('controller.fieldValues');
                            var morecommand = "";
                            $.each(values, function(k, v) {
                                var field = null;
                                for (var i = 0; i < fields.length; i++) {
                                    if (fields[i].name == k) {
                                        field = fields[i];
                                        break;
                                    }
                                }
                                if (field) {
                                     if (field.nokeyword) {
                                        if (field.boolean) {
                                            morecommand += " " + k;
                                        } else {
                                            morecommand += " " + v;
                                        }
                                    } else if (field.boolean) {
                                        morecommand += " " + k;
                                    } else {
                                        morecommand += " " + k + " " + v;
                                    }
                                }
                            });
                            this.get('parentView')
                                .get('parentView').destroy();

                            // Update output header
                            this.set('controller.command', 
                                        'on ' + poss.data.target + ' ' 
                                              + command + morecommand);

                            $.clira.runCommand(view, poss.data.target,
                                            command + morecommand);
                        }
                    }];
                
                    view.get('parentView').pushObject(v);
                }
            }
        });
    }
});
