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
