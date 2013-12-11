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
                    $.clira.runCommand(view, poss.data.target,
                                       poss.data.command);
                },
                complete: function (poss, results, value) {
                    if (!(poss.data.target && poss.data.command))
                        return;

                    if (poss.data.command.length < 2)
                        return;

                    addCompletions(poss.data.target, poss.data.command,
                                   results);
                }
            }
        ]
    });

    function addCompletions(target, command, results) {
        var completion = '';
        $.clira.muxer().rpc({
            target: target,
            payload: '<command expand="expand">' + command + '?</command>',
            onreply: function (data) {
                completion += data;
            },
            oncomplete: function () {
                var $xmlDoc = $($.parseXML(completion));
                $xmlDoc.find("expand-item").each(function() {
                    var complete = {
                        command: command,
                        name: $(this).find("name").text(),
                        help: $(this).find("help").text(),
                        expandable: $(this).find("expandable").text(),
                        enter: $(this).find("enter").text(),
                        data: $(this).find("data").text()
                    };

                    var r = {
                        label: p.text + complete.name,
                        value: p.text + complete.name,
                        html: p.html + complete.name,
                        help: complete.help,
                        image: p.command.image,
                        image_class: p.command.image_class,
                        complete: complete
                    }

                    var scores = {
                        complete: 10,
                        enter: 5
                    }

                    r.score = scores.complete;
                    if (r.complete.enter)
                        r.score += scores.enter;

                    push(r);
                });
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
            }
        });
    }
});
