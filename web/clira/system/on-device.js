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
                                   poss.data.command, results);

                    return 200; // Need a delay/timeout
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
                                                    null, ex.format,
                                                    ex.stream, ex.onReplyRender,
                                                    ex.onCompleteRender);

            if (ex.stop && ex.stopAction) {
                view.get('controller').set('stopButton', true);
                view.set('controller._actions.stopAction', ex.stopAction);
            }

        } else {
            $.clira.runCommandInternal(view, target, command, null, 'html',
                                       false);
        }
    }

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

    function formatLabel (name) {
        var words = ['SNMP', 'ISSU', 'IPv4', 'IPv6', 'IP', 'TE', 'ISO', 'CCC',
                     'MAC', 'TOS', 'TTL'];

        var label = name.replace(/-/g, ' ');
        label = label.charAt(0).toUpperCase() + label.slice(1);

        label = label.split(' ');
        
        var op = '';

        for (var i = 0; i < label.length; i++) {
            for (var j = 0; j < words.length; j++) {
                if (label[i].toUpperCase() == words[j].toUpperCase()) {
                    break;
                }
            }
            if (j == words.length) {
                op += label[i];
            } else {
                op += words[j];
            }

            if (i < label.length - 1) {
                op += ' ';
            }
        }

        return op;
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
                    nokeyItem = null,
                    dataValues = {},
                    noname = 0,
                    fields = [],
                    mandatoryFields = false,
                    firstNokeyItem = null;

                dataValues['_nokeyItem'] = [];

                $xmlDoc.find("expand-item").each(
                    function(n, item) {
                        var $this = $(this),
                            hidden = $this.find('hidden').length,
                            enter = $this.find('enter').length,
                            data = $this.find('data').length
                            type = $this.find('type').text(),
                            match = $this.find('match').text(),
                            matchMessage = $this.find('match-message').text(),
                            rangeMin = $this.find('range-min').text(),
                            rangeMax = $this.find('range-max').text(),
                            name = $this.find('name').text();

                        if (type === 'TYPE_COMMAND' || hidden > 0 || enter > 0
                            || name === '|') {
                            return;
                        }

                        if (!name) {
                            name = '_clira' + noname;
                            noname++;
                        }

                        var label = formatLabel(name);

                        if ($this.find('flag-mandatory').length > 0) {
                            label += ' *';
                            mandatoryFields = true;
                        }

                        // Each field has an errors object that holds error
                        // messages of each type. Following are the class of
                        // errors
                        // mandatory: Field is mandatory
                        // match: Regular expression match fail
                        // type: Type mismatch
                        // rangeMin: Value less than minimum allowed
                        // rangeMax: Value more than maximum allowed
                        if ($this.find('flag-mandatory').length) {
                            errors = {
                                mandatory: name + ' is mandatory'
                            };
                            errorCount = 1;
                        } else {
                            errors = {};
                            errorCount = 0;
                        }

                        var item = {
                            name: name,
                            title: name,
                            fieldType: type,
                            hidden: hidden,
                            label: label,
                            help: $this.find('help').text(),
                            match: match,
                            matchMessage: matchMessage,
                            nokeyword: $this.find('flag-nokeyword').length,
                            mandatory: $this.find('flag-mandatory').length,
                            boolean: type == 'TYPE_TOGGLE',
                            type: type == 'TYPE_TOGGLE' ? 2 : 3,
                            errors: errors,
                            errorCount: errorCount
                        };

                        if (rangeMin) {
                            item['rangeMin'] = rangeMin;
                        }

                        if (rangeMax) {
                            item['rangeMax'] = rangeMax;
                        }

                        if ($this.find('data').length > 0) {
                            var dName = $this.find('data').text();
                            if (dName) {
                                if (dataValuse[dName]) {
                                    dataValues[dName].push(name);
                                } else {
                                    dataValues[dName] = [name];
                                }
                            } else {
                                dataValues['_nokeyItem'].push(name);
                            }
                        } else if (type === 'TYPE_CHOICE' 
                                    && $this.find('parent').length > 0) {
                            var parent = $this.find('parent').text();
                            if (dataValues[parent]) {
                                dataValues[parent].push(name);
                            } else {
                                dataValues[parent] = [name];
                            }
                        } else {
                            fields.unshift(item);
                            if (firstNokeyItem == null && item.nokeyword > 0) {
                                firstNokeyItem = name;
                            }
                        }
                    }
                );

                if (mandatoryFields) {
                    view.get('controller').set('mandatoryFields', true);
                }

                // Assign data to fields
                $.each(fields, function(i, v) {
                    if (v.name == firstNokeyItem) {
                        fields[i].select = true;
                        fields[i].data = dataValues['_nokeyItem'];
                    } else if (dataValues[v.name]) {
                        fields[i].select = true;
                        fields[i].data = dataValues[v.name];
                    } else {
                        fields[i].select = false;
                    }
                });

                // Add missing field names
                $.each(dataValues, function(k, v) {
                    if (k == '_nokeyItem') {
                        return;
                    }

                    for (var i = 0; i < fields.length; i++) {
                        if (fields[i].name == v.name) {
                            break;
                        }
                    }

                    if (i == fields.length) {
                        var item = {                            
                            name: k,
                            nokeyword: true,
                            help: k,
                            title: k,
                            label: formatLabel(k),
                            radio: true,
                            data: v,
                            type: 0
                        };
                        fields.push(item);
                    }
                });

                //Sort fields so they can be rendered properly
                fields.sort(function(a, b) {
                    if (a.mandatory == 1) {
                        return -1;
                    } else if (b.mandatory == 1) {
                        return 1;
                    } else {
                        if (a.boolean && !b.boolean) {
                            return 1;
                        } else if (!a.boolean && b.boolean) {
                            return -1;
                        } else if (a.radio) {
                            return 1;
                        } else if (b.radio) {
                            return -1;
                        } else if (a.label.length > 20 
                            && a.label.length > b.label.length) {
                            return 1;
                        }
                    }
                    return -1;
                });

                var prevType = -1;
                // Group fields together
                fields.forEach(function(field) {
                    if (prevType >= 0 && field.type != prevType) {
                        fields.splice(fields.indexOf(field), 0, 
                                        { spacer: true });
                    }
                    prevType = field.type;
                });

                var v = view.get('parentView').container
                            .lookup('view:DynForm');
                v.fields = fields;
                v.buttons = [{
                    caption: "Execute",
                    validate: true,
                    onclick: function() {
                        var values = this.get('controller.fieldValues');
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

                view.get('parentView').pushObject(v);
            }
        });
    }
});
