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
                execute: function ($output, cmd, parse, poss) {
                    parse.dbgpr("working command: " + poss.command.command);
                    $output.html("<div>Running.... </div>");
                    
                    var cname = poss.data.target;
                    // cname.replace("_", "__", "g"); // Maybe not needed?
                    // classnames can't have periods
                    cname = cname.replace(".", "_", "g");
                    
                    $.clira.targetListMarkUsed(poss.data.target, cname,
                        function ($target, target) {
                            $.clira.cmdHistory.select("on " + target + " ");
                        });
                    $.clira.runCommand($output, poss.data.target,
                                       poss.data.command);
                },
                complete: function (poss, results, value) {
                    if (!(poss.data.target && poss.data.command))
                        return;

                    addCompletions(poss, poss.data.target, poss.data.command,
                                   results);

                    return 200; // Need a delay/timeout
                }
            }
        ]
    });

    function addCompletions(poss, target, command, results) {
        var completion = "";
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
});
